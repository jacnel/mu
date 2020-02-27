#include <stdint.h>
#include <cstring>
#include <memory>

#include "consts.hpp"

/// NOTE: Buffer entries are indexed beginning from 1! Trying to access an entry
/// at index 0 will throw a `std::out_of_range`. Internally, indexing starts at
/// 0. However, the exposed interface assumes indexes starting from 1.

/**
 * A buffer entry has a fixed size of `BUFFER_ENTRY_SIZE` which should always
 * be a multiple of 2.
 *
 * +----------+-----------+-----------+
 * | id       | content   | signature |
 * +----------+-----------+-----------+
 * | uint64_t | uint8_t[] | uint8_t[] |
 * +----------+-----------+-----------+
 *
 **/
class BufferEntry {
 public:
  /**
   * @param start: a reference to the the entry
   * */
  BufferEntry(const volatile uint8_t& start);

  /**
   * @returns: the message id
   **/
  uint64_t id();

  /**
   * @returns: a reference to the content
   **/
  const volatile uint8_t& content();

  /**
   * @returns: a reference to the signature
   **/
  const volatile uint8_t& signature();

  /**
   * @returns: the address of the entry
   **/
  uint64_t addr();

 private:
  const volatile uint8_t& start;
};

/**
 * This buffer overlay is used to access the written messages by remote
 * processes. For every remote process there exist one broadcast buffer.
 **/
class BroadcastBuffer {
 public:
  /**
   * @param start: a reference to the buffer
   * @param buf_size: the buffer size in bytes
   **/
  BroadcastBuffer(volatile uint8_t& start, size_t buf_size);

  /**
   * @param index: index of the entry
   * @returns: the offset in bytes where this entry resides in the buffer
   * @thorws: std::out_of_range
   **/
  uint64_t get_byte_offset(uint64_t index);

  /**
   * @param index: index of the entry
   * @returns: the entry associated with the provided index
   * @thorws: std::out_of_range
   **/
  std::unique_ptr<BufferEntry> get_entry(uint64_t index);

  /**
   * Ideally we should not copy but marshall data directly into the broadcast
   * buffer.
   * This function is only used initially when broadcasting a message to write
   *it into a memory region accessible by the RNIC
   * @param index: index of the entry
   * @param k: message key
   * @param buf: a reference to the buffer to copy into the `content` field of
   *the entry
   * @param len: the length of the buffer to copy
   * @thorws: std::out_of_range
   **/
  std::unique_ptr<BufferEntry> write(uint64_t index, uint64_t k,
                                     volatile uint8_t& buf, size_t len);

 private:
  volatile uint8_t& start;
  size_t buf_size;
  uint64_t num_entries;
};

/**
 * This buffer overlay is used to replay the read values within the
 * `BroadcastBuffer`.
 *
 * The buffer space is split among all processes in the cluster.
 *
 * We don't support writing to this buffer, as performance wise we should prefer
 * to local RDMA write from the Broadcast buffer to this replay buffer.
 **/
class ReplayBufferWriter {
 public:
  /**
   * @param start: a reference to the buffer
   * @param buf_size: the size of the buffer in bytes
   * @param num_proc: the total number of processes in the cluster
   **/
  ReplayBufferWriter(const volatile uint8_t& start, size_t buf_size,
                     int num_proc);

  /**
   * @param proc_id: the process id
   * @param index: index of the entry
   **/
  uint64_t get_byte_offset(size_t proc_id, uint64_t index);

  /**
   * @param proc_id: the process id
   * @param index: index of the entry
   **/
  std::unique_ptr<BufferEntry> get_entry(size_t proc_id, uint64_t index);

 private:
  const volatile uint8_t& start;
  size_t buf_size;
  uint64_t num_proc;
  uint64_t num_entries_per_proc;
};

/**
 * This buffer overlay is for reading gathered remote replay entries.
 *
 * The buffer space is split among all processes in the cluster. Additionally,
 * for every index there is a slot for every process to store the replayed
 * values.
 *
 * We don't support any direct write opertaions since the RNIC will write to
 * this buffer.
 **/
class ReplayBufferReader {
 public:
  /**
   * @param start: a reference to the buffer
   * @param buf_size: the size of the buffer in bytes
   * @param num_proc: the total number of processes in the cluster
   **/
  ReplayBufferReader(const volatile uint8_t& start, size_t buf_size,
                     int num_proc);

  /**
   * @param origin_id: the id of the process who's value is replayed
   * @param replayer_id: the id of the process who replayed the value
   * @param index: the index of the entry
   **/
  uint64_t get_byte_offset(size_t origin_id, size_t replayer_id,
                           uint64_t index);

  /**
   * @param origin_id: the id of the process who's value is replayed
   * @param replayer_id: the id of the process who replayed the value
   * @param index: the index of the entry
   **/
  std::unique_ptr<BufferEntry> get_entry(size_t origin_id, size_t replayer_id,
                                         uint64_t index);

 private:
  const volatile uint8_t& start;
  size_t buf_size;
  uint64_t num_proc;
  uint64_t num_entries_per_proc;
};
