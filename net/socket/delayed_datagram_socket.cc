// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/delayed_datagram_socket.h"

#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Scratch buffer for inner UDP reads. Sized to comfortably hold a maximum
// UDP datagram.
constexpr int kInnerReadBufferSize = 64 * 1024;

// Maximum number of packets we'll buffer ahead of the consumer. This is the
// memory cap for read-ahead; in steady state it should rarely matter.
constexpr size_t kMaxQueuedPackets = 64;

// Cap on the send queue. A misbehaving sender (or a slow link) could
// otherwise grow the queue without bound, since Write() always returns
// synchronous success. When the queue is full the wrapper drops the packet
// (still reporting the byte count) rather than fabricate a queue-overflow
// error visible to QUIC; this matches UDP's normal loss model.
constexpr size_t kMaxQueuedSendPackets = 1024;

}  // namespace

DelayedDatagramSocket::QueuedPacket::QueuedPacket(std::vector<uint8_t> data,
                                                  base::TimeTicks ready_at,
                                                  DscpAndEcn tos)
    : data(std::move(data)), ready_at(ready_at), tos(tos) {}

DelayedDatagramSocket::QueuedPacket::QueuedPacket(QueuedPacket&&) = default;

DelayedDatagramSocket::QueuedPacket&
DelayedDatagramSocket::QueuedPacket::operator=(QueuedPacket&&) = default;

DelayedDatagramSocket::QueuedPacket::~QueuedPacket() = default;

DelayedDatagramSocket::PendingSend::PendingSend() = default;
DelayedDatagramSocket::PendingSend::PendingSend(PendingSend&&) = default;
DelayedDatagramSocket::PendingSend&
DelayedDatagramSocket::PendingSend::operator=(PendingSend&&) = default;
DelayedDatagramSocket::PendingSend::~PendingSend() = default;

DelayedDatagramSocket::DelayedDatagramSocket(
    std::unique_ptr<DatagramClientSocket> socket,
    const DelayedSocketConfig& config,
    scoped_refptr<BandwidthThrottle> download_throttle,
    scoped_refptr<BandwidthThrottle> upload_throttle)
    : wrapped_socket_(std::move(socket)),
      config_(config),
      download_throttle_(std::move(download_throttle)),
      upload_throttle_(std::move(upload_throttle)),
      inner_read_buffer_(
          base::MakeRefCounted<IOBufferWithSize>(kInnerReadBufferSize)) {
  // Finite throughput requires a shared throttle; a null throttle implies
  // unconstrained throughput (std::nullopt). Matches DelayedStreamSocket's
  // invariant so the factory's per-profile throttle wiring is uniform
  // across TCP and UDP.
  CHECK(download_throttle_ ||
        !config_.download_throughput_bytes_per_sec.has_value());
  CHECK(upload_throttle_ ||
        !config_.upload_throughput_bytes_per_sec.has_value());
}

DelayedDatagramSocket::~DelayedDatagramSocket() = default;

// --- Connect variants ---

