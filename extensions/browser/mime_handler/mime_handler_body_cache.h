// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace extensions {

// Drains a data pipe into an in-memory buffer. Optionally also forwards
// bytes into a second consumer pipe in real time so a consumer can read
// the response while it is still being cached. Once the source is fully
// drained, `CreatePipe()` replays the entire buffer into a fresh
// consumer pipe.
//
// Responses larger than the configured cap abandon caching: the
// buffer is released and the forwarding pipe (if any) is torn down,
// which aborts the live load. `is_complete()` stays false so the
// fallback path refetches from the network.
class MimeHandlerBodyCache : public base::RefCounted<MimeHandlerBodyCache>,
                             public mojo::DataPipeDrainer::Client {
 public:
  // Creates a cache that drains `source` into memory. If
  // `out_forwarding_pipe` is non-null, the cache also forwards live
  // bytes into a new consumer pipe returned via that out-parameter. On
  // forwarding-pipe creation failure, `source` is moved back into
  // `*out_forwarding_pipe` so the caller can recover it. Returns null
  // if `source` is invalid.
  static scoped_refptr<MimeHandlerBodyCache> Create(
      mojo::ScopedDataPipeConsumerHandle source,
      mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe);

  // Overrides the cache cap for the lifetime of the returned object.
  // Used by tests that exercise the abandon-on-overflow path without
  // buffering 100 MiB of data.
  [[nodiscard]] static base::AutoReset<size_t> SetMaxCacheBytesForTesting(
      size_t max_bytes);

  MimeHandlerBodyCache();

  MimeHandlerBodyCache(const MimeHandlerBodyCache&) = delete;
  MimeHandlerBodyCache& operator=(const MimeHandlerBodyCache&) = delete;

  // Returns true once the source pipe has been fully drained. A pipe
  // can only be created from the cached buffer once this is true.
  bool is_complete() const { return state_ == State::kComplete; }

  // Returns true if the response exceeded the cap and the cache
  // released its buffer. `is_complete()` stays false in this state;
  // callers must refetch from the network for the full body.
  bool is_abandoned() const { return state_ == State::kAbandoned; }

  // Returns the number of cached bytes.
  size_t cached_size() const { return buffer_.size(); }

  // Creates a new data pipe consumer containing the cached data.
  // Returns an invalid handle if the source is not yet fully drained.
  mojo::ScopedDataPipeConsumerHandle CreatePipe();

 private:
  friend class base::RefCounted<MimeHandlerBodyCache>;
  ~MimeHandlerBodyCache() override;

  // Lifecycle of the buffered drain.
  enum class State { kDraining, kComplete, kAbandoned };

  // Creates the forwarding data pipe and stores the producer end.
  // Returns false on pipe creation failure.
  bool InitializeForwarding(
      mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe);

  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  // Flushes already-buffered bytes that have not yet been written to the
  // forwarding pipe. Re-arms the watcher when the pipe is back-pressured.
  void WritePendingToForwarding();

  // Watcher callback that resumes forwarding when the pipe becomes
  // writable, or tears it down on peer close / error.
  void OnForwardingPipeWritable(MojoResult result,
                                const mojo::HandleSignalsState& state);

  // Drains the source pipe into `buffer_`.
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;

  // Bytes accumulated from the source pipe; replayed by `CreatePipe()`.
  std::vector<uint8_t> buffer_;

  // Current state of the drain.
  State state_ = State::kDraining;

  // Producer end of the forwarding pipe, or invalid if forwarding is
  // not requested or has been torn down.
  mojo::ScopedDataPipeProducerHandle forwarding_producer_;

  // Watches `forwarding_producer_` for writable / peer-closed signals.
  mojo::SimpleWatcher forwarding_watcher_;

  // Number of bytes from `buffer_` that have already been written into
  // `forwarding_producer_`.
  size_t forwarding_offset_ = 0;

  base::WeakPtrFactory<MimeHandlerBodyCache> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_
