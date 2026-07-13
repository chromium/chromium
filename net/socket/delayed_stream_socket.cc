// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_stream_socket.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/stream_socket.h"

namespace net {

namespace {

// Size of the scratch buffer for reading from / writing to the inner socket.
// 32KB is large enough for efficiency, small enough to avoid large transient
// allocations per IO.
constexpr int kInnerReadBufferSize = 32 * 1024;
constexpr int kInnerWriteBufferSize = 32 * 1024;

}  // namespace

DelayedStreamSocket::DelayedStreamSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    const DelayedSocketConfig& config,
    scoped_refptr<BandwidthThrottle> download_throttle,
    scoped_refptr<BandwidthThrottle> upload_throttle)
    : wrapped_socket_(std::move(stream_socket)),
      config_(config),
      download_throttle_(std::move(download_throttle)),
      upload_throttle_(std::move(upload_throttle)),
      // BottleneckBuffer models propagation delay only; the shared
      // BandwidthThrottle(s) above do the rate limiting. Config throughput
      // only sizes buffer capacity (absent == unconstrained == 0 to
      // BdpCapacity) and is intentionally separate from the throttle rate.
      download_buffer_(
          config_.rtt / 2,
          BottleneckBuffer::BdpCapacity(
              config_.download_throughput_bytes_per_sec.value_or(0),
              config_.rtt / 2)),
      upload_buffer_(config_.rtt / 2,
                     BottleneckBuffer::BdpCapacity(
                         config_.upload_throughput_bytes_per_sec.value_or(0),
                         config_.rtt / 2)),
      inner_read_buffer_(
          base::MakeRefCounted<IOBufferWithSize>(kInnerReadBufferSize)),
      inner_write_buffer_(
          base::MakeRefCounted<IOBufferWithSize>(kInnerWriteBufferSize)) {
  // Finite throughput requires a shared throttle; a null throttle implies
  // unconstrained throughput (std::nullopt).
  CHECK(download_throttle_ ||
        !config_.download_throughput_bytes_per_sec.has_value());
  CHECK(upload_throttle_ ||
        !config_.upload_throughput_bytes_per_sec.has_value());

  RebindBufferCallbacks();
}

void DelayedStreamSocket::RebindBufferCallbacks() {
  download_buffer_.SetCallbacks(
      base::BindRepeating(&DelayedStreamSocket::OnDownloadDataReady,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DelayedStreamSocket::OnDownloadSpaceAvailable,
                          weak_factory_.GetWeakPtr()));
  upload_buffer_.SetCallbacks(
      base::BindRepeating(&DelayedStreamSocket::OnUploadDataReady,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&DelayedStreamSocket::OnUploadSpaceAvailable,
                          weak_factory_.GetWeakPtr()));
}

DelayedStreamSocket::~DelayedStreamSocket() = default;

// --- Connect ---

int DelayedStreamSocket::Connect(CompletionOnceCallback callback) {
  // No re-entrant Connect while a prior Connect is still in flight: the
  // previous attempt's callback is on the way to the consumer and would
  // be lost. In the passthrough branch below (no configured latency)
  // the wrapped socket diagnoses its own re-entry.
  CHECK(!connect_pending_);

  // Reset the per-session usage bit on reconnect so `WasEverUsed()`
  // reflects activity on the current session, matching TCPClientSocket.
  if (previously_disconnected_) {
    was_ever_used_ = false;
    previously_disconnected_ = false;
  }

  // StreamSocket allows reconnecting after Disconnect(); refresh the buffer
  // callbacks so they bind to the *current* WeakPtr (a prior Disconnect()
  // invalidates all existing weak pointers, including the buffer's).
  RebindBufferCallbacks();

  // Sync passthrough when no latency is configured. Skip
  // `connect_pending_` here: the wrapped socket owns the pending state
  // and IsConnected() delegates to it, so a shape-free wrapper matches
  // the wrapped socket's own semantics exactly.
  if (!config_.rtt.is_positive()) {
    return wrapped_socket_->Connect(std::move(callback));
  }

  // Shaping path: from here until the consumer's callback fires we are
  // pending, so IsConnected()/IsConnectedAndIdle() must report false
  // even between wrapped-Connect completion and our latency timer
  // firing (during that window wrapped_socket_->IsConnected() may
  // already be true but the consumer has not been signalled).
  connect_pending_ = true;
  // Timestamp Connect() entry so ScheduleConnectCompletion can model a full
  // RTT measured from here rather than adding a full RTT on top of however
  // long the inner connect took.
  const base::TimeTicks connect_started = base::TimeTicks::Now();
  auto [for_async, for_sync] = base::SplitOnceCallback(std::move(callback));
  int rv = wrapped_socket_->Connect(base::BindOnce(
      &DelayedStreamSocket::ScheduleConnectCompletion,
      weak_factory_.GetWeakPtr(), std::move(for_async), connect_started));
  if (rv == ERR_IO_PENDING) {
    return ERR_IO_PENDING;
  }
  ScheduleConnectCompletion(std::move(for_sync), connect_started, rv);
  return ERR_IO_PENDING;
}

