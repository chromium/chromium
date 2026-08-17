// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DELAYED_DATAGRAM_SOCKET_H_
#define NET_SOCKET_DELAYED_DATAGRAM_SOCKET_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/delayed_socket_config.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/read_multiple_emulator.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class IOBuffer;
class IPEndPoint;

// A DatagramClientSocket wrapper that introduces socket-level network delay.
// Wraps an existing DatagramClientSocket and adds configurable latency and
// bandwidth constraints to Read() and Write() operations.
//
// ---- Read pipeline ----
//
// A continuous read-ahead loop drains packets from the inner socket. Each
// packet is admitted through the shared download `BandwidthThrottle` (if
// one is configured) before being tagged with
// `ready_at = throttle_grant + half_RTT`; with no throttle (i.e. unlimited
// download throughput), `ready_at = arrival + half_RTT`, where `arrival` is
// when the wrapper observes the inner Read complete (after the kernel and a
// task hop) - used as a surrogate for actual wire-arrival time. A caller Read()
// either delivers the front packet synchronously if it is ready, or stashes
// the caller's IOBuffer and callback and waits for the front packet's
// ready_at deadline. Packets arriving at the bottleneck simultaneously are
// thus serialized at the link rate (each takes its own size/rate of wire
// time), and only the propagation delay (half-RTT) is added on top - the
// same model as a real bottleneck link.
//
// ---- Write pipeline ----
//
// In shaping mode Write() returns synchronously with the byte count it
// accepted, never ERR_IO_PENDING. (In the unshaped passthrough mode -
// `rtt == 0` and no upload throttle - Write() instead forwards the inner
// socket's result verbatim, which may be ERR_IO_PENDING.) Internally the
// packet is copied into a send queue and a
// `BandwidthThrottle::RequestBytes()` is issued for it immediately, so all
// queued packets compete for tokens on the shared link in parallel and
// the throttle's FIFO serializes their admissions at the link rate. When
// the admission fires, the packet's `send_at = grant_time + half-RTT` is
// recorded and the inner Write is submitted at that time. Returning
// synchronously avoids the QuicChromiumPacketWriter "write-blocked" state
// that would otherwise serialize outbound packets at one per-RTT and
// collapse upload throughput. When `upload_throttle_` is null (i.e.
// unlimited upload throughput), each packet's only delay is half-RTT
// from its own Write() time. The send queue is bounded by
// `kMaxQueuedSendPackets`; overflowing Write()s are dropped while still
// reporting success - see "Scope limitations" below.
//
// ---- Latency model ----
//
// - Every Read() pays half-RTT + size/throughput.
// - Every Write() pays half-RTT + size/throughput.
// - All Connect() variants pass through (UDP "connect" is local socket setup;
//   actual network latency emerges from the first Write/Read exchange).
//
// A QUIC 1-RTT handshake naturally emerges:
//   Write(ClientHello) [half-RTT] + Read(ServerHello) [half-RTT] = 1 RTT
//
// Note: the read-ahead loop is *not* started by Connect(); it kicks off on
// the first Read(). This matches QUIC, which immediately issues a continuous
// Read loop after connecting, but consumers that connect without ever
// calling Read() will leave packets sitting in the kernel buffer.
//
// ---- Scope limitations ----
//
// - Inner-Write errors after the wrapper's synchronous success are treated
//   as ordinary UDP loss when they are per-packet (ERR_MSG_TOO_BIG) or
//   transient (ERR_NO_BUFFER_SPACE): the packet is dropped silently. Any
//   other error is fatal: the socket latches dead, the queue is discarded,
//   and the error is surfaced by the next Write() so consumers (e.g. QUIC's
//   write-error handling) observe persistent failures; Reads then fail with
//   ERR_SOCKET_NOT_CONNECTED. Shaped writes issued while not connected are
//   likewise rejected up front with ERR_SOCKET_NOT_CONNECTED rather than
//   enqueued (see Write()).
// - Zero-length datagrams are delivered as a Read() return of 0 - a valid
//   empty datagram. Datagram sockets have no close/EOF semantics, so 0 is
//   unambiguous here.
// - When the send queue saturates (more than `kMaxQueuedSendPackets`
//   in-flight at once), additional Write()s are dropped: the wrapper
//   returns the byte count (synchronous success) but does not enqueue or
//   transmit the packet. This is a deliberate trade-off - the alternatives
//   are fabricating ERR_WOULD_BLOCK (which QUIC has no way to handle for a
//   single datagram) or unbounded memory growth on a buggy sender.
//   Dropping matches UDP's normal loss model and avoids ever issuing a
//   second concurrent inner Write (which a real UDP socket forbids). QUIC
//   treats the drop as ordinary packet loss.
// - ReadMultiple delegates to Read() via ReadMultipleEmulator
//   (crbug.com/515333601) so an active QuicUseReadMultiple feature does
//   not crash; each emulated read still flows through the per-packet delay
//   pipeline. A native batched implementation can replace this later.
class NET_EXPORT DelayedDatagramSocket : public DatagramClientSocket {
 public:
  // `download_throttle` and `upload_throttle` may be null only when the
  // corresponding throughput is unlimited. Finite UDP throughput is
  // enforced by these shared throttles, allowing multiple sockets in a
  // throttling profile to coordinate bandwidth on the same simulated
  // link. This matches DelayedStreamSocket's invariant.
  DelayedDatagramSocket(std::unique_ptr<DatagramClientSocket> socket,
                        const DelayedSocketConfig& config,
                        scoped_refptr<BandwidthThrottle> download_throttle,
                        scoped_refptr<BandwidthThrottle> upload_throttle);

