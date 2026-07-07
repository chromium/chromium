// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_

#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace extensions {

// Drains a data pipe into an in-memory buffer. Optionally also forwards
// bytes into a second consumer pipe in real time so a consumer can read
// the response while it is still being cached. Once the source is fully
// drained, `CreatePipe()` replays the entire buffer into a fresh
// consumer pipe.
//
// Responses larger than the configured cap abandon caching: the buffer
// is released and `is_complete()` stays false so the fallback path
// refetches from the network. A live forwarding consumer keeps
// receiving the remaining bytes with bounded memory: the crossover
// reuses the cached buffer as the staged backlog (bounded by the
// cap); once it drains, bytes flow straight from the source pipe to
// the consumer, and back-pressure leaves them unread in the source
// pipe until the consumer makes room.
//
// State machine (a chunk handler returns the next action; the source
// watcher re-arms only between chunks). A consumer disconnect while
// caching keeps the state at kCaching so the replay buffer stays
// available for the fallback path:
//
//                            source bytes
//                                 |
//                                 v
//                          +---------------+
//                          |    kCaching   |  append to buffer_;
//                          | (buffer for   |  forward live if a
//                          |    replay)    |  consumer is attached
//                          +---------------+
//                            |     |     |
//             EOF under cap  |     |     |  over cap, no consumer
//                  +---------+     |     +---------+
//                  |   over cap, live consumer     |
//                  v               v               v
//           +-----------+   +--------------+   +-----------+
//           | kComplete |   | kForwardOnly |   |  kStopped |
//           |  (replay  |   | (stream rest |   | (no replay|
//           |  buffer_) |   |  bounded mem)|   |  source   |
//           +-----------+   +--------------+   |  closed)  |
//                                  |           +-----------+
//                         EOF / consumer gone        ^
//                                  +-----------------+
class MimeHandlerBodyCache : public base::RefCounted<MimeHandlerBodyCache> {
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
  bool is_abandoned() const {
    return state_ == State::kForwardOnly || state_ == State::kStopped;
  }

  // Returns the number of cached bytes.
  size_t cached_size() const { return buffer_.size(); }

  // Creates a new data pipe consumer containing the cached data.
  // Returns an invalid handle if the source is not yet fully drained.
  mojo::ScopedDataPipeConsumerHandle CreatePipe();

 private:
  friend class base::RefCounted<MimeHandlerBodyCache>;
  ~MimeHandlerBodyCache();

  // Lifecycle of the buffered drain.
  enum class State {
    // Source bytes are accumulated in `buffer_` for replay.
    kCaching,
    // The source ended under the cap; `buffer_` is replayable.
    kComplete,
    // The cap was exceeded: replay is abandoned, but bytes keep
    // streaming to the live forwarding consumer with bounded memory.
    kForwardOnly,
    // No replay is possible and no live consumer remains.
    kStopped,
  };

  // Continuation after a source chunk has been dispatched.
  enum class NextAction {
    kNone,
    kReadSource,
    kFlushStaging,
    kStop,
  };

  // Result of dispatching one source chunk.
  struct SourceReadResult {
    size_t consumed = 0;
    NextAction next_action = NextAction::kNone;
  };

  // Creates the forwarding data pipe and stores the producer end.
  // Returns false on pipe creation failure.
  bool InitializeForwarding(
      mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe);

  // Takes ownership of `source` and starts pumping it.
  void StartReading(mojo::ScopedDataPipeConsumerHandle source);

  // Source watcher callback shell: reads the next chunk, dispatches it
  // via `HandleSourceBytes()`, ends the read, and runs the returned
  // continuation.
  void OnSourceReadable(MojoResult result,
                        const mojo::HandleSignalsState& state);

  // Dispatches one source chunk according to the current state.
  SourceReadResult HandleSourceBytes(base::span<const uint8_t> data);

  // Appends `data` to `buffer_`, or crosses over to `kForwardOnly` /
  // `kStopped` when the chunk would exceed the cap. The returned
  // `consumed` count may be zero: at the crossover the chunk is left in
  // the source pipe (appending it to the backlog could reallocate the
  // cap-sized buffer) and is re-read once the backlog drains.
  SourceReadResult HandleCachingBytes(base::span<const uint8_t> data);

