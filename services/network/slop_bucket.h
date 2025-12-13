// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SlopBucket is a mechanism to store parts of response bodies in temporary
// buffers in the network service when the mojo data pipe is full.

#ifndef SERVICES_NETWORK_SLOP_BUCKET_H_
#define SERVICES_NETWORK_SLOP_BUCKET_H_

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

namespace net {
class URLRequest;
}

namespace network {

// Enable the use of extra memory to cache response body data when the render
// process is slow to read from the mojo data pipe.
BASE_DECLARE_FEATURE(kSlopBucket);

// SlopBucket is the interface used by URLLoader to request and use extra
// temporary storage for response bodies.
class SlopBucket final {
 public:
  using PassKey = base::PassKey<SlopBucket>;

  // Chunk allocation errors for UMA.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ChunkAllocationFailedReason)
  enum ChunkAllocationFailedReason {
    kOK = 0,
    kDisabled = 1,
    kMaxChunksPerRequest = 2,
    kMaxChunksGlobal = 3,
    kMaxValue = kMaxChunksGlobal
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:SlopBucketChunkAllocationFailedReason)

  // Returns a SlopBucket object for the request to use if it is eligible. The
  // returned SlopBucket object must be destroyed before `for_request`.
  static std::unique_ptr<SlopBucket> RequestSlopBucket(
      net::URLRequest* for_request);

  SlopBucket(PassKey, net::URLRequest* request);
  ~SlopBucket();

  SlopBucket(const SlopBucket&) = delete;
  SlopBucket& operator=(const SlopBucket&) = delete;

  // Attempts to Read() from `request_` into the bucket. Returns the return
  // value from Read() if a Read() was attempted. If the return value was
  // ERR_IO_PENDING, then OnReadCompleted() should be called later when the
  // URLRequest calls OnReadCompleted() on its Delegate. If the return value is
  // std::nullopt then there was no bucket available to read into, and Read()
  // was *not* attempted. Should not be called if read_in_progress() or
  // IsComplete() are true.
  std::optional<int> AttemptRead();

  // When a read completes and read_in_progress() is true, the SlopBucket object
  // must be informed by passing the value of `bytes_read` to this function.
  void OnReadCompleted(int bytes_read);

  // Writes up cached bytes into `buffer` up to its size, and returns the number
  // of bytes written. The bytes will be removed from the bucket. If the return
  // value is zero, then the bucket was empty. If the return value is less than
  // `buffer.size()`, then the bucket is now empty. Once the bucket is empty it
  // will not become non-empty until there is another call to AttemptRead() or
  // OnReadCompleted(). If the return value is `buffer.size`, there may or may
  // not still be bytes remaining in the bucket.
  size_t Consume(base::span<uint8_t> buffer);

  // True if we are currently reading from `request_`.
  bool read_in_progress() const { return read_in_progress_; }

  // True if we have the result of the final read from URLRequest.
  bool IsComplete() const { return completion_code_.has_value(); }

  // The result of the final read from URLRequest.
  int completion_code() const { return completion_code_.value(); }

 private:
  // The global configuration of SlopBucket.
  class Configuration;

  // An object that manages global state for SlopBuckets.
  class Manager;

  // A struct representing a single fixed-size chunk. A bucket may contain zero
  // or more chunks. Implements net::IOBuffer so that its lifetime matches the
  // expectations of net::URLRequest.
  class ChunkIOBuffer;

  // To make it easy for the compiler to optimize, the part of Consume() that
  // copies data is split off into a separate method.
  size_t ConsumeSlowPath(base::span<uint8_t> buffer)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Attempts to allocates a new chunk. On success, the new chunk is pushed onto
  // `chunks_` and true is returned.
  bool TryToAllocateChunk() VALID_CONTEXT_REQUIRED(sequence_checker_);

  // The chunks this buffer currently owns.
  base::queue<scoped_refptr<ChunkIOBuffer>> chunks_;

  // The largest size reached by `chunks_`. For metrics.
  size_t peak_chunks_allocated_ = 0;

  // The first Chunk allocation blocked reason if it has.
  std::optional<ChunkAllocationFailedReason>
      first_chunk_allocation_failed_reason_;

  // The URLRequest to which reads will be issued.
  const raw_ptr<net::URLRequest> request_;

  // If we saw the final read from the URLRequest, the value it returned. This
  // will be either 0 if the response body was read successfully, or a
  // net::Error code.
  std::optional<int> completion_code_;

  // True if we are currently performing an async read.
  bool read_in_progress_ = false;

  // A SlopBucket should only be accessed on the thread that created it.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SLOP_BUCKET_H_