int DelayedDatagramSocket::Connect(const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  // UDP "connect" is a local operation; actual network latency is modeled by
  // packet Write()/Read() delays.
  int rv = wrapped_socket_->Connect(address);
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

int DelayedDatagramSocket::ConnectUsingNetwork(handles::NetworkHandle network,
                                               const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  int rv = wrapped_socket_->ConnectUsingNetwork(network, address);
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

int DelayedDatagramSocket::ConnectUsingDefaultNetwork(
    const IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  int rv = wrapped_socket_->ConnectUsingDefaultNetwork(address);
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

int DelayedDatagramSocket::ConnectAsync(const IPEndPoint& address,
                                        CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  int rv = wrapped_socket_->ConnectAsync(
      address, base::BindOnce(&DelayedDatagramSocket::OnConnectComplete,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
  // A synchronous completion won't run OnConnectComplete, so record state
  // here.
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

int DelayedDatagramSocket::ConnectUsingNetworkAsync(
    handles::NetworkHandle network,
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  int rv = wrapped_socket_->ConnectUsingNetworkAsync(
      network, address,
      base::BindOnce(&DelayedDatagramSocket::OnConnectComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

int DelayedDatagramSocket::ConnectUsingDefaultNetworkAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!connected_);
  int rv = wrapped_socket_->ConnectUsingDefaultNetworkAsync(
      address, base::BindOnce(&DelayedDatagramSocket::OnConnectComplete,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
  if (rv == OK) {
    connected_ = true;
  }
  return rv;
}

void DelayedDatagramSocket::OnConnectComplete(CompletionOnceCallback callback,
                                              int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == OK) {
    connected_ = true;
  }
  std::move(callback).Run(result);
}

// --- Read pipeline ---

int DelayedDatagramSocket::Read(IOBuffer* buffer,
                                int buffer_len,
                                CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!pending_read_callback_);
  CHECK(buffer);
  CHECK_GT(buffer_len, 0);

  // Reject reads on a socket that isn't connected before touching the inner
  // socket: an unopened OS UDP socket can trip a release CHECK in the
  // underlying read path. Note that after a fatal wire-write error this
  // deliberately discards any already-buffered received packets: the socket
  // is dead (see OnInnerWireSendComplete), so Reads fail from here on.
  if (!connected_) {
    return ERR_SOCKET_NOT_CONNECTED;
  }

  // Unlike Write(), Read() has no zero-shaping fast passthrough even when
  // `config_.rtt` is zero and there is no download throttle. All reads flow
  // through the read-ahead pipeline so GetLastTos() can report the
  // per-packet ToS snapshot (see QueuedPacket::tos) and burst timing stays
  // uniform; a direct passthrough would bypass both.
  //
  // Drive the inner read-ahead loop so we keep draining packets from the
  // kernel even while the caller is between Reads.
  EnsureInnerReadInFlight();

  const base::TimeTicks now = base::TimeTicks::Now();
  if (!receive_queue_.empty() && now >= receive_queue_.front().ready_at) {
    return CopyFrontPacketInto(buffer, buffer_len);
  }

  pending_read_buffer_ = buffer;
  pending_read_buffer_len_ = buffer_len;
  pending_read_callback_ = std::move(callback);

  if ((inner_read_closed_ || recoverable_read_error_) &&
      receive_queue_.empty()) {
    // Surface the inner error/EOF (or a one-shot recoverable error)
    // asynchronously to honour the IO_PENDING contract: the callback -
    // stashed in `pending_read_callback_` above and consumed by
    // TryDeliverNextPacket - must fire after Read returns.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DelayedDatagramSocket::TryDeliverNextPacket,
                                  weak_factory_.GetWeakPtr()));
    return ERR_IO_PENDING;
  }

  if (!receive_queue_.empty()) {
    // Packet exists but isn't ready yet; the check above used the same
    // `now`, so `ready_at - now` is strictly positive. Schedule delivery.
    deliver_timer_.Start(FROM_HERE, receive_queue_.front().ready_at - now,
                         base::BindOnce(&DelayedDatagramSocket::OnDeliverTimer,
                                        weak_factory_.GetWeakPtr()));
  }
  return ERR_IO_PENDING;
}

void DelayedDatagramSocket::EnsureInnerReadInFlight()
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  // Iterative pump: process synchronous inner-Read completions inline
  // rather than via tail-recursion. Bound by `kMaxQueuedPackets` (cap)
  // and by the throttle's serialization (which sets
  // `download_throttle_pending_` after each push), so this loop is
  // self-bounded under load.
  while (true) {
    if (inner_read_pending_ || inner_read_closed_ || recoverable_read_error_) {
      // A recoverable error is awaiting delivery to the caller; pause
      // read-ahead until the caller consumes it (see ProcessInnerReadResult).
      return;
    }
    if (download_throttle_pending_) {
      // A throttle RequestBytes is in flight for the previous packet;
      // serialize the read-ahead pipeline through the throttle so we
      // don't pile up multiple unbounded requests on the shared link.
      return;
    }
    if (receive_queue_.size() >= kMaxQueuedPackets) {
      return;  // Backpressure - wait for consumer to drain.
    }
    inner_read_pending_ = true;
    int rv = wrapped_socket_->Read(
        inner_read_buffer_.get(), kInnerReadBufferSize,
        base::BindOnce(&DelayedDatagramSocket::OnInnerReadComplete,
                       weak_factory_.GetWeakPtr()));
    if (rv == ERR_IO_PENDING) {
      return;  // OnInnerReadComplete will resume the pump.
    }
    ProcessInnerReadResult(rv);
  }
}

void DelayedDatagramSocket::OnInnerReadComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ProcessInnerReadResult(result);
  // Resume the pump in case more inner Reads can be issued now (the
  // previous one just freed `inner_read_pending_`). If the read completed
  // with an error/close, ProcessInnerReadResult set `inner_read_closed_`
  // and this call is an intentional no-op: the pump exits immediately on
  // that guard. Do not add a defensive early-return here.
  EnsureInnerReadInFlight();
}

void DelayedDatagramSocket::ProcessInnerReadResult(int result)
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  inner_read_pending_ = false;

  if (result < 0) {
    if (result == ERR_MSG_TOO_BIG) {
      // The only recoverable per-datagram read error: the oversized datagram
      // is dropped but the socket stays usable, so don't latch the pump
      // closed. Surface it once, then pause read-ahead until the caller reads
      // again (re-reading immediately could spin on a sticky error). Matches
      // QUIC, which ignores ERR_MSG_TOO_BIG and keeps reading; all other
      // negative results are terminal and fall through to the latch below.
      recoverable_read_error_ = result;
      if (receive_queue_.empty()) {
        TryDeliverNextPacket();
      }
      return;
    }
    inner_read_closed_ = true;
    inner_read_error_ = result;
    // Wake any pending consumer to surface the error if the queue is empty.
    if (receive_queue_.empty()) {
      TryDeliverNextPacket();
    }
    return;
  }

  // result >= 0 is a datagram (result == 0 is a valid empty UDP datagram).
  auto inner_span =
      inner_read_buffer_->span().first(static_cast<size_t>(result));
  std::vector<uint8_t> data(std::from_range, inner_span);
  // Snapshot the inner socket's TOS as observed for this packet so
  // GetLastTos() can later report it independently of subsequent reads.
  DscpAndEcn packet_tos = wrapped_socket_->GetLastTos();

  // Shared-throttle path, but only for non-empty datagrams: RequestBytes()
  // requires a positive count, and an empty datagram consumes no link
  // capacity anyway, so bypass the throttle and enqueue it with propagation
  // delay only.
  if (download_throttle_ && result > 0) {
    // Request `result` bytes from the throttle. The grant time is when the
    // packet has "finished arriving" off the simulated bottleneck (sharing
    // capacity with sibling sockets); we then add half-RTT for propagation
    // in OnDownloadThrottleGranted. Setting `download_throttle_pending_` here
    // also causes the EnsureInnerReadInFlight pump to exit on its next
    // iteration, so the read-ahead loop serializes through the throttle.
    download_throttle_pending_ = true;
    download_throttle_request_ = download_throttle_->RequestBytes(
        result, base::BindOnce(
                    &DelayedDatagramSocket::OnDownloadThrottleGranted,
                    weak_factory_.GetWeakPtr(), std::move(data), packet_tos));
    return;
  }

  // No throttle (unlimited throughput, CHECKed in the constructor), or a
  // zero-length datagram: the only delay is half-RTT for propagation. The
  // EnsureInnerReadInFlight pump (our caller) will re-issue another inner
  // Read on its next iteration if there's still capacity.
  base::TimeTicks ready_at = base::TimeTicks::Now() + HalfRtt();
  receive_queue_.emplace_back(std::move(data), ready_at, packet_tos);
  TryDeliverNextPacket();
}

void DelayedDatagramSocket::OnDownloadThrottleGranted(std::vector<uint8_t> data,
                                                      DscpAndEcn tos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  download_throttle_pending_ = false;
  // Throttle admitted the packet's bytes; the packet has finished arriving
  // off the simulated bottleneck. Add half-RTT for propagation before it
  // becomes available to the consumer.
  base::TimeTicks ready_at = base::TimeTicks::Now() + HalfRtt();
  receive_queue_.emplace_back(std::move(data), ready_at, tos);
  TryDeliverNextPacket();
  EnsureInnerReadInFlight();
}

void DelayedDatagramSocket::TryDeliverNextPacket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_read_callback_) {
    return;
  }

  if (!receive_queue_.empty()) {
    const QueuedPacket& front = receive_queue_.front();
    base::TimeTicks now = base::TimeTicks::Now();
    if (now < front.ready_at) {
      // Not ready yet - schedule the timer.
      if (!deliver_timer_.IsRunning()) {
        deliver_timer_.Start(
            FROM_HERE, front.ready_at - now,
            base::BindOnce(&DelayedDatagramSocket::OnDeliverTimer,
                           weak_factory_.GetWeakPtr()));
      }
      return;
    }
    // Move the pending state out before CopyFrontPacketInto. That call
    // triggers read-ahead from the inner socket, which can synchronously
    // re-enter TryDeliverNextPacket via OnInnerReadComplete; clearing the
    // pending state first ensures the re-entry sees no Read to fulfill and
    // returns immediately.
    deliver_timer_.Stop();
    scoped_refptr<IOBuffer> dest_buffer = std::move(pending_read_buffer_);
    int dest_len = pending_read_buffer_len_;
    pending_read_buffer_len_ = 0;
    CompletionOnceCallback callback = std::move(pending_read_callback_);
    int bytes = CopyFrontPacketInto(dest_buffer.get(), dest_len);
    // Post the consumer's callback through the WeakPtr-bound trampoline so
    // Close()/dtor cancels it (per the Socket contract) and so the IOBuffer
    // ref stays alive until the callback fires.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DelayedDatagramSocket::DispatchPendingCompletion,
                       weak_factory_.GetWeakPtr(), std::move(dest_buffer),
                       std::move(callback), bytes));
    return;
  }

  if (inner_read_closed_) {
    scoped_refptr<IOBuffer> dest_buffer = std::move(pending_read_buffer_);
    pending_read_buffer_len_ = 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DelayedDatagramSocket::DispatchPendingCompletion,
                       weak_factory_.GetWeakPtr(), std::move(dest_buffer),
                       std::move(pending_read_callback_), inner_read_error_));
    return;
  }

  if (recoverable_read_error_) {
    // Surface the one-shot recoverable error and clear it. Read-ahead resumes
    // on the caller's next Read() (the pump is no longer paused now that the
    // error is cleared), so the inner socket is re-read rather than staying
    // latched closed.
    int error = recoverable_read_error_;
    recoverable_read_error_ = 0;
    scoped_refptr<IOBuffer> dest_buffer = std::move(pending_read_buffer_);
    pending_read_buffer_len_ = 0;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&DelayedDatagramSocket::DispatchPendingCompletion,
                       weak_factory_.GetWeakPtr(), std::move(dest_buffer),
                       std::move(pending_read_callback_), error));
  }
}