void DelayedStreamSocket::ScheduleConnectCompletion(
    CompletionOnceCallback callback,
    base::TimeTicks connect_started,
    int result) {
  CHECK(config_.rtt.is_positive());
  // Model a full-RTT connect (SYN + SYN-ACK) measured from Connect() entry.
  // The inner connect already consumed real time; subtract it so the total
  // modelled connect latency is `config_.rtt`, not `config_.rtt`
  // plus the inner connect duration. Clamp at zero so an inner connect that
  // already exceeded the modelled RTT still completes asynchronously, as
  // soon as possible.
  const base::TimeDelta delay =
      std::max(config_.rtt - (base::TimeTicks::Now() - connect_started),
               base::TimeDelta());
  connect_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&DelayedStreamSocket::OnConnectTimerFired,
                     weak_factory_.GetWeakPtr(), std::move(callback), result));
}

void DelayedStreamSocket::OnConnectTimerFired(CompletionOnceCallback callback,
                                              int result) {
  connect_pending_ = false;
  std::move(callback).Run(result);
}

// --- Read ---

int DelayedStreamSocket::Read(IOBuffer* buffer,
                              int buffer_len,
                              CompletionOnceCallback callback) {
  CHECK(buffer);
  CHECK_GT(buffer_len, 0);
  CHECK(!pending_read_callback_);
  CHECK(!pending_read_if_ready_callback_);

  if (IsReadPassthrough()) {
    int rv = wrapped_socket_->Read(
        buffer, buffer_len,
        base::BindOnce(&DelayedStreamSocket::DidCompletePassthroughIO,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    if (rv > 0) {
      was_ever_used_ = true;
    }
    return rv;
  }

  // Fast path: bytes are ready and we either have no shared throttle or a
  // previous request already granted bytes that we haven't fully consumed.
  if (download_buffer_.has_ready_data() &&
      (!download_throttle_ || pending_read_throttle_grant_ > 0)) {
    int bytes_read = ReadIntoCaller(buffer, buffer_len);
    if (bytes_read > 0) {
      ConsumeDownloadGrant(bytes_read);
      MaybeStartInnerRead();
      was_ever_used_ = true;
      return bytes_read;
    }
  }

  if (IsInnerReadDone()) {
    return inner_read_error_;
  }

  // Reject Read on a disconnected socket synchronously (matches
  // Socket::Read's ERR_SOCKET_NOT_CONNECTED contract) rather than pending
  // and then failing async. Preserve leftover reads: the fast path above
  // has already drained any ready bytes, and IsInnerReadDone reported
  // false so `download_buffer_` may still hold not-yet-ready bytes we
  // should still deliver. Only bail when the buffer is fully empty AND
  // the inner has never surfaced an error/EOF yet (so we can't already
  // return `inner_read_error_`).
  if (!wrapped_socket_->IsConnected() && download_buffer_.empty() &&
      !inner_read_eof_) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  pending_read_buffer_ = buffer;
  pending_read_buffer_len_ = buffer_len;
  pending_read_callback_ = std::move(callback);

  // Either bytes are ready and need a shared-throttle grant, or we need to
  // pull more from the inner socket. TryFulfillRead handles both.
  TryFulfillRead();
  MaybeStartInnerRead();
  return ERR_IO_PENDING;
}

int DelayedStreamSocket::ReadIfReady(IOBuffer* buffer,
                                     int buffer_len,
                                     CompletionOnceCallback callback) {
  CHECK(buffer);
  CHECK_GT(buffer_len, 0);
  CHECK(!pending_read_callback_);
  CHECK(!pending_read_if_ready_callback_);

  if (IsReadPassthrough()) {
    int rv = wrapped_socket_->ReadIfReady(
        buffer, buffer_len,
        base::BindOnce(&DelayedStreamSocket::DidCompletePassthroughIO,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    if (rv > 0) {
      was_ever_used_ = true;
    }
    return rv;
  }

  // Fast path: data is ready and either no throttle is required or one has
  // already granted bandwidth.
  if (download_buffer_.has_ready_data() &&
      (!download_throttle_ || pending_read_throttle_grant_ > 0)) {
    int bytes_read = ReadIntoCaller(buffer, buffer_len);
    if (bytes_read > 0) {
      ConsumeDownloadGrant(bytes_read);
      MaybeStartInnerRead();
      was_ever_used_ = true;
      return bytes_read;
    }
  }

  if (IsInnerReadDone()) {
    return inner_read_error_;
  }

  // Reject on disconnected inner socket for the same reasons as Read().
  if (!wrapped_socket_->IsConnected() && download_buffer_.empty() &&
      !inner_read_eof_) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  // ReadIfReady never holds the caller's IOBuffer; we just need a callback
  // to signal readiness with OK once data is available (and bandwidth is
  // granted, if a shared throttle is in use).
  pending_read_if_ready_callback_ = std::move(callback);
  TryFulfillRead();
  MaybeStartInnerRead();
  return ERR_IO_PENDING;
}

int DelayedStreamSocket::CancelReadIfReady() {
  // In passthrough mode the caller's ReadIfReady went straight to the inner
  // socket; we must forward the cancellation or the inner read stays
  // pending and eventually fires unexpectedly.
  if (IsReadPassthrough()) {
    return wrapped_socket_->CancelReadIfReady();
  }
  pending_read_if_ready_callback_.Reset();
  // Cancel any already-posted trampoline so the consumer's callback does
  // not fire after this returns (Socket::CancelReadIfReady contract).
  pending_read_if_ready_dispatch_.Cancel();
  // Drop any leftover throttle grant so a subsequent Read can't absorb
  // bytes the cancelled caller already paid for (the tokens have already
  // been deducted from the shared throttle, but they were earmarked for
  // this cancelled request).
  pending_read_throttle_grant_ = 0;
  // Cancel any in-flight throttle request so it doesn't keep tokens
  // charged against the shared link on behalf of a request the caller
  // already cancelled, and can't later grant bytes to a no-longer-pending
  // read.
  download_throttle_cancellation_ = BandwidthThrottle::CancellationHandle();
  download_throttle_pending_ = false;
  // We deliberately do not cancel inner reads on the pipeline path: data in
  // the buffer is kept for future reads.
  return OK;
}

bool DelayedStreamSocket::IsReadPassthrough() const {
  // `download_throttle_` is null when download bandwidth is unlimited.
  return !config_.rtt.is_positive() && !download_throttle_;
}

bool DelayedStreamSocket::IsInnerReadDone() const {
  return download_buffer_.empty() && inner_read_eof_;
}

void DelayedStreamSocket::MaybeStartInnerRead() {
  // `!download_throttle_pending_` mirrors the sync loop guard in
  // StartInnerRead: while the consumer is waiting for a shared
  // download-throttle grant, don't issue another inner Read behind its
  // back or read-ahead would proceed uncapped and defeat backpressure.
  // Reads resume when OnDownloadThrottleReady clears the flag and calls
  // back into MaybeStartInnerRead.
  if (!processing_inner_read_ && !inner_read_pending_ && !inner_read_eof_ &&
      !download_throttle_pending_ && !download_buffer_.full()) {
    StartInnerRead();
  }
}

// Copies as many ready bytes as possible from the download buffer into the
// caller's IOBuffer, returning the byte count (the value Read() would
// report). Returns 0 if the front chunk isn't ready yet.
//
// `BottleneckBuffer::Pull` in kStream mode concatenates ready chunks across
// boundaries, so if a shared throttle is in use we MUST cap the destination
// span to `pending_read_throttle_grant_` or we would charge the throttle
// for fewer bytes than we hand to the caller and bypass the link's rate
// limit.
int DelayedStreamSocket::ReadIntoCaller(IOBuffer* buffer, int buffer_len) {
  int max_to_read = buffer_len;
  if (download_throttle_) {
    max_to_read = std::min(max_to_read, pending_read_throttle_grant_);
  }
  auto dest = buffer->span().first(static_cast<size_t>(max_to_read));
  return download_buffer_.Pull(dest);
}

void DelayedStreamSocket::CompleteRead(int result) {
  // Move state out cleanly so the callback can never observe stale member
  // state. Route the actual invocation through DispatchPendingCompletion
  // so:
  //   * the consumer's callback never fires on a stack that is still
  //     inside Read() / StartInnerRead() / OnInnerReadComplete(); the
  //     consumer is allowed to destroy the socket inside the callback;
  //   * Disconnect()/dtor cancels the pending completion per the Socket
  //     contract (the trampoline is WeakPtr-bound);
  //   * the caller's IOBuffer is held alive across the post hop (carried
  //     as the `keep_alive` parameter).
  scoped_refptr<IOBuffer> held_buffer = std::move(pending_read_buffer_);
  pending_read_buffer_len_ = 0;
  if (result > 0) {
    ConsumeDownloadGrant(result);
  } else {
    pending_read_throttle_grant_ = 0;
  }
  CompletionOnceCallback callback = std::move(pending_read_callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DelayedStreamSocket::DispatchPendingCompletion,
                     weak_factory_.GetWeakPtr(), std::move(held_buffer),
                     std::move(callback), result));
}

void DelayedStreamSocket::ConsumeDownloadGrant(int bytes_consumed) {
  pending_read_throttle_grant_ =
      std::max(0, pending_read_throttle_grant_ - bytes_consumed);
}

void DelayedStreamSocket::CompleteReadIfReady(int result) {
  // See CompleteRead for the rationale. ReadIfReady never holds the
  // caller's IOBuffer, so the trampoline carries a null buffer ref.
  //
  // The trampoline post is wrapped in a CancelableOnceClosure so that
  // CancelReadIfReady() can cancel it even after the callback has been
  // moved out of `pending_read_if_ready_callback_`. Without this the
  // consumer's callback could still run after CancelReadIfReady() returns.
  CompletionOnceCallback callback = std::move(pending_read_if_ready_callback_);
  pending_read_if_ready_dispatch_.Reset(base::BindOnce(
      &DelayedStreamSocket::DispatchPendingCompletion,
      weak_factory_.GetWeakPtr(),
      /*keep_alive=*/scoped_refptr<IOBuffer>(), std::move(callback), result));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, pending_read_if_ready_dispatch_.callback());
}

void DelayedStreamSocket::DispatchPendingCompletion(
    scoped_refptr<IOBuffer> /*keep_alive*/,
    CompletionOnceCallback callback,
    int result) {
  // `keep_alive` is intentionally unused; the parameter exists solely to
  // hold a reference to the consumer's IOBuffer across the posted-task hop
  // (per the Socket contract).
  //
  // Chokepoint for all async Read/Write completions: set the
  // WasEverUsed() bit only on a positive byte count, matching
  // TCPClientSocket. Read errors, EOF (0), and ReadIfReady's readiness
  // signal (OK == 0) do NOT flip the bit.
  if (result > 0) {
    was_ever_used_ = true;
  }
  std::move(callback).Run(result);
}

void DelayedStreamSocket::DidCompletePassthroughIO(
    CompletionOnceCallback callback,
    int result) {
  if (result > 0) {
    was_ever_used_ = true;
  }
  std::move(callback).Run(result);
}

void DelayedStreamSocket::StartInnerRead() {
  CHECK(!inner_read_pending_);
  CHECK(!inner_read_eof_);

  base::AutoReset<bool> auto_reset(&processing_inner_read_, true);

  while (true) {
    // free_space() returns size_t; Socket::Read takes int.
    int read_size = base::checked_cast<int>(
        std::min<size_t>(kInnerReadBufferSize, download_buffer_.free_space()));
    if (read_size == 0) {
      return;
    }
    inner_read_pending_ = true;
    int rv = wrapped_socket_->Read(
        inner_read_buffer_.get(), read_size,
        base::BindOnce(&DelayedStreamSocket::OnInnerReadComplete,
                       weak_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING) {
      return;  // Will continue in OnInnerReadComplete.
    }
    ProcessInnerReadResult(rv);

    // If the consumer is waiting for the shared throttle, keep buffered
    // bytes in place to preserve backpressure. Pending reads that are only
    // waiting for latency do not block read-ahead.
    if (inner_read_eof_ || download_throttle_pending_) {
      return;
    }
  }
}

void DelayedStreamSocket::OnInnerReadComplete(int result) {
  ProcessInnerReadResult(result);
  // ProcessInnerReadResult may have synchronously fulfilled a pending
  // read and re-entered MaybeStartInnerRead(). Go through
  // MaybeStartInnerRead here too so we don't try to overlap two inner
  // reads on the same socket.
  MaybeStartInnerRead();
}

// Common handling for sync and async inner-read completions. Updates EOF /
// error state, pushes bytes into the buffer, and tries to wake any pending
// reader.
void DelayedStreamSocket::ProcessInnerReadResult(int result) {
  inner_read_pending_ = false;
  if (result <= 0) {
    inner_read_eof_ = true;
    inner_read_error_ = result;
    if (download_buffer_.empty()) {
      TryFulfillRead();
    }
    return;
  }
  int accepted = download_buffer_.Push(
      inner_read_buffer_->span().first(static_cast<size_t>(result)));
  CHECK_EQ(accepted, result);
  TryFulfillRead();
}

void DelayedStreamSocket::OnDownloadDataReady() {
  TryFulfillRead();
}

void DelayedStreamSocket::OnDownloadSpaceAvailable() {
  // Buffer freed space; resume reading from OS socket.
  if (!inner_read_pending_ && !inner_read_eof_) {
    StartInnerRead();
  }
}

// Dispatches to whichever read (if any) is pending. Either:
//   - a ReadIfReady waiting only for a readiness signal, or
//   - a Read waiting to actually copy bytes into the caller's buffer.
void DelayedStreamSocket::TryFulfillRead() {
  if (pending_read_if_ready_callback_) {
    TryFulfillReadIfReady();
  } else if (pending_read_callback_) {
    TryFulfillPendingRead();
  }
}

void DelayedStreamSocket::TryFulfillReadIfReady() {
  if (download_buffer_.has_ready_data()) {
    if (!EnsureDownloadGrant(std::numeric_limits<int>::max())) {
      return;  // Waiting for shared throttle.
    }
    // ReadIfReady contract: completion only signals readiness; the caller
    // re-enters ReadIfReady to copy bytes from the buffer.
    CompleteReadIfReady(OK);
    return;
  }
  if (IsInnerReadDone()) {
    CompleteReadIfReady(inner_read_error_);
  }
}

void DelayedStreamSocket::TryFulfillPendingRead() {
  if (download_buffer_.has_ready_data()) {
    if (!EnsureDownloadGrant(pending_read_buffer_len_)) {
      return;  // Waiting for shared throttle.
    }
    int bytes_read =
        ReadIntoCaller(pending_read_buffer_.get(), pending_read_buffer_len_);
    if (bytes_read > 0) {
      // CompleteRead posts the consumer's callback, so any subsequent
      // member access here is safe even if the callback would have
      // destroyed `this` had it run synchronously.
      CompleteRead(bytes_read);
      MaybeStartInnerRead();
      return;
    }
  }
  if (IsInnerReadDone()) {
    CompleteRead(inner_read_error_);
  }
}

// Returns true if we may consume from the download buffer right now: either
// there is no shared throttle, or a previous request already granted bytes.
// Otherwise issues a throttle request (if not already pending) and returns
// false.
bool DelayedStreamSocket::EnsureDownloadGrant(int max_bytes) {
  if (!download_throttle_ || pending_read_throttle_grant_ > 0) {
    return true;
  }
  RequestDownloadThrottle(max_bytes);
  return false;
}

void DelayedStreamSocket::RequestDownloadThrottle(int max_bytes) {
  if (download_throttle_pending_) {
    return;
  }
  // GetReadyBytesAtFront returns size_t; RequestBytes takes int.
  int ready =
      base::checked_cast<int>(download_buffer_.GetReadyBytesAtFront(max_bytes));
  if (ready == 0) {
    return;
  }
  download_throttle_pending_ = true;
  download_throttle_cancellation_ = download_throttle_->RequestBytes(
      ready, base::BindOnce(&DelayedStreamSocket::OnDownloadThrottleReady,
                            weak_factory_.GetWeakPtr(), ready));
}

void DelayedStreamSocket::OnDownloadThrottleReady(int granted_bytes) {
  download_throttle_pending_ = false;
  pending_read_throttle_grant_ = granted_bytes;
  TryFulfillRead();
  // Resume read-ahead if it was suppressed by `download_throttle_pending_`
  // in MaybeStartInnerRead(). TryFulfillRead()'s success paths call
  // MaybeStartInnerRead() for the pending-Read case, but the
  // pending-ReadIfReady and no-pending-read cases do not, so a bare
  // call here covers the gap without needing to reason about which
  // caller branch we came from.
  MaybeStartInnerRead();
}

// --- Write ---

int DelayedStreamSocket::Write(
    IOBuffer* buffer,
    int buffer_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(buffer);
  CHECK_GT(buffer_len, 0);
  CHECK(!pending_write_callback_);

  if (!config_.rtt.is_positive() && !upload_throttle_) {
    int rv = wrapped_socket_->Write(
        buffer, buffer_len,
        base::BindOnce(&DelayedStreamSocket::DidCompletePassthroughIO,
                       weak_factory_.GetWeakPtr(), std::move(callback)),
        traffic_annotation);
    if (rv > 0) {
      was_ever_used_ = true;
    }
    return rv;
  }

  // Reject Write on a disconnected inner socket synchronously (matches
  // Socket::Write's ERR_SOCKET_NOT_CONNECTED contract). The wrapper has
  // no read-buffer analog to preserve, so this is unconditional in the
  // shaping path.
  if (!wrapped_socket_->IsConnected()) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  auto data = buffer->span().first(static_cast<size_t>(buffer_len));
  int accepted = upload_buffer_.Push(data);

  // Queue this Write's annotation alongside the bytes it added to the
  // upload buffer. The drain path consumes the queue front-first and
  // never crosses an annotation boundary in a single inner Write, so a
  // partial-accept-then-second-Write sequence still drains each segment
  // under its originating caller's annotation.
  if (accepted > 0) {
    upload_annotations_.push_back(
        {static_cast<size_t>(accepted),
         MutableNetworkTrafficAnnotationTag(traffic_annotation)});
  }

  if (accepted == 0) {
    // Buffer full. Stash the IOBuffer and retry when space frees up. Keep
    // the annotation so OnUploadSpaceAvailable applies the originating
    // Write's annotation when it re-Push()es the stashed bytes (rather
    // than whichever annotation happened to be set last).
    pending_write_callback_ = std::move(callback);
    pending_write_buffer_ = buffer;
    pending_write_buffer_len_ = buffer_len;
    stashed_write_annotation_ =
        MutableNetworkTrafficAnnotationTag(traffic_annotation);
    return ERR_IO_PENDING;
  }

  if (accepted < buffer_len) {
    // Partial accept: report what we took. The caller will retry the rest.
    MaybeDrainUploadBuffer();
    was_ever_used_ = true;
    return accepted;
  }

  // All data accepted. Caller's Write completes only after the buffer drains
  // to the underlying socket. Store the pending state before starting the
  // drain so HandleInnerWriteResult can see the pending callback (and the
  // total accepted byte count) when the chunk finishes draining.
  //
  // Note: we deliberately do NOT retain `buffer` here even though
  // Socket::Write documents that the socket "acquires a reference to the
  // provided buffer" while the operation is pending. BottleneckBuffer::Push
  // has already copied the bytes into its own storage, so this wrapper
  // never re-reads from the caller's IOBuffer after Push returns. Callers
  // are still expected to hold their own ref per the contract, but nothing
  // in this implementation depends on it. The buffer-full path above DOES
  // retain the IOBuffer because OnUploadSpaceAvailable re-Push()es from it.
  //
  // Leaving `pending_write_buffer_` null here is load-bearing:
  // has_stashed_write() (used by HandleInnerWriteResult) keys off it to
  // decide whether all supplied bytes are already buffered. Do NOT set
  // `pending_write_buffer_ = buffer` here or that signal breaks.
  pending_write_callback_ = std::move(callback);
  pending_write_buffer_ = nullptr;
  pending_write_buffer_len_ = accepted;
  MaybeDrainUploadBuffer();
  return ERR_IO_PENDING;
}

void DelayedStreamSocket::MaybeDrainUploadBuffer() {
  if (inner_write_pending_ || !upload_buffer_.has_ready_data()) {
    return;
  }
  if (upload_throttle_) {
    RequestUploadThrottle();
    return;
  }
  // Unlimited upload throughput: pull and write as much as fits. Cap the
  // pull at the front annotation entry's bytes_remaining so the inner
  // Write never aggregates bytes from two different Write() callers'
  // annotations into one inner Write.
  CHECK(!upload_annotations_.empty());
  size_t max_pull = std::min(static_cast<size_t>(kInnerWriteBufferSize),
                             upload_annotations_.front().bytes_remaining);
  auto dest = inner_write_buffer_->span().first(max_pull);
  int pulled = upload_buffer_.Pull(dest);
  if (pulled > 0) {
    MutableNetworkTrafficAnnotationTag annotation =
        ConsumeUploadAnnotationFront(pulled);
    IssueInnerWrite(pulled, std::move(annotation));
  }
}

void DelayedStreamSocket::RequestUploadThrottle() {
  if (upload_throttle_pending_) {
    return;
  }
  // Cap the request at the front annotation entry's bytes_remaining so the
  // throttle grant we eventually receive corresponds to bytes covered by
  // a single Write() caller's annotation; see MaybeDrainUploadBuffer().
  CHECK(!upload_annotations_.empty());
  size_t max_ready = std::min(static_cast<size_t>(kInnerWriteBufferSize),
                              upload_annotations_.front().bytes_remaining);
  // GetReadyBytesAtFront returns size_t; RequestBytes takes int.
  int ready =
      base::checked_cast<int>(upload_buffer_.GetReadyBytesAtFront(max_ready));
  if (ready == 0) {
    return;
  }
  upload_throttle_pending_ = true;
  upload_throttle_cancellation_ = upload_throttle_->RequestBytes(
      ready, base::BindOnce(&DelayedStreamSocket::OnUploadThrottleReady,
                            weak_factory_.GetWeakPtr(), ready));
}

void DelayedStreamSocket::OnUploadThrottleReady(int granted_bytes) {
  upload_throttle_pending_ = false;
  auto dest =
      inner_write_buffer_->span().first(static_cast<size_t>(granted_bytes));
  int pulled = upload_buffer_.Pull(dest);
  if (pulled > 0) {
    MutableNetworkTrafficAnnotationTag annotation =
        ConsumeUploadAnnotationFront(pulled);
    IssueInnerWrite(pulled, std::move(annotation));
  }
}

MutableNetworkTrafficAnnotationTag
DelayedStreamSocket::ConsumeUploadAnnotationFront(int bytes) {
  CHECK_GT(bytes, 0);
  CHECK(!upload_annotations_.empty());
  PendingUploadAnnotation& front = upload_annotations_.front();
  CHECK_GE(front.bytes_remaining, static_cast<size_t>(bytes));
  MutableNetworkTrafficAnnotationTag annotation = front.annotation;
  front.bytes_remaining -= static_cast<size_t>(bytes);
  if (front.bytes_remaining == 0) {
    upload_annotations_.pop_front();
  }
  return annotation;
}

// Issues an inner write of `bytes` bytes from `inner_write_buffer_`. Handles
// partial writes by advancing `inner_write_offset_` and re-issuing until the
// chunk completes (or goes async). The scratch buffer is wrapped in a
// DrainableIOBuffer so partial writes do not copy. `annotation` is the
// traffic annotation of the originating Write() call whose bytes are in
// this chunk; it is retained for partial-write retries.
void DelayedStreamSocket::IssueInnerWrite(
    int bytes,
    MutableNetworkTrafficAnnotationTag annotation) {
  CHECK_GT(bytes, 0);
  inner_write_pulled_ = bytes;
  inner_write_offset_ = 0;
  current_inner_write_annotation_ = std::move(annotation);
  int rv = SubmitCurrentInnerWriteChunk();
  if (rv != ERR_IO_PENDING) {
    HandleInnerWriteResult(rv);
  }
}

int DelayedStreamSocket::SubmitCurrentInnerWriteChunk() {
  int remaining = inner_write_pulled_ - inner_write_offset_;
  CHECK_GT(remaining, 0);
  inner_write_pending_ = true;
  // DrainableIOBuffer wraps the scratch buffer with the current offset, so
  // we can re-issue the remainder without copying.
  auto chunk = base::MakeRefCounted<DrainableIOBuffer>(
      inner_write_buffer_, static_cast<size_t>(inner_write_pulled_));
  if (inner_write_offset_ > 0) {
    chunk->DidConsume(inner_write_offset_);
  }
  return wrapped_socket_->Write(
      chunk.get(), remaining,
      base::BindOnce(&DelayedStreamSocket::OnInnerWriteComplete,
                     weak_factory_.GetWeakPtr()),
      NetworkTrafficAnnotationTag(current_inner_write_annotation_));
}

void DelayedStreamSocket::OnInnerWriteComplete(int result) {
  HandleInnerWriteResult(result);
}

// Drives the upload pipeline forward after each inner-write completion.
// Loops to handle partial writes and sync drain continuation without
// recursion.
void DelayedStreamSocket::HandleInnerWriteResult(int result) {
  while (true) {
    inner_write_pending_ = false;

    int remaining = inner_write_pulled_ - inner_write_offset_;
    // An inner-socket Write that returns 0 for a positive-length chunk means
    // the socket is closed; without this guard the loop would re-issue the
    // same chunk forever. Map it to ERR_CONNECTION_CLOSED so the error path
    // below tears the pipeline down.
    // Zero bytes written for a non-zero buffer indicates connection closure.
    if (result == 0 && remaining > 0) {
      result = ERR_CONNECTION_CLOSED;
    }

    // Inner write failed. Surface the error to the caller (if any) and drop
    // any buffered upload; the socket is no longer writable.
    if (result < 0) {
      upload_buffer_.Reset();
      upload_annotations_.clear();
      inner_write_pulled_ = 0;
      inner_write_offset_ = 0;
      if (pending_write_callback_) {
        pending_write_buffer_ = nullptr;
        pending_write_buffer_len_ = 0;
        // Route through the WeakPtr-bound trampoline so Disconnect()/dtor
        // cancels this pending completion per the Socket contract
        // (consistent with the read paths).
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&DelayedStreamSocket::DispatchPendingCompletion,
                           weak_factory_.GetWeakPtr(),
                           /*keep_alive=*/scoped_refptr<IOBuffer>(),
                           std::move(pending_write_callback_), result));
      }
      return;
    }

    if (result < remaining) {
      // Partial write: advance the offset and re-issue from there.
      inner_write_offset_ += result;
      int rv = SubmitCurrentInnerWriteChunk();
      if (rv == ERR_IO_PENDING) {
        return;
      }
      result = rv;
      continue;
    }

    // Full chunk written. Reset offset state for the next chunk.
    inner_write_pulled_ = 0;
    inner_write_offset_ = 0;

    // If the buffer is drained and a Write was waiting for completion, fire
    // its callback now. Post the invocation so this completion never lands
    // on the consumer's stack while we are still inside Write() or any
    // other method on `this`.
    if (upload_buffer_.empty() && pending_write_callback_ &&
        !has_stashed_write()) {
      int write_result = pending_write_buffer_len_;
      pending_write_buffer_len_ = 0;
      // Route through the WeakPtr-bound trampoline so Disconnect()/dtor
      // cancels this pending completion per the Socket contract.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&DelayedStreamSocket::DispatchPendingCompletion,
                         weak_factory_.GetWeakPtr(),
                         /*keep_alive=*/scoped_refptr<IOBuffer>(),
                         std::move(pending_write_callback_), write_result));
      return;
    }

    if (!upload_buffer_.has_ready_data()) {
      return;
    }

    // More data ready. Throttle path is async; let MaybeDrainUploadBuffer
    // handle it. Unlimited-throughput path can pull and write inline.
    if (upload_throttle_) {
      MaybeDrainUploadBuffer();
      return;
    }
    // Cap the next pull at the front annotation's bytes_remaining (same
    // reason as MaybeDrainUploadBuffer): never aggregate two callers'
    // annotations into a single inner Write.
    CHECK(!upload_annotations_.empty());
    size_t max_pull = std::min(static_cast<size_t>(kInnerWriteBufferSize),
                               upload_annotations_.front().bytes_remaining);
    auto dest = inner_write_buffer_->span().first(max_pull);
    int pulled = upload_buffer_.Pull(dest);
    if (pulled == 0) {
      return;
    }
    current_inner_write_annotation_ = ConsumeUploadAnnotationFront(pulled);
    inner_write_pulled_ = pulled;
    inner_write_offset_ = 0;
    int rv = SubmitCurrentInnerWriteChunk();
    if (rv == ERR_IO_PENDING) {
      return;
    }
    result = rv;
  }
}

