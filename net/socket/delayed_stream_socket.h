// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DELAYED_STREAM_SOCKET_H_
#define NET_SOCKET_DELAYED_STREAM_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "base/cancelable_callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/bottleneck_buffer.h"
#include "net/socket/delayed_socket_config.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class BandwidthThrottle;
class IOBuffer;
class IPEndPoint;

// A StreamSocket wrapper that introduces realistic socket-level network delay
// using an Intermediate Bottleneck Buffer model.
//
// ---- Architecture ----
//
// For READS (download):
//   OS Socket --read--> [Download BottleneckBuffer] --pull--> Consumer
//
//   Data is eagerly drained from the OS socket into a fixed-size intermediate
//   buffer. Each chunk is tagged with TimeAvailable = now + half_RTT. If
//   download throughput is finite, bytes remain in the buffer until the shared
//   BandwidthThrottle grants capacity. When the buffer fills, we stop reading
//   from the OS socket; this shrinks the TCP receive window and creates
//   natural backpressure that propagates to the sender, faithfully modeling a
//   slow link. Once the inner socket closes and all buffered read-ahead has
//   been delivered, Read() reports EOF by returning 0 exactly once (and any
//   inner error thereafter), matching the Socket contract.
//
// For WRITES (upload):
//   Producer --push--> [Upload BottleneckBuffer] --write--> OS Socket
//
//   Application writes are accepted into a fixed-size buffer. Each chunk is
//   tagged with TimeAvailable = now + half_RTT. If upload throughput is finite,
//   bytes remain in the buffer until the shared BandwidthThrottle grants
//   capacity, and only then are written to the OS socket. When the buffer
//   fills, Write() returns ERR_IO_PENDING until space is available; this
//   throttles the application at the source, preventing data from hitting the
//   OS send buffer at line rate.
//
// ---- Latency model ----
//
// - Connect() completion is delayed by one full RTT (TCP SYN + SYN-ACK).
//   When `latency` is positive, Connect() always returns ERR_IO_PENDING and
//   posts the result after one RTT, even when the wrapped socket completes
//   synchronously (including the StreamSocket::Connect contract clause that
//   re-Connect on an already-connected socket returns OK synchronously).
//   This deliberate drift makes the latency model uniform regardless of the
//   wrapped socket's state.
// - Every chunk entering the download buffer is tagged with half-RTT latency.
// - Every chunk entering the upload buffer is tagged with half-RTT latency.
// - Bandwidth limiting is handled by shared BandwidthThrottles, so bytes stay
//   in the per-socket BottleneckBuffers until the shared link grants capacity.
//   Finite stream throughput requires a shared throttle.
//
// When the inner socket completes synchronously but delay must be applied, the
// result is converted to asynchronous (ERR_IO_PENDING is returned and the
// callback is posted after the delay).
//
// Note: the read-ahead loop is *not* started by Connect(); it kicks off on
// the first Read(). Consumers that connect without ever calling Read() will
// leave bytes sitting in the kernel buffer until they do, mirroring the
// behavior of DelayedDatagramSocket.
//
// ---- Scope limitations ----
//
// - Read() and Write() synchronously return ERR_SOCKET_NOT_CONNECTED when
//   the inner socket is not connected. This is tighter than the plain
//   Socket contract (which would allow ERR_IO_PENDING followed by an
//   async error) so callers don't wait on an operation that cannot
//   complete. Buffered read-ahead bytes still drain after the inner has
//   closed; the check gates on `download_buffer_.empty()` for that
//   reason.
// - When both `latency == 0` and the corresponding shared throttle is
//   null, Read/Write become pure passthroughs to the inner socket: no
//   read-ahead loop, no BottleneckBuffer usage in that direction, no
//   annotation queue, and no wrapper-owned callback trampoline. This is
//   the wrapper's "off" state for a given direction; the shared
//   BandwidthThrottle wiring is what turns shaping on.
// - Inner-Write errors on already-committed bytes from a synchronous
//   partial-accept Write are silently dropped: the caller has already
//   seen a positive sync return, so there is no pending completion
//   callback to fire. The consumer discovers the failure on its next
//   Write attempt (which surfaces ERR_SOCKET_NOT_CONNECTED when the
//   inner has disconnected).
// - Disconnect() drops any upload bytes still held in the delay buffer that
//   have not yet been written to the OS socket. With a plain TCPClientSocket,
//   bytes accepted by Write() before Disconnect() are usually still delivered
//   to the remote host; DelayedStreamSocket cannot make that guarantee for
//   still-delayed bytes because they never reached the kernel. Faithfully
//   modeling in-flight delivery would be disproportionately complex and does
//   not matter for the client protocols we use, so we simply drop them.
class NET_EXPORT DelayedStreamSocket : public StreamSocket {
 public:
  // `download_throttle` and `upload_throttle` may be null only when the
  // corresponding throughput is unlimited. Finite stream throughput is
  // enforced by these shared throttles.
  DelayedStreamSocket(std::unique_ptr<StreamSocket> stream_socket,
                      const DelayedSocketConfig& config,
                      scoped_refptr<BandwidthThrottle> download_throttle,
                      scoped_refptr<BandwidthThrottle> upload_throttle);

