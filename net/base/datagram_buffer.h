// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DATAGRAM_BUFFER_H_
#define NET_BASE_DATAGRAM_BUFFER_H_

#include <list>

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"

namespace net {

// An IO buffer, (at least initially) specifically for use with the
// new DatagramClientSocket::WriteAsync method, with the following key
// features:
//
//   1) Meant to be easily batched when that improves efficiency. The
//      primary goal of WriteAsync is to enable enlisting an
//      additional cpu core for the kernel part of socket write.
//   2) Uses unique_ptr (with std::move) rather than reference
//      counting as in IOBuffers.  The benefit is safer cancellation
//      semantics, IOBuffer used reference count to enforce unique
//      ownership in an idiomatic fashion.  unique_ptr is ligher weight
//      as it doesn't use thread safe primitives as
//      RefCountedThreadSafe does.
//   3) Provides a pooling allocator, which for datagram buffers is
//      much cheaper than using fully general allocator (e.g. malloc
//      etc.).  The implementation takes advantage of
//      std::list::splice so that costs associated with allocations
//      and copies of pool metadata quickly amortize to zero, and all
//      common operations are O(1).

class DatagramBuffer;

// Batches of DatagramBuffers are treated as a FIFO queue, implemented
// by |std::list|.  Note that |std::list::splice()| is attractive for
// this use case because it keeps most operations to O(1) and
// minimizes allocations/frees and copies.
typedef std::list<std::unique_ptr<DatagramBuffer>> DatagramBuffers;

class NET_EXPORT_PRIVATE DatagramBufferPool {
 public:
  // |max_buffer_size| must be >= largest |buf_len| provided to
  // ||New()|.
  explicit DatagramBufferPool(size_t max_buffer_size);
  DatagramBufferPool(const DatagramBufferPool&) = delete;
  DatagramBufferPool& operator=(const DatagramBufferPool&) = delete;
  virtual ~DatagramBufferPool();
  // Insert a new element (drawn from the pool) containing a copy of
  // |buffer| to |buffers|. Caller retains owenership of |buffers| and |buffer|.
  void Enqueue(const char* buffer, size_t buf_len, DatagramBuffers* buffers);
  // Return all elements of |buffers| to the pool.  Caller retains
  // ownership of |buffers|.
  void Dequeue(DatagramBuffers* buffers);

  size_t max_buffer_size() { return max_buffer_size_; }

 private:
  const size_t max_buffer_size_;
  DatagramBuffers free_list_;
};

// |DatagramBuffer|s can only be created via
// |DatagramBufferPool::Enqueue()|.
//

// |DatagramBuffer|s should be recycled via
// |DatagramBufferPool::Dequeue|.  Care must be taken when a
// |DatagramBuffer| is moved to another thread via
// |PostTask|. |Dequeue| is not expected to be thread-safe, so it
// is preferred to move the |DatagramBuffer|s back to the thread where
// the pool lives (e.g. using |PostTaskAndReturnWithResult|) and
// dequeuing them from there.  In the exception of pathalogical
// cancellation (e.g. due to thread tear-down), the destructor will
// release its memory permanently rather than returning to the pool.
class NET_EXPORT_PRIVATE DatagramBuffer {
 public:
  DatagramBuffer() = delete;
  DatagramBuffer(const DatagramBuffer&) = delete;
  DatagramBuffer& operator=(const DatagramBuffer&) = delete;
  ~DatagramBuffer();

  char* data() const;
  size_t length() const;

 protected:
  explicit DatagramBuffer(size_t max_packet_size);

 private:
  friend class DatagramBufferPool;

  void Set(const char* buffer, size_t buf_len);

  std::unique_ptr<char[]> data_;
  size_t length_ = 0;
};

}  // namespace net

#endif  // NET_BASE_DATAGRAM_BUFFER_H_