void DelayedStreamSocket::OnUploadDataReady() {
  if (!inner_write_pending_) {
    MaybeDrainUploadBuffer();
  }
}

void DelayedStreamSocket::OnUploadSpaceAvailable() {
  // Buffer freed space; retry the stashed write.
  if (!pending_write_callback_ || !has_stashed_write()) {
    return;
  }

  auto data = pending_write_buffer_->span().first(
      static_cast<size_t>(pending_write_buffer_len_));
  int accepted = upload_buffer_.Push(data);
  if (accepted == 0) {
    return;  // Still full; wait for more draining.
  }
  // Queue the stashed write's annotation alongside the just-Push()ed bytes.
  upload_annotations_.push_back(
      {static_cast<size_t>(accepted), stashed_write_annotation_});

  // Drain synchronously while keeping `pending_write_buffer_` non-null.
  // HandleInnerWriteResult's success branch is gated on
  // `!pending_write_buffer_`, so it will NOT consume our callback and
  // mis-report a full-accept completion. Its error branch, however, does
  // not check `pending_write_buffer_`, so a synchronous inner-write error
  // during this drain still routes through `pending_write_callback_` and
  // reports the failure to the original Write() caller.
  MaybeDrainUploadBuffer();

  if (!pending_write_callback_) {
    // Sync drain hit an error and HandleInnerWriteResult already fired
    // our callback with the error code; nothing more to do.
    return;
  }

  // Drain completed (or went async) without error. Move state out and
  // post the partial-accept completion. The freshly-pushed bytes will be
  // throttled (and timed) during their actual drain to the OS socket;
  // don't double-charge the throttle by re-timing the caller's completion
  // here. Route through the WeakPtr-bound trampoline so Disconnect()/dtor
  // cancels this pending completion per the Socket contract.
  CompletionOnceCallback callback = std::move(pending_write_callback_);
  pending_write_buffer_ = nullptr;
  pending_write_buffer_len_ = 0;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DelayedStreamSocket::DispatchPendingCompletion,
                                weak_factory_.GetWeakPtr(),
                                /*keep_alive=*/scoped_refptr<IOBuffer>(),
                                std::move(callback), accepted));
}