void DelayedDatagramSocket::DispatchPendingCompletion(
    scoped_refptr<IOBuffer> /*keep_alive*/,
    CompletionOnceCallback callback,
    int result) {
  // `keep_alive` is intentionally unused - the parameter exists solely to
  // hold a reference to the consumer's IOBuffer across the posted-task hop
  // (per the Socket contract).
  std::move(callback).Run(result);
}

void DelayedDatagramSocket::OnDeliverTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TryDeliverNextPacket();
}

int DelayedDatagramSocket::CopyFrontPacketInto(IOBuffer* dest_buffer,
                                               int dest_len)
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  CHECK(!receive_queue_.empty());
  CHECK_GT(dest_len, 0);
  QueuedPacket& front = receive_queue_.front();
  // Record this packet's TOS so GetLastTos() returns it after delivery, even
  // when the datagram is too big and gets dropped below.
  last_delivered_tos_ = front.tos;
  int result = static_cast<int>(front.data.size());
  if (result > dest_len) {
    // Datagram larger than the caller's buffer: return ERR_MSG_TOO_BIG and
    // drop it, as real UDP does, rather than delivering a silently truncated
    // packet.
    result = ERR_MSG_TOO_BIG;
  } else {
    dest_buffer->span()
        .first(front.data.size())
        .copy_from(base::span(front.data));
  }
  receive_queue_.pop_front();
  // We just freed a slot; keep the kernel pipeline primed.
  EnsureInnerReadInFlight();
  return result;
}