  DelayedStreamSocket(const DelayedStreamSocket&) = delete;
  DelayedStreamSocket& operator=(const DelayedStreamSocket&) = delete;

  ~DelayedStreamSocket() override;

  // Socket implementation:
  int Read(IOBuffer* buffer,
           int buffer_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buffer,
                  int buffer_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buffer,
            int buffer_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  void SetDnsAliases(std::set<std::string> aliases) override;
  const std::set<std::string>& GetDnsAliases() const override;

  // StreamSocket implementation:
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override;
  int ConfirmHandshake(CompletionOnceCallback callback) override;
  std::optional<std::string_view> GetPeerApplicationSettings() const override;
  void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) const override;

 private:
  // Runs `callback` with `result` once the modelled connect RTT elapses.
  // `connect_started` is the time Connect() was entered; the delay applied
  // is `config_.rtt` minus the time the inner connect already consumed
  // (clamped at zero), so the total modelled connect latency is one RTT.
  void ScheduleConnectCompletion(CompletionOnceCallback callback,
                                 base::TimeTicks connect_started,
                                 int result);
  // Fires when the connect timer elapses; clears `connect_pending_`
  // then runs the consumer's Connect completion callback.
  void OnConnectTimerFired(CompletionOnceCallback callback, int result);

  // (Re)binds the per-direction BottleneckBuffer notification callbacks to
  // freshly-allocated WeakPtrs. Called from the constructor and on every
  // Connect() so that a prior Disconnect() (which invalidates all weak
  // pointers) does not permanently break the wakeup path.
  void RebindBufferCallbacks();

  // ---- Read pipeline helpers ----
  bool IsReadPassthrough() const;
  bool IsInnerReadDone() const;
  void MaybeStartInnerRead();
  int ReadIntoCaller(IOBuffer* buffer, int buffer_len);
  void CompleteRead(int result);
  void CompleteReadIfReady(int result);

  // Posted via a WeakPtr binding so Disconnect()/dtor cancels pending
  // completions per the Socket contract. `keep_alive` keeps the caller's
  // IOBuffer alive for the duration of the pending completion (per the
  // Socket contract); pass an empty refptr when no IOBuffer is involved
  // (Write or ReadIfReady). The parameter is intentionally unused in the
  // body; it exists solely for its lifetime side effect.
  // Passthrough-completion trampoline: sets `was_ever_used_` on positive
  // results, then forwards the result to the consumer's callback. Used
  // by the Read/ReadIfReady/Write passthrough paths so an async positive
  // completion at the inner socket propagates to the wrapper's
  // WasEverUsed() bit.
  void DidCompletePassthroughIO(CompletionOnceCallback callback, int result);
  void DispatchPendingCompletion(scoped_refptr<IOBuffer> keep_alive,
                                 CompletionOnceCallback callback,
                                 int result);
  // Decrements `pending_read_throttle_grant_` by `bytes_consumed`, clamped
  // at zero. Preserves any unconsumed grant so subsequent reads can use it
  // instead of re-charging the shared throttle.
  void ConsumeDownloadGrant(int bytes_consumed);
  void StartInnerRead();
  void OnInnerReadComplete(int result);
  void ProcessInnerReadResult(int result);
  void OnDownloadDataReady();
  void OnDownloadSpaceAvailable();
  void TryFulfillRead();
  void TryFulfillReadIfReady();
  void TryFulfillPendingRead();
  bool EnsureDownloadGrant(int max_bytes);
  void RequestDownloadThrottle(int max_bytes);
  void OnDownloadThrottleReady(int granted_bytes);