// --- Passthrough methods ---

int DelayedStreamSocket::SetReceiveBufferSize(int32_t size) {
  return wrapped_socket_->SetReceiveBufferSize(size);
}

int DelayedStreamSocket::SetSendBufferSize(int32_t size) {
  return wrapped_socket_->SetSendBufferSize(size);
}

void DelayedStreamSocket::SetDnsAliases(std::set<std::string> aliases) {
  wrapped_socket_->SetDnsAliases(std::move(aliases));
}

const std::set<std::string>& DelayedStreamSocket::GetDnsAliases() const {
  return wrapped_socket_->GetDnsAliases();
}

void DelayedStreamSocket::Disconnect() {
  connect_timer_.Stop();
  connect_pending_ = false;

  download_buffer_.Reset();
  upload_buffer_.Reset();
  inner_read_pending_ = false;
  processing_inner_read_ = false;
  inner_read_eof_ = false;
  inner_read_error_ = 0;
  pending_read_callback_.Reset();
  pending_read_buffer_ = nullptr;
  pending_read_buffer_len_ = 0;
  pending_read_if_ready_callback_.Reset();
  // Cancel() is needed *in addition to* InvalidateWeakPtrs() below: the
  // posted trampoline binds both a WeakPtr and the consumer's callback.
  // InvalidateWeakPtrs() stops the trampoline from running, but the bound
  // CompletionOnceCallback would still hold a (cancelled) reference until
  // the closure's natural lifetime expires. Cancel() releases it now.
  pending_read_if_ready_dispatch_.Cancel();
  pending_read_throttle_grant_ = 0;
  download_throttle_pending_ = false;
  // Cancel any in-flight throttle requests so a Disconnect-then-
  // Connect cycle doesn't leave tokens charged against the shared
  // throttle on behalf of a request whose completion we'll never honor.
  download_throttle_cancellation_ = BandwidthThrottle::CancellationHandle();
  inner_write_pending_ = false;
  inner_write_pulled_ = 0;
  inner_write_offset_ = 0;
  // Drop the queued/stashed annotations so a subsequent reconnect's first
  // inner write does not inherit the previous session's tag.
  current_inner_write_annotation_ = MutableNetworkTrafficAnnotationTag();
  stashed_write_annotation_ = MutableNetworkTrafficAnnotationTag();
  upload_annotations_.clear();
  pending_write_callback_.Reset();
  pending_write_buffer_ = nullptr;
  pending_write_buffer_len_ = 0;
  upload_throttle_pending_ = false;
  upload_throttle_cancellation_ = BandwidthThrottle::CancellationHandle();

  // Consumed by the next `Connect()` to reset `was_ever_used_`, matching
  // TCPClientSocket's `previously_disconnected_` pattern.
  previously_disconnected_ = true;

  weak_factory_.InvalidateWeakPtrs();
  wrapped_socket_->Disconnect();
}