base::expected<DatagramsMetadata, Error> DelayedDatagramSocket::ReadMultiple(
    IOBuffer* buffer,
    size_t buffer_len,
    size_t max_message_size,
    base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connected_) {
    return base::unexpected(ERR_SOCKET_NOT_CONNECTED);
  }
  // Delegate to Read() via the emulator (crbug.com/515333601) so an active
  // QuicUseReadMultiple feature doesn't crash. Each emulated read still flows
  // through the per-packet delay pipeline (the emulator calls our Read()).
  // TODO(crbug.com/496616821): implement native batch reads through the
  // per-packet delay queue for higher fidelity.
  return read_multiple_emulator_.ReadMultiple(
      buffer, buffer_len, max_message_size, std::move(callback));
}

// --- Write pipeline ---

int DelayedDatagramSocket::Write(
    IOBuffer* buffer,
    int buffer_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(buffer);
  CHECK_GT(buffer_len, 0);
  // The underlying UDP sockets reject a null callback; require it here too
  // so the contract is uniform across the shaping and passthrough paths.
  CHECK(!callback.is_null());

  // Reject writes on a socket that isn't connected before touching the inner
  // socket. This covers both the passthrough path below (which would
  // otherwise forward to an unopened OS socket) and the shaping path (whose
  // queued write's eventual inner-write failure would be silently dropped
  // after our synchronous success). If a previous queued write died with a
  // fatal wire error, surface that error rather than the generic code.
  if (!connected_) {
    return latched_write_error_ != OK ? latched_write_error_
                                      : ERR_SOCKET_NOT_CONNECTED;
  }

  // No-shaping passthrough: not in queue-shaping mode, so we forward the
  // inner socket's contract verbatim - synchronous return or ERR_IO_PENDING
  // followed by the caller's `callback`. The shaping path below intentionally
  // *always* returns synchronously and discards `callback`; the two return
  // disciplines are different on purpose.
  //
  // A null upload throttle is equivalent to "unlimited upload throughput"
  // by the constructor invariant; combined with `latency == 0`, there is
  // nothing to shape and we pass straight through.
  if (!config_.rtt.is_positive() && !upload_throttle_) {
    return wrapped_socket_->Write(buffer, buffer_len, std::move(callback),
                                  traffic_annotation);
  }

  // If the send queue is saturated, drop the packet (reporting synchronous
  // success) rather than grow it without bound. We must not pass the packet
  // straight to the inner socket: that could start a second concurrent inner
  // Write while a queued one is in flight, which datagram sockets disallow.
  // Dropping matches UDP's normal loss model.
  // TODO(crbug.com/496616821): count dropped vs. sent packets and record a
  // drop-rate histogram once this wrapper is wired into production.
  if (send_queue_.size() >= kMaxQueuedSendPackets) {
    return buffer_len;
  }

  // Copy the bytes into an owned IOBuffer so the caller can release theirs
  // immediately. We always return synchronously below, so `callback` is
  // discarded - per the Socket contract, a sync return means the operation
  // has completed and no callback should fire.
  auto owned =
      base::MakeRefCounted<IOBufferWithSize>(static_cast<size_t>(buffer_len));
  owned->span().copy_from(
      buffer->span().first(static_cast<size_t>(buffer_len)));

  // Snapshot send-side per-packet state at Write() time so each packet hits
  // the wire with the DSCP/ECN/msg-confirm/socket-tag the application had
  // configured at the call site, even if the application changes those
  // values before the delayed send fires.
  PendingSend send;
  send.data = std::move(owned);
  send.data_len = buffer_len;
  send.annotation = MutableNetworkTrafficAnnotationTag(traffic_annotation);
  send.apply_tos = tos_configured_;
  send.dscp = effective_dscp_;
  send.ecn = effective_ecn_;
  send.msg_confirm = current_msg_confirm_;
  send.socket_tag = current_socket_tag_;

  if (upload_throttle_) {
    // Issue the throttle request NOW (not lazily when this packet reaches
    // the front of the queue): the throttle's FIFO queue is the
    // bottleneck-link simulation, and packets queued back-to-back must
    // arrive at the bottleneck simultaneously so their wire-times
    // serialize at the link rate. Lazy/serialized issuance would instead
    // pay a full half-RTT propagation delay between each pair of
    // packets, collapsing throughput.
    //
    // `send.send_at` is left default-constructed here; the grant
    // callback fills it in at the slot pointed to by
    // `next_send_to_grant_`.
    pending_upload_throttle_requests_.push_back(upload_throttle_->RequestBytes(
        buffer_len,
        base::BindOnce(&DelayedDatagramSocket::OnUploadThrottleGranted,
                       weak_factory_.GetWeakPtr())));
    send_queue_.push_back(std::move(send));
  } else {
    // No throttle → unlimited throughput (CHECKed in the constructor); each
    // packet's only delay is half-RTT for propagation from its own Write()
    // time. Push as already-granted to keep the invariant that
    // `next_send_to_grant_` indexes the first ungranted packet.
    send.send_at = base::TimeTicks::Now() + HalfRtt();
    send_queue_.push_back(std::move(send));
    ++next_send_to_grant_;
  }

  if (!inner_write_pending_ && !send_timer_.IsRunning()) {
    ScheduleNextWireSend();
  }
  return buffer_len;
}

