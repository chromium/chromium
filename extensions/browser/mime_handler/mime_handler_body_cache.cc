// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_body_cache.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"

namespace extensions {

namespace {

// Matches network::kDefaultDataPipeAllocationSize (see
// //services/network/public/cpp/loading_params.cc): response bodies
// flow in from the network service at this ring-buffer size, so the
// forwarding and replay pipes here use the same cadence.
constexpr uint32_t kDefaultPipeCapacity = 512 * 1024;  // 512 KB

// Cap on bytes buffered for replay. Responses larger than this abandon
// the cache to protect the browser process from OOM. Mutable so tests
// can override it via `SetMaxCacheBytesForTesting()`.
size_t g_max_cache_bytes = 100u * 1024u * 1024u;  // 100 MiB

}  // namespace

// static
scoped_refptr<MimeHandlerBodyCache> MimeHandlerBodyCache::Create(
    mojo::ScopedDataPipeConsumerHandle source,
    mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe) {
  if (!source.is_valid()) {
    return nullptr;
  }

  auto cache = base::MakeRefCounted<MimeHandlerBodyCache>();
  if (out_forwarding_pipe &&
      !cache->InitializeForwarding(out_forwarding_pipe)) {
    // Hand `source` back so the caller can still use the original pipe.
    *out_forwarding_pipe = std::move(source);
    return nullptr;
  }
  cache->StartReading(std::move(source));
  return cache;
}

// static
base::AutoReset<size_t>
MimeHandlerBodyCache::SetMaxCacheBytesForTesting(  // IN-TEST
    size_t max_bytes) {
  return base::AutoReset<size_t>(&g_max_cache_bytes, max_bytes);
}

MimeHandlerBodyCache::MimeHandlerBodyCache()
    : source_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      forwarding_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      forwarding_peer_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

MimeHandlerBodyCache::~MimeHandlerBodyCache() = default;

bool MimeHandlerBodyCache::InitializeForwarding(
    mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe) {
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(kDefaultPipeCapacity, forwarding_producer_,
                           consumer) != MOJO_RESULT_OK) {
    return false;
  }

  forwarding_watcher_.Watch(
      forwarding_producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MimeHandlerBodyCache::OnForwardingPipeWritable,
                          weak_factory_.GetWeakPtr()));

  // Armed only once: a peer can only close once, and the handler
  // always tears the forwarding pipe down.
  forwarding_peer_watcher_.Watch(
      forwarding_producer_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MimeHandlerBodyCache::OnForwardingPeerClosed,
                          weak_factory_.GetWeakPtr()));
  forwarding_peer_watcher_.ArmOrNotify();

  *out_forwarding_pipe = std::move(consumer);
  return true;
}

void MimeHandlerBodyCache::StartReading(
    mojo::ScopedDataPipeConsumerHandle source) {
  source_ = std::move(source);
  source_watcher_.Watch(
      source_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MimeHandlerBodyCache::OnSourceReadable,
                          weak_factory_.GetWeakPtr()));
  source_watcher_.ArmOrNotify();
}

void MimeHandlerBodyCache::OnSourceReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    OnSourceDone();
    return;
  }

  base::span<const uint8_t> data;
  MojoResult read_result =
      source_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, data);
  if (read_result == MOJO_RESULT_SHOULD_WAIT) {
    source_watcher_.ArmOrNotify();
    return;
  }
  if (read_result != MOJO_RESULT_OK) {
    OnSourceDone();
    return;
  }

  // `data` is only valid until `EndReadData()`; the handler consumes
  // it before that.
  SourceReadResult dispatch = HandleSourceBytes(data);
  source_->EndReadData(dispatch.consumed);
  RunNextAction(dispatch.next_action);
}

MimeHandlerBodyCache::SourceReadResult MimeHandlerBodyCache::HandleSourceBytes(
    base::span<const uint8_t> data) {
  switch (state_) {
    case State::kCaching:
      return HandleCachingBytes(data);
    case State::kForwardOnly:
      return HandleForwardOnlyBytes(data);
    case State::kComplete:
    case State::kStopped:
      NOTREACHED();
  }
}