  DelayedDatagramSocket(const DelayedDatagramSocket&) = delete;
  DelayedDatagramSocket& operator=(const DelayedDatagramSocket&) = delete;

  ~DelayedDatagramSocket() override;

  // Socket implementation:
  int Read(IOBuffer* buffer,
           int buffer_len,
           CompletionOnceCallback callback) override;
  // Delegates to Read() via ReadMultipleEmulator (crbug.com/515333601) so an
  // active QuicUseReadMultiple feature does not crash. Each emulated read
  // still flows through the per-packet delay pipeline; a native batched
  // implementation can replace this later.
  base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer* buffer,
      size_t buffer_len,
      size_t max_message_size,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback) override;
  int Write(IOBuffer* buffer,
            int buffer_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // DatagramSocket implementation:
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override;
  void SetMsgConfirm(bool confirm) override;
  const NetLogWithSource& NetLog() const override;
  DscpAndEcn GetLastTos() const override;

  // DatagramClientSocket implementation:
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) override;
  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  void EnableRecvOptimization() override;
  int SetMulticastInterface(uint32_t interface_index) override;
  void SetIOSNetworkServiceType(int ios_network_service_type) override;
  void RegisterQuicConnectionClosePayload(base::span<uint8_t> payload) override;
  void UnregisterQuicConnectionClosePayload() override;

 private:
  struct QueuedPacket {
    QueuedPacket(std::vector<uint8_t> data,
                 base::TimeTicks ready_at,
                 DscpAndEcn tos);
    QueuedPacket(QueuedPacket&&);
    QueuedPacket& operator=(QueuedPacket&&);
    ~QueuedPacket();

    std::vector<uint8_t> data;
    base::TimeTicks ready_at;
    // ECN/TOS bits read from the inner socket for this specific packet,
    // snapshotted at arrival time. Returned by GetLastTos() when this
    // packet is delivered so QUIC's ECN/CC sees this packet's mark, not
    // a later read-ahead packet's.
    DscpAndEcn tos;
  };

  // Constructed on Write().
  struct PendingSend {
    PendingSend();
    PendingSend(PendingSend&&);
    PendingSend& operator=(PendingSend&&);
    ~PendingSend();

    scoped_refptr<IOBuffer> data;
    int data_len = 0;
    MutableNetworkTrafficAnnotationTag annotation;
    // When this packet is scheduled to leave the inner socket. Only valid
    // once the packet has been granted. Grant status is tracked by
    // `next_send_to_grant_` (which counts the granted front-most packets),
    // NOT by a sentinel value of this field - do not reintroduce a
    // `send_at == base::TimeTicks()` "ungranted" check.
    base::TimeTicks send_at;

    // Sender-side metadata snapshotted at Write() time so each packet
    // hits the wire with the state the application had configured when it
    // issued the write - not whatever state happens to be current when
    // the delayed wire send fires.
    //
    // `dscp`/`ecn` are the *resolved* effective TOS at Write() time (any
    // NO_CHANGE sentinel is resolved against the running effective state,
    // so these are concrete values). `apply_tos` is true only once the
    // application has configured TOS at least once; vanilla packets leave
    // it false so DrainOneWireSend skips SetTos.
    bool apply_tos = false;
    DiffServCodePoint dscp = DSCP_DEFAULT;
    EcnCodePoint ecn = ECN_DEFAULT;
    bool msg_confirm = false;
    SocketTag socket_tag;
  };

  // Half the configured RTT, used as one-way propagation delay.
  base::TimeDelta HalfRtt() const;

  // Async-connect completion trampoline: records `connected_` on an OK
  // result, then runs the consumer's callback.
  void OnConnectComplete(CompletionOnceCallback callback, int result);

  // ---- Read pipeline ----
  // Pump the read-ahead loop: while there's room in the receive queue, no
  // inner read is already in flight, and the download throttle isn't
  // queueing a previous packet, issue inner Reads and process their
  // results inline. Loops so that a burst of synchronous inner Reads
  // doesn't recursively grow the stack.
  void EnsureInnerReadInFlight() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Async-completion entry for an inner Read. Processes the result and
  // re-enters the pump.
  void OnInnerReadComplete(int result);
  // The non-looping body of an inner-Read completion: pushes the result
  // through the throttle queue (or directly to `receive_queue_` when no
  // throttle is configured) and wakes any pending consumer. Does NOT
  // restart the pump itself; callers of this helper are responsible for
  // re-entering `EnsureInnerReadInFlight()` afterward if more reads
  // should be issued.
  void ProcessInnerReadResult(int result)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Fires when the shared download throttle admits a previously-received
  // packet's bytes; finalizes the packet's `ready_at` (admission time +
  // half-RTT) and pushes it onto `receive_queue_`. Owns the packet data
  // and TOS snapshot across the async hop.
  void OnDownloadThrottleGranted(std::vector<uint8_t> data, DscpAndEcn tos);
  void TryDeliverNextPacket();
  void OnDeliverTimer();
  // WeakPtr-bound trampoline that holds the caller's IOBuffer ref until
  // the callback fires (matching the Socket contract) and self-cancels
  // after Close()/dtor.
  void DispatchPendingCompletion(scoped_refptr<IOBuffer> keep_alive,
                                 CompletionOnceCallback callback,
                                 int result);
  // Copies the front of `receive_queue_` into `dest_buffer` (clamped to
  // `dest_len`), pops the packet, and returns the byte count.
  int CopyFrontPacketInto(IOBuffer* dest_buffer, int dest_len)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // ---- Write pipeline ----
  void ScheduleNextWireSend() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Fires when the shared upload throttle admits the front send_queue_
  // packet's bytes; starts the half-RTT propagation timer, after which
  // DrainOneWireSend submits the inner Write.
  void OnUploadThrottleGranted();
  void OnSendTimer();
  void DrainOneWireSend() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnInnerWireSendComplete(int result);

  const std::unique_ptr<DatagramClientSocket> wrapped_socket_;
  const DelayedSocketConfig config_;
  // Shared bandwidth throttles, one per direction. Non-null iff the
  // corresponding throughput is finite (CHECKed in the constructor),
  // and may be shared with sibling DelayedStreamSocket /
  // DelayedDatagramSocket instances to coordinate one simulated link.
  const scoped_refptr<BandwidthThrottle> download_throttle_;
  const scoped_refptr<BandwidthThrottle> upload_throttle_;

  // Read pipeline state.
  const scoped_refptr<IOBuffer> inner_read_buffer_;
  bool inner_read_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool inner_read_closed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int inner_read_error_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  // One-shot recoverable inner-read error (e.g. ERR_MSG_TOO_BIG) awaiting
  // delivery to the caller. Unlike `inner_read_closed_` this does not latch
  // the socket closed: it pauses read-ahead until the caller consumes the
  // error, then read-ahead resumes and re-reads the inner socket.
  int recoverable_read_error_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::circular_deque<QueuedPacket> receive_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Pending Read state. `pending_read_buffer_` holds the caller's IOBuffer
  // for the duration of ERR_IO_PENDING, satisfying the Socket buffer-
  // reference contract.
  scoped_refptr<IOBuffer> pending_read_buffer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  int pending_read_buffer_len_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  CompletionOnceCallback pending_read_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer deliver_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // In-flight `download_throttle_->RequestBytes()` request. Held so that
  // Close()/dtor cancels the request and releases its tokens back to
  // sibling sockets sharing the link, instead of leaving the throttle to
  // charge the tokens for a packet we will never deliver.
  BandwidthThrottle::CancellationHandle download_throttle_request_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // True while a download-throttle RequestBytes() is queued and we are
  // waiting for its grant. Gates `EnsureInnerReadInFlight()` so the
  // read-ahead pipeline serializes through the throttle and never piles
  // up multiple in-flight requests against the same shared link.
  bool download_throttle_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // TOS of the packet most recently delivered to the consumer via Read().
  // Returned by GetLastTos(); see QueuedPacket::tos for the rationale.
  // Defaults to the wrapped socket's no-packet-yet value (ToS byte 0 ==
  // {DSCP_DEFAULT, ECN_DEFAULT}), matching UDPSocketPosix/Win's GetLastTos()
  // before any datagram is received. DSCP_NO_CHANGE / ECN_NO_CHANGE are
  // setter-only sentinels and must never leak through this receive-side API.
  DscpAndEcn last_delivered_tos_ GUARDED_BY_CONTEXT(sequence_checker_) = {
      DSCP_DEFAULT, ECN_DEFAULT};

  // Per-socket sender-side state the application can set at any time. Each
  // Write() snapshots these into PendingSend so the eventual wire send uses
  // whatever was configured at the call site, not whatever state is current
  // when the delayed wire send fires.
  //
  // TOS is tracked as a resolved *effective* value: SetTos(dscp, ecn) with a
  // NO_CHANGE sentinel leaves the corresponding effective component
  // unchanged, so a partial sentinel (e.g. SetTos(DSCP_NO_CHANGE, ecn)) is
  // resolved against the running effective state rather than against
  // whatever the inner socket happens to have at drain time.
  // `tos_configured_` stays false until the app configures TOS at least
  // once, so vanilla packets skip SetTos.
  bool tos_configured_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  DiffServCodePoint effective_dscp_ GUARDED_BY_CONTEXT(sequence_checker_) =
      DSCP_DEFAULT;
  EcnCodePoint effective_ecn_ GUARDED_BY_CONTEXT(sequence_checker_) =
      ECN_DEFAULT;
  bool current_msg_confirm_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  SocketTag current_socket_tag_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether the wrapped socket is currently connected: set on a successful
  // Connect*() and cleared by Close(). Shaped Write()s are rejected with
  // ERR_SOCKET_NOT_CONNECTED when false, because a queued write's eventual
  // inner-write failure would otherwise be silently dropped.
  bool connected_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // Fatal inner wire-write error observed after the wrapper's synchronous
  // Write() success (see OnInnerWireSendComplete). Once set, the socket is
  // dead (`connected_` is false) and the next Write() returns this error
  // instead of the generic ERR_SOCKET_NOT_CONNECTED.
  int latched_write_error_ GUARDED_BY_CONTEXT(sequence_checker_) = OK;

  // Write pipeline state. `send_queue_` holds packets the caller has handed
  // off; each is scheduled to actually leave the inner socket at its
  // `send_at`. With a shared throttle, `send_at` is filled in by the
  // throttle's grant callback; without one, it is computed at Write() time
  // as `now + half-RTT`.
  base::circular_deque<PendingSend> send_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer send_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool inner_write_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // In-flight `upload_throttle_->RequestBytes()` requests, FIFO-ordered to
  // match `send_queue_`'s front-to-back order. We issue one request per
  // Write() at submission time so all queued packets compete for the
  // shared link's tokens in parallel - the throttle's internal FIFO
  // serializes admissions at the link rate, modelling the bottleneck.
  // Issuing serially (one at a time, waiting for each inner Write to
  // finish) would incorrectly add a full half-RTT between every
  // consecutive write. Holding the handles keeps the requests alive
  // until they're admitted; dropping a handle cancels its request, so on
  // Close we destroy the whole deque to release outstanding tokens back
  // to sibling sockets on the same throttle.
  base::circular_deque<BandwidthThrottle::CancellationHandle>
      pending_upload_throttle_requests_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Number of front-most `send_queue_` packets whose `send_at` has been
  // set (either by the throttle's grant callback or, in the no-throttle
  // path, at Write() time). Lets `OnUploadThrottleGranted` find the next
  // ungranted packet in O(1) instead of scanning. Maintained by:
  //   * Write() throttle path: no change (new packet pushed at
  //     `next_send_to_grant_` is ungranted).
  //   * Write() no-throttle path: ++ (new packet pushed granted).
  //   * OnUploadThrottleGranted: assign send_at on the index it points
  //     at, then ++.
  //   * DrainOneWireSend (pop_front): -- (the popped packet was granted).
  //   * Close: reset to 0 (queue cleared).
  size_t next_send_to_grant_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Emulates ReadMultiple() by delegating to Read(); see crbug.com/515333601.
  ReadMultipleEmulator read_multiple_emulator_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DelayedDatagramSocket> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SOCKET_DELAYED_DATAGRAM_SOCKET_H_