  // ---- Write pipeline helpers ----
  // True while a Write()'s bytes are stashed in `pending_write_buffer_`
  // waiting for upload-buffer space; OnUploadSpaceAvailable owns that
  // completion. When false, every byte supplied to the pending Write() is
  // already in `upload_buffer_`, so HandleInnerWriteResult fires the
  // callback once the buffer drains. This is the single source of truth for
  // that distinction: do not mirror it into a separate flag.
  bool has_stashed_write() const { return pending_write_buffer_ != nullptr; }
  void MaybeDrainUploadBuffer();
  void RequestUploadThrottle();
  void OnUploadThrottleReady(int granted_bytes);
  // Issues an inner Write of `bytes` bytes pulled from `upload_buffer_` into
  // `inner_write_buffer_`, using `annotation` as the inner Write's traffic
  // annotation. The annotation is the one originally supplied by the
  // Write() caller whose bytes are in this chunk (see `upload_annotations_`).
  void IssueInnerWrite(int bytes,
                       MutableNetworkTrafficAnnotationTag annotation);
  // Consumes `bytes` from the front entry of `upload_annotations_` and
  // returns its annotation. Pops the front entry if its `bytes_remaining`
  // reaches zero. `bytes` must be > 0 and <= the front entry's
  // `bytes_remaining` (the pull paths cap their pull to enforce this).
  MutableNetworkTrafficAnnotationTag ConsumeUploadAnnotationFront(int bytes);
  // Submits the still-unwritten portion of the current scratch chunk
  // (`inner_write_pulled_ - inner_write_offset_` bytes starting at offset
  // `inner_write_offset_`) to the inner socket. Returns the inner Write rv.
  int SubmitCurrentInnerWriteChunk();
  void OnInnerWriteComplete(int result);
  void HandleInnerWriteResult(int result);
  void OnUploadDataReady();
  void OnUploadSpaceAvailable();

  // ---- State ----
  std::unique_ptr<StreamSocket> wrapped_socket_;
  const DelayedSocketConfig config_;
  scoped_refptr<BandwidthThrottle> download_throttle_;
  scoped_refptr<BandwidthThrottle> upload_throttle_;

  BottleneckBuffer download_buffer_;
  BottleneckBuffer upload_buffer_;
  scoped_refptr<IOBuffer> inner_read_buffer_;
  scoped_refptr<IOBuffer> inner_write_buffer_;

  // Inner-read pipeline state.
  bool inner_read_pending_ = false;
  bool processing_inner_read_ = false;
  bool inner_read_eof_ = false;
  int inner_read_error_ = 0;

  // Pending Read state.
  scoped_refptr<IOBuffer> pending_read_buffer_;
  int pending_read_buffer_len_ = 0;
  CompletionOnceCallback pending_read_callback_;

  // Pending ReadIfReady state (no IOBuffer per contract).
  CompletionOnceCallback pending_read_if_ready_callback_;
  // Wraps the posted completion of `pending_read_if_ready_callback_` so
  // CancelReadIfReady() can cancel it even after CompleteReadIfReady() has
  // moved the callback into a posted-task closure. Without this the
  // consumer's callback could still fire after CancelReadIfReady() returns,
  // violating the Socket::CancelReadIfReady() contract.
  base::CancelableOnceClosure pending_read_if_ready_dispatch_;