void MimeHandlerBodyCache::RunNextAction(NextAction action) {
  switch (action) {
    case NextAction::kNone:
      break;
    case NextAction::kReadSource:
      source_watcher_.ArmOrNotify();
      break;
    case NextAction::kFlushStaging:
      // The unconsumed chunk is still in the source pipe; source reads
      // resume once the backlog drains.
      FlushStaging();
      break;
    case NextAction::kStop:
      EnterStopped();
      break;
  }
}

void MimeHandlerBodyCache::OnSourceDone() {
  source_watcher_.Cancel();
  source_.reset();

  switch (state_) {
    case State::kCaching:
      state_ = State::kComplete;
      if (forwarding_producer_.is_valid()) {
        // Flushes the tail; the producer closes once everything has
        // been written, signalling EOF to the live consumer.
        WritePendingToForwarding();
      }
      break;
    case State::kForwardOnly:
      // Source reads pause while the backlog is in flight, so EOF
      // can only be observed with an empty staging buffer. Closing the
      // forwarding producer signals EOF to the live consumer.
      CHECK(staging_.empty());
      EnterStopped();
      break;
    case State::kComplete:
    case State::kStopped:
      NOTREACHED();
  }
}

MimeHandlerBodyCache::SourceReadResult MimeHandlerBodyCache::HandleCachingBytes(
    base::span<const uint8_t> data) {
  if (buffer_.size() + data.size() > g_max_cache_bytes) {
    // Response exceeds the cap: replay is abandoned and `is_complete()`
    // stays false, so the fallback path refetches from the network.
    if (!forwarding_producer_.is_valid()) {
      // No live consumer needs the remaining bytes either.
      buffer_.clear();
      buffer_.shrink_to_fit();
      return {data.size(), NextAction::kStop};
    }
    // A live consumer still needs the body. The cached buffer becomes
    // the staged backlog as-is (no copy); the chunk stays unconsumed
    // in the source pipe and is re-read once the backlog drains, so
    // streaming continues with bounded memory from here on.
    state_ = State::kForwardOnly;
    staging_ = std::exchange(buffer_, {});
    staging_offset_ = forwarding_offset_;
    return {0, NextAction::kFlushStaging};
  }

  buffer_.insert(buffer_.end(), data.begin(), data.end());
  if (forwarding_producer_.is_valid()) {
    WritePendingToForwarding();
  }
  return {data.size(), NextAction::kReadSource};
}

MimeHandlerBodyCache::SourceReadResult
MimeHandlerBodyCache::HandleForwardOnlyBytes(base::span<const uint8_t> data) {
  // Source reads pause while the crossover backlog is in flight, so it
  // has necessarily drained here: bytes go straight from the source
  // span into the pipe.
  CHECK(staging_.empty());
  size_t offset = 0;
  switch (ForwardBytes(data, offset)) {
    case ForwardResult::kPeerGone:
      return {offset, NextAction::kStop};
    case ForwardResult::kDrained:
      return {offset, NextAction::kReadSource};
    case ForwardResult::kBackpressured:
      // `ForwardBytes()` armed the writable watcher; the unread bytes
      // stay in the source pipe.
      return {offset, NextAction::kNone};
  }
}

MimeHandlerBodyCache::ForwardResult MimeHandlerBodyCache::ForwardBytes(
    base::span<const uint8_t> pending,
    size_t& offset) {
  while (offset < pending.size()) {
    size_t bytes_written = 0;
    MojoResult result = forwarding_producer_->WriteData(
        pending.subspan(offset), MOJO_WRITE_DATA_FLAG_NONE, bytes_written);

    if (result == MOJO_RESULT_OK) {
      offset += bytes_written;
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      // Back-pressure: source reads stay paused until the consumer
      // drains the pipe and the watcher fires.
      forwarding_watcher_.ArmOrNotify();
      return ForwardResult::kBackpressured;
    } else {
      return ForwardResult::kPeerGone;
    }
  }
  return ForwardResult::kDrained;
}

void MimeHandlerBodyCache::WritePendingToForwarding() {
  if (!forwarding_producer_.is_valid()) {
    return;
  }

  switch (ForwardBytes(buffer_, forwarding_offset_)) {
    case ForwardResult::kBackpressured:
      return;
    case ForwardResult::kPeerGone:
      // The consumer went away; keep caching for the fallback replay.
      CloseForwarding();
      return;
    case ForwardResult::kDrained:
      break;
  }

  if (state_ == State::kComplete) {
    CloseForwarding();
  }
}