void DelayedDatagramSocket::ScheduleNextWireSend()
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  if (send_queue_.empty() || inner_write_pending_ || send_timer_.IsRunning()) {
    return;
  }
  // The front packet's `send_at` is only valid once its throttle grant has
  // fired. `next_send_to_grant_` counts the granted front-most packets, so a
  // zero count means the front packet has not been granted yet; wait for the
  // grant callback to set send_at (and bump the counter) and re-enter here.
  if (next_send_to_grant_ == 0) {
    return;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta wait = send_queue_.front().send_at - now;
  if (wait.is_negative()) {
    wait = base::TimeDelta();
  }
  send_timer_.Start(FROM_HERE, wait,
                    base::BindOnce(&DelayedDatagramSocket::OnSendTimer,
                                   weak_factory_.GetWeakPtr()));
}

void DelayedDatagramSocket::OnUploadThrottleGranted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Grants arrive FIFO from the throttle; the front handle in our deque
  // is the one that just admitted. Pop it (the request is complete -
  // dropping the handle is now a no-op).
  CHECK(!pending_upload_throttle_requests_.empty());
  pending_upload_throttle_requests_.pop_front();
  // Fill in `send_at` on the next ungranted packet. `next_send_to_grant_`
  // tracks that index exactly (see header comment), so the lookup is
  // O(1): grants are FIFO, Write() submissions are FIFO, and the
  // throttle preserves that ordering.
  CHECK_LT(next_send_to_grant_, send_queue_.size());
  send_queue_[next_send_to_grant_].send_at = base::TimeTicks::Now() + HalfRtt();
  ++next_send_to_grant_;
  ScheduleNextWireSend();
}