bool DelayedStreamSocket::IsConnected() const {
  // While our own Connect is in flight, the wrapped socket may already
  // have completed its Connect but our caller hasn't been signalled OK
  // yet. Report not-yet-connected so callers respect the wrapper's
  // state machine. `connect_pending_` covers the window from Connect()
  // entry until our latency callback fires, which is strictly wider
  // than `connect_timer_.IsRunning()` (the timer is not yet armed
  // between Connect() and the wrapped socket's async completion).
  if (connect_pending_) {
    return false;
  }
  // Per StreamSocket::IsConnected: "True is returned if the connection
  // was terminated, but there is unread data in the incoming buffer."
  // The read-ahead sitting in `download_buffer_` counts as unread data,
  // so keep reporting connected until it drains, even after the wrapped
  // socket has closed.
  return wrapped_socket_->IsConnected() || !download_buffer_.empty();
}

bool DelayedStreamSocket::IsConnectedAndIdle() const {
  // See IsConnected(): don't lie about idle-ness while our own Connect
  // delay hasn't fired the consumer's callback yet.
  if (connect_pending_) {
    return false;
  }
  // Buffered read-ahead bytes that the consumer hasn't pulled yet count as
  // pending data on this StreamSocket; if we reported "idle" with those
  // bytes still sitting in our buffer a socket-pool reuse would deliver
  // stale data to the next transaction.
  return wrapped_socket_->IsConnectedAndIdle() && download_buffer_.empty();
}