void MimeHandlerBodyCache::FlushStaging() {
  CHECK(state_ == State::kForwardOnly);

  switch (ForwardBytes(staging_, staging_offset_)) {
    case ForwardResult::kBackpressured:
      return;
    case ForwardResult::kPeerGone:
      EnterStopped();
      return;
    case ForwardResult::kDrained:
      break;
  }

  // The backlog has been fully handed to the pipe; release the
  // memory before resuming source reads. It can be as large as the
  // cap, so keeping its capacity around would defeat the bound.
  staging_.clear();
  staging_.shrink_to_fit();
  staging_offset_ = 0;
  source_watcher_.ArmOrNotify();
}

void MimeHandlerBodyCache::OnForwardingPipeWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK || state.peer_closed()) {
    OnForwardingGone();
    return;
  }

  switch (state_) {
    case State::kCaching:
    case State::kComplete:
      WritePendingToForwarding();
      break;
    case State::kForwardOnly:
      if (!staging_.empty()) {
        FlushStaging();
      } else {
        // Back-pressure hit on a direct forward: the bytes are still
        // in the source pipe, so resume reading it.
        source_watcher_.ArmOrNotify();
      }
      break;
    case State::kStopped:
      NOTREACHED();
  }
}

void MimeHandlerBodyCache::OnForwardingPeerClosed(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result == MOJO_RESULT_CANCELLED) {
    // The watched handle was closed locally; not a disconnect.
    return;
  }
  OnForwardingGone();
}

void MimeHandlerBodyCache::OnForwardingGone() {
  switch (state_) {
    case State::kCaching:
    case State::kComplete:
      // The live consumer went away; keep caching so the fallback
      // replay stays available.
      CloseForwarding();
      break;
    case State::kForwardOnly:
      // No replay and no live consumer: nothing needs the remaining
      // bytes, so stop reading and close the source.
      EnterStopped();
      break;
    case State::kStopped:
      NOTREACHED();
  }
}

void MimeHandlerBodyCache::CloseForwarding() {
  // Cancel before closing the handle so neither watcher observes the
  // local close as a MOJO_RESULT_CANCELLED notification.
  forwarding_watcher_.Cancel();
  forwarding_peer_watcher_.Cancel();
  forwarding_producer_.reset();
}

void MimeHandlerBodyCache::EnterStopped() {
  state_ = State::kStopped;
  staging_.clear();
  staging_.shrink_to_fit();
  staging_offset_ = 0;
  CloseForwarding();
  source_watcher_.Cancel();
  source_.reset();
}

mojo::ScopedDataPipeConsumerHandle MimeHandlerBodyCache::CreatePipe() {
  if (!is_complete()) {
    return mojo::ScopedDataPipeConsumerHandle();
  }

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(kDefaultPipeCapacity, producer_handle, consumer) !=
      MOJO_RESULT_OK) {
    return mojo::ScopedDataPipeConsumerHandle();
  }

  if (buffer_.empty()) {
    // Nothing to replay; drop the producer so the consumer sees EOF.
    return consumer;
  }

  // Stream the buffer asynchronously through `mojo::DataPipeProducer`.
  // It chunks the write into the pipe's ring-buffer capacity rather
  // than allocating shared memory the size of the response and
  // copying it synchronously on the calling sequence.
  auto pipe_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* const producer_ptr = pipe_producer.get();
  producer_ptr->Write(
      std::make_unique<mojo::StringDataSource>(
          base::as_chars(base::span(buffer_)),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_STAYS_VALID_UNTIL_COMPLETION),
      // The bound `scoped_refptr` keeps this cache (and `buffer_`)
      // alive while the worker sequence reads from it. The bound
      // `unique_ptr` owns the producer until the write completes; the
      // producer handle closes when the producer is destroyed at the
      // end of this lambda, signalling EOF to the consumer.
      base::BindOnce([](scoped_refptr<MimeHandlerBodyCache>,
                        std::unique_ptr<mojo::DataPipeProducer>, MojoResult) {},
                     base::WrapRefCounted(this), std::move(pipe_producer)));
  return consumer;
}

}  // namespace extensions