void DelayedDatagramSocket::OnSendTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DrainOneWireSend();
}

void DelayedDatagramSocket::DrainOneWireSend()
    VALID_CONTEXT_REQUIRED(sequence_checker_) {
  if (send_queue_.empty() || inner_write_pending_) {
    return;
  }
  PendingSend send = std::move(send_queue_.front());
  send_queue_.pop_front();
  // The popped packet had its `send_at` set (ScheduleNextWireSend only
  // schedules the timer when the front packet is granted), so decrement
  // the counter to keep it pointing at the first ungranted packet in
  // the new (post-pop) `send_queue_`.
  CHECK_GT(next_send_to_grant_, 0u);
  --next_send_to_grant_;

  // Restore the per-packet sender-side state snapshot taken at Write()
  // time so each packet hits the wire with the DSCP/ECN/msg_confirm/tag
  // the application had configured at the call site, even if the
  // application has since mutated those values.
  //
  // `apply_tos` is set iff the application configured TOS at least once; the
  // snapshotted `dscp`/`ecn` are already resolved to concrete effective
  // values (no NO_CHANGE sentinels). Vanilla packets (apply_tos == false)
  // skip SetTos so we don't churn the inner socket's state.
  //
  // msg_confirm and SocketTag are *not* skipped on their defaults: `false`
  // and a default-constructed SocketTag are real states the inner socket
  // must be put into. Skipping them would leave a previous packet's
  // SetMsgConfirm(true) / non-default tag set on the inner socket, and a
  // subsequent packet that wants the default would silently inherit the
  // wrong state.
  if (send.apply_tos) {
    wrapped_socket_->SetTos(send.dscp, send.ecn);
  }
  wrapped_socket_->SetMsgConfirm(send.msg_confirm);
  wrapped_socket_->ApplySocketTag(send.socket_tag);

  inner_write_pending_ = true;
  // `send` (and with it our scoped_refptr) is destroyed at scope end even
  // when this write completes asynchronously; `wrapped_socket_` retains
  // `send.data` per the Socket contract (IOBuffers are reference-counted).
  int rv = wrapped_socket_->Write(
      send.data.get(), send.data_len,
      base::BindOnce(&DelayedDatagramSocket::OnInnerWireSendComplete,
                     weak_factory_.GetWeakPtr()),
      NetworkTrafficAnnotationTag(send.annotation));
  if (rv != ERR_IO_PENDING) {
    OnInnerWireSendComplete(rv);
  }
}