int DelayedStreamSocket::GetPeerAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetPeerAddress(address);
}

int DelayedStreamSocket::GetLocalAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetLocalAddress(address);
}

const NetLogWithSource& DelayedStreamSocket::NetLog() const {
  return wrapped_socket_->NetLog();
}

bool DelayedStreamSocket::WasEverUsed() const {
  // Per `StreamSocket::WasEverUsed`, layered sockets must report whether
  // their *own* Read()/Write() methods were called, not the inner
  // transport's. The wrapper's Connect already exchanges latency-modelled
  // bytes with the inner socket (e.g. read-ahead), so delegating would
  // report "used" before the consumer had a chance to call us.
  return was_ever_used_;
}

NextProto DelayedStreamSocket::GetNegotiatedProtocol() const {
  return wrapped_socket_->GetNegotiatedProtocol();
}

bool DelayedStreamSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return wrapped_socket_->GetSSLInfo(ssl_info);
}

int64_t DelayedStreamSocket::GetTotalReceivedBytes() const {
  return wrapped_socket_->GetTotalReceivedBytes();
}

void DelayedStreamSocket::ApplySocketTag(const SocketTag& tag) {
  wrapped_socket_->ApplySocketTag(tag);
}

void DelayedStreamSocket::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  wrapped_socket_->SetBeforeConnectCallback(before_connect_callback);
}

int DelayedStreamSocket::ConfirmHandshake(CompletionOnceCallback callback) {
  return wrapped_socket_->ConfirmHandshake(std::move(callback));
}

std::optional<std::string_view>
DelayedStreamSocket::GetPeerApplicationSettings() const {
  return wrapped_socket_->GetPeerApplicationSettings();
}

void DelayedStreamSocket::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) const {
  wrapped_socket_->GetSSLCertRequestInfo(cert_request_info);
}

}  // namespace net