  // Download throttle state.
  int pending_read_throttle_grant_ = 0;
  bool download_throttle_pending_ = false;
  // Cancellation handle for the in-flight download RequestBytes() call.
  // Held so that destroying this socket cancels the request; otherwise
  // the pending request would charge tokens against the shared throttle,
  // then run a callback bound to an invalidated WeakPtr (no-op).
  // Cancellation releases the tokens to live sockets sharing the link.
  BandwidthThrottle::CancellationHandle download_throttle_cancellation_;

  // Inner-write pipeline state.
  bool inner_write_pending_ = false;
  // Bytes pulled from `upload_buffer_` into `inner_write_buffer_` for the
  // current write chunk, and how many of those have already been written to
  // the OS socket (in case of partial-write retries).
  int inner_write_pulled_ = 0;
  int inner_write_offset_ = 0;
  // Traffic annotation for the inner Write currently in flight. Set when
  // bytes are pulled from `upload_buffer_` into `inner_write_buffer_`;
  // partial-write retries (`SubmitCurrentInnerWriteChunk`) reuse this
  // value so a chunk re-issued by `DrainableIOBuffer` keeps the same
  // annotation as the original Write() that submitted the bytes.
  MutableNetworkTrafficAnnotationTag current_inner_write_annotation_;

  // FIFO record of the traffic annotation associated with each segment of
  // bytes still sitting in `upload_buffer_`. Maintained in lockstep with
  // Push()/Pull() of the buffer:
  //   * Each successful Push() pushes one entry with the originating
  //     Write()'s annotation and the bytes accepted.
  //   * Each Pull() into `inner_write_buffer_` is capped at the front
  //     entry's `bytes_remaining` and consumes those bytes (popping the
  //     entry on zero).
  // This guarantees that bytes from a Write() with annotation A never
  // drain to the OS socket under a later Write()'s annotation B, which
  // matters when a sync partial-accept lets a second Write() interleave
  // before the first's queued bytes have actually been written.
  struct PendingUploadAnnotation {
    size_t bytes_remaining;
    MutableNetworkTrafficAnnotationTag annotation;
  };
  base::circular_deque<PendingUploadAnnotation> upload_annotations_;
  // Annotation for `pending_write_buffer_`, the IOBuffer stashed when
  // Push() returned 0 (buffer full). Consumed by OnUploadSpaceAvailable()
  // when room frees up and the stashed bytes are re-Push()ed.
  MutableNetworkTrafficAnnotationTag stashed_write_annotation_;

  // Pending Write state.
  CompletionOnceCallback pending_write_callback_;
  scoped_refptr<IOBuffer> pending_write_buffer_;
  int pending_write_buffer_len_ = 0;

  // Upload throttle state.
  bool upload_throttle_pending_ = false;
  // See `download_throttle_cancellation_`.
  BandwidthThrottle::CancellationHandle upload_throttle_cancellation_;

  // Connect timer.
  base::OneShotTimer connect_timer_;

  // True from the moment `Connect()` is entered until the consumer's
  // Connect completion callback fires. Guards against re-entrant
  // `Connect()` and against `IsConnected()`/`IsConnectedAndIdle()`
  // reporting connected while the wrapped socket has already completed
  // its own Connect but our latency callback has not yet been
  // delivered. Subsumes (and is strictly wider than) the
  // `connect_timer_.IsRunning()` check.
  bool connect_pending_ = false;

  // Per `StreamSocket::WasEverUsed`, this tracks whether our own Read /
  // ReadIfReady / Write methods have ever *positively completed* (returned
  // >0 sync, or fired their consumer callback with >0). Matches the
  // TCPClientSocket convention: a Read that returns ERR_IO_PENDING or
  // fails does not flip this. Reset on reconnect via
  // `previously_disconnected_`, also matching TCPClientSocket.
  bool was_ever_used_ = false;
  // Set to true by `Disconnect()`; consumed by the next `Connect()` to
  // reset `was_ever_used_` (so a Disconnect-then-Connect cycle reports
  // per-session usage, not lifetime usage). Follows TCPClientSocket.
  bool previously_disconnected_ = false;

  base::WeakPtrFactory<DelayedStreamSocket> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_DELAYED_STREAM_SOCKET_H_