void DelayedDatagramSocket::OnInnerWireSendComplete(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_write_pending_ = false;
  if (result < 0 && result != ERR_MSG_TOO_BIG &&
      result != ERR_NO_BUFFER_SPACE) {
    // Fatal wire error (e.g. unreachable route). The application already saw
    // a synchronous success from Write(), so latch the error and tear down:
    // the next Write() surfaces it, letting consumers observe persistent
    // failures instead of sending into a black hole. Cancelling the queued
    // sends' outstanding throttle requests (by destroying their handles)
    // also releases their tokens back to sibling sockets on the shared link.
    connected_ = false;
    latched_write_error_ = result;
    send_timer_.Stop();
    send_queue_.clear();
    pending_upload_throttle_requests_.clear();
    next_send_to_grant_ = 0;
    return;
  }
  // Success, a per-packet error (ERR_MSG_TOO_BIG), or a transient one
  // (ERR_NO_BUFFER_SPACE): drop the packet as ordinary UDP loss and continue
  // draining the queue.
  if (!send_queue_.empty()) {
    ScheduleNextWireSend();
  }
}

// --- Passthrough: Socket ---

int DelayedDatagramSocket::SetReceiveBufferSize(int32_t size) {
  return wrapped_socket_->SetReceiveBufferSize(size);
}

int DelayedDatagramSocket::SetSendBufferSize(int32_t size) {
  return wrapped_socket_->SetSendBufferSize(size);
}

// --- Passthrough: DatagramSocket ---

void DelayedDatagramSocket::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  deliver_timer_.Stop();
  send_timer_.Stop();
  receive_queue_.clear();
  send_queue_.clear();
  pending_read_buffer_ = nullptr;
  pending_read_buffer_len_ = 0;
  pending_read_callback_.Reset();
  inner_read_pending_ = false;
  inner_read_closed_ = false;
  inner_read_error_ = 0;
  inner_write_pending_ = false;
  // Cancel any in-flight shared-throttle requests so a Close()-then-
  // reuse cycle doesn't leave tokens charged against the shared link
  // on behalf of a request whose completion we will never honour. Token
  // cancellation releases them to sibling sockets on the same throttle.
  download_throttle_request_ = BandwidthThrottle::CancellationHandle();
  download_throttle_pending_ = false;
  recoverable_read_error_ = 0;
  // Destroying the upload-request handles cancels their requests.
  pending_upload_throttle_requests_.clear();
  next_send_to_grant_ = 0;
  // Reset per-socket sender-side state so any (currently unsupported)
  // post-Close reuse of this wrapper would not leak the previous
  // session's TOS/tag/msg_confirm settings into the next one.
  tos_configured_ = false;
  effective_dscp_ = DSCP_DEFAULT;
  effective_ecn_ = ECN_DEFAULT;
  current_msg_confirm_ = false;
  current_socket_tag_ = SocketTag();
  connected_ = false;
  latched_write_error_ = OK;
  // Reset to the wrapped socket's no-packet-yet default (ToS 0), not the
  // setter-only NO_CHANGE sentinels, so a post-Close GetLastTos() matches
  // real UDP semantics.
  last_delivered_tos_ = {DSCP_DEFAULT, ECN_DEFAULT};
  weak_factory_.InvalidateWeakPtrs();
  wrapped_socket_->Close();
}

int DelayedDatagramSocket::GetPeerAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetPeerAddress(address);
}

int DelayedDatagramSocket::GetLocalAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetLocalAddress(address);
}

void DelayedDatagramSocket::UseNonBlockingIO() {
  wrapped_socket_->UseNonBlockingIO();
}

int DelayedDatagramSocket::SetDoNotFragment() {
  return wrapped_socket_->SetDoNotFragment();
}

int DelayedDatagramSocket::SetRecvTos() {
  return wrapped_socket_->SetRecvTos();
}