  // Forwards `data` straight from the source span into the forwarding
  // pipe in `kForwardOnly`, once the crossover backlog has drained.
  SourceReadResult HandleForwardOnlyBytes(base::span<const uint8_t> data);

  // Runs the continuation chosen by a chunk handler: re-arms the
  // source, flushes the crossover backlog, finishes teardown, or does
  // nothing.
  void RunNextAction(NextAction action);

  // Handles source EOF (or a broken source pipe, which is
  // indistinguishable and treated the same way).
  void OnSourceDone();

  // Outcome of pushing bytes into the forwarding pipe.
  enum class ForwardResult {
    // All bytes were accepted.
    kDrained,
    // The pipe is full; the writable watcher has been armed.
    kBackpressured,
    // The write failed; the consumer is gone.
    kPeerGone,
  };

  // Writes `pending` from `offset` onwards into the forwarding pipe,
  // advancing `offset` past the accepted bytes.
  ForwardResult ForwardBytes(base::span<const uint8_t> pending, size_t& offset);

  // Flushes already-buffered bytes that have not yet been written to the
  // forwarding pipe. Re-arms the watcher when the pipe is back-pressured.
  void WritePendingToForwarding();

  // Flushes `staging_` into the forwarding pipe. Source reads resume
  // only once the whole staged backlog has been accepted; on
  // back-pressure the writable watcher re-arms instead.
  void FlushStaging();

  // Watcher callback that resumes forwarding when the pipe becomes
  // writable.
  void OnForwardingPipeWritable(MojoResult result,
                                const mojo::HandleSignalsState& state);

  // Watcher callback for forwarding-pipe peer closure. Armed once at
  // creation so a disconnect is noticed even while no write is
  // pending.
  void OnForwardingPeerClosed(MojoResult result,
                              const mojo::HandleSignalsState& state);

  // Reacts to the forwarding consumer going away: before overflow the
  // cache keeps buffering for replay; after overflow nothing needs the
  // remaining bytes, so everything shuts down.
  void OnForwardingGone();

  // Closes the forwarding producer and cancels its watchers.
  void CloseForwarding();

  // Terminal teardown: releases all pipes, watchers and staged bytes.
  void EnterStopped();

  // Source pipe being pumped. Reads are eager while caching; in
  // `kForwardOnly` they pause while the backlog is in flight or the
  // forwarding pipe is full.
  mojo::ScopedDataPipeConsumerHandle source_;

  // Watches `source_` for readability.
  mojo::SimpleWatcher source_watcher_;

  // Bytes accumulated from the source pipe; replayed by `CreatePipe()`.
  std::vector<uint8_t> buffer_;

  // Current state of the drain.
  State state_ = State::kCaching;

  // Producer end of the forwarding pipe, or invalid if forwarding is
  // not requested or has been torn down.
  mojo::ScopedDataPipeProducerHandle forwarding_producer_;

  // Watches `forwarding_producer_` for writability; armed only while
  // forwarded bytes are pending behind back-pressure.
  mojo::SimpleWatcher forwarding_watcher_;

  // Watches `forwarding_producer_` for peer closure. Separate from
  // `forwarding_watcher_`, which is armed only under back-pressure: an
  // idle cache would otherwise never observe a disconnect and, after
  // overflow, would keep the source pipe open indefinitely.
  mojo::SimpleWatcher forwarding_peer_watcher_;

  // Number of bytes from `buffer_` that have already been written into
  // `forwarding_producer_`.
  size_t forwarding_offset_ = 0;

  // The cached buffer, reused at the crossover as the in-flight
  // backlog for the forwarding pipe (bounded by the cap;
  // `staging_offset_` skips the already-forwarded prefix). Empty once
  // drained: from then on bytes are forwarded straight from the
  // source pipe. This is what bounds memory in `kForwardOnly`.
  std::vector<uint8_t> staging_;

  // Number of bytes from `staging_` already accepted by the pipe.
  size_t staging_offset_ = 0;

  base::WeakPtrFactory<MimeHandlerBodyCache> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_BODY_CACHE_H_