// Each setter below does two things:
//   1. Updates the effective per-socket state so the *next* Write() snapshot
//      picks up the value the application had configured at the call site.
//   2. Forwards to the inner socket so its setsockopt() actually takes
//      effect and the call's return value (e.g. ERR_NOT_IMPLEMENTED on
//      platforms without TOS support) reaches the caller.
//
// DrainOneWireSend will re-apply the per-packet snapshot before each
// delayed wire send, so the inner socket's current state between Writes
// is unimportant for shaping. The forwarding here is purely for the
// setter's contract.

int DelayedDatagramSocket::SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!connected_) {
    return ERR_SOCKET_NOT_CONNECTED;
  }
  if (!config_.rtt.is_positive() && !upload_throttle_) {
    // Unshaped passthrough: Writes go straight to the inner socket, so
    // forward the mutation and only record effective state it accepted.
    int rv = wrapped_socket_->SetTos(dscp, ecn);
    if (rv != OK) {
      return rv;
    }
  }
  // Shaping mode deliberately does NOT forward to the inner socket here.
  // Every queued packet snapshots the effective TOS at Write() time and
  // DrainOneWireSend applies that snapshot right before the packet's own
  // inner write, so a forward here could only mutate the TOS of an inner
  // write still in flight (a datagram the application sent under the
  // previous TOS).
  //
  // Resolve NO_CHANGE sentinels against the running effective state so a
  // later Write() snapshots the exact intended per-packet TOS. In particular
  // a partial sentinel like SetTos(DSCP_NO_CHANGE, explicit_ecn) leaves the
  // effective DSCP untouched instead of forwarding a sentinel that
  // DrainOneWireSend would resolve against stale inner state.
  if (dscp != DSCP_NO_CHANGE) {
    effective_dscp_ = dscp;
  }
  if (ecn != ECN_NO_CHANGE) {
    effective_ecn_ = ecn;
  }
  tos_configured_ = true;
  return OK;
}

void DelayedDatagramSocket::SetMsgConfirm(bool confirm) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_msg_confirm_ = confirm;
  // In shaping mode this is applied per packet by DrainOneWireSend from the
  // Write()-time snapshot; forwarding it now would instead mutate packets
  // already queued or in flight on the inner socket.
  if (!config_.rtt.is_positive() && !upload_throttle_) {
    wrapped_socket_->SetMsgConfirm(confirm);
  }
}

const NetLogWithSource& DelayedDatagramSocket::NetLog() const {
  return wrapped_socket_->NetLog();
}

DscpAndEcn DelayedDatagramSocket::GetLastTos() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Return the TOS snapshotted for the *last packet delivered to the
  // consumer*, not the inner socket's current value (which would belong
  // to a read-ahead packet that hasn't been delivered yet).
  return last_delivered_tos_;
}

// --- Passthrough: DatagramClientSocket ---

handles::NetworkHandle DelayedDatagramSocket::GetBoundNetwork() const {
  return wrapped_socket_->GetBoundNetwork();
}

void DelayedDatagramSocket::ApplySocketTag(const SocketTag& tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_socket_tag_ = tag;
  // See SetMsgConfirm(): in shaping mode the tag is applied per packet at
  // drain time, so it must not be forwarded while packets are queued or in
  // flight.
  if (!config_.rtt.is_positive() && !upload_throttle_) {
    wrapped_socket_->ApplySocketTag(tag);
  }
}

void DelayedDatagramSocket::EnableRecvOptimization() {
  wrapped_socket_->EnableRecvOptimization();
}

int DelayedDatagramSocket::SetMulticastInterface(uint32_t interface_index) {
  return wrapped_socket_->SetMulticastInterface(interface_index);
}

void DelayedDatagramSocket::SetIOSNetworkServiceType(
    int ios_network_service_type) {
  wrapped_socket_->SetIOSNetworkServiceType(ios_network_service_type);
}

void DelayedDatagramSocket::RegisterQuicConnectionClosePayload(
    base::span<uint8_t> payload) {
  wrapped_socket_->RegisterQuicConnectionClosePayload(payload);
}

void DelayedDatagramSocket::UnregisterQuicConnectionClosePayload() {
  wrapped_socket_->UnregisterQuicConnectionClosePayload();
}

// --- Private ---

base::TimeDelta DelayedDatagramSocket::HalfRtt() const {
  return config_.rtt / 2;
}

}  // namespace net
