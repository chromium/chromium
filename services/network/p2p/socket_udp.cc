// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_udp.h"

#include <tuple>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "net/base/backoff_entry.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/log/net_log_source.h"
#include "services/network/p2p/socket_throttler.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_interceptor.h"
#include "services/network/throttling/throttling_p2p_network_interceptor.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace {

// Frequently used type of service (ToS) values. We're using this enum to log
// failures of commonly used SetTos() arguments.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SetTosArguments {
  OTHER = 0,
  DSCP_OTHER_ECN_NOT_ECT = 1,
  DSCP_OTHER_ECN_ECT1 = 2,
  DSCP_CS0_ECN_NOT_ECT = 3,
  DSCP_CS0_ECN_ECT1 = 4,
  DSCP_CS1_ECN_NOT_ECT = 5,
  DSCP_CS1_ECN_ECT1 = 6,
  DSCP_AF41_ECN_NOT_ECT = 7,
  DSCP_AF41_ECN_ECT1 = 8,
  DSCP_AF42_ECN_NOT_ECT = 9,
  DSCP_AF42_ECN_ECT1 = 10,
  kMaxValue = DSCP_AF42_ECN_ECT1,
};

// UDP packets cannot be bigger than 64k.
const int kUdpReadBufferSize = 65536;
// Socket receive buffer size.
const int kUdpRecvSocketBufferSize = 65536;  // 64K
// Socket send buffer size.
const int kUdpSendSocketBufferSize = 65536;

constexpr net::BackoffEntry::Policy kSetTosBackoffPolicy = {
    0,          // Number of initial errors to ignore before backing off.
    100,        // Initial delay for exponential back-off in ms.
    2,          // Factor by which the delay will be multiplied.
    0.0,        // Fuzzing percentage. We're not using any fuzzing.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    false,      // Don't use initial delay.
};

// Defines set of transient errors. These errors are ignored when we get them
// from sendto() or recvfrom() calls.
//
// net::ERR_OUT_OF_MEMORY
//
// This is caused by ENOBUFS which means the buffer of the network interface
// is full.
//
// net::ERR_CONNECTION_RESET
//
// This is caused by WSAENETRESET or WSAECONNRESET which means the
// last send resulted in an "ICMP Port Unreachable" message.
struct {
  int code;
  const char* name;
} static const kTransientErrors[]{
    {net::ERR_ADDRESS_UNREACHABLE, "net::ERR_ADDRESS_UNREACHABLE"},
    {net::ERR_ADDRESS_INVALID, "net::ERR_ADDRESS_INVALID"},
    {net::ERR_ACCESS_DENIED, "net::ERR_ACCESS_DENIED"},
    {net::ERR_CONNECTION_RESET, "net::ERR_CONNECTION_RESET"},
    {net::ERR_OUT_OF_MEMORY, "net::ERR_OUT_OF_MEMORY"},
    {net::ERR_INTERNET_DISCONNECTED, "net::ERR_INTERNET_DISCONNECTED"}};

bool IsTransientError(int error) {
  for (const auto& transient_error : kTransientErrors) {
    if (transient_error.code == error)
      return true;
  }
  return false;
}

const char* GetTransientErrorName(int error) {
  for (const auto& transient_error : kTransientErrors) {
    if (transient_error.code == error)
      return transient_error.name;
  }
  return "";
}

std::unique_ptr<net::DatagramServerSocket> DefaultSocketFactory(
    net::NetLog* net_log) {
  net::UDPServerSocket* socket =
      new net::UDPServerSocket(net_log, net::NetLogSource());
#if BUILDFLAG(IS_WIN)
  socket->UseNonBlockingIO();
#endif

  return base::WrapUnique(socket);
}

webrtc::EcnMarking GetEcnMarking(net::DscpAndEcn tos) {
  switch (tos.ecn) {
    case net::ECN_NO_CHANGE:
      NOTREACHED();
    case net::ECN_NOT_ECT:
      return webrtc::EcnMarking::kNotEct;
    case net::ECN_ECT1:
      return webrtc::EcnMarking::kEct1;
    case net::ECN_ECT0:
      return webrtc::EcnMarking::kEct0;
    case net::ECN_CE:
      return webrtc::EcnMarking::kCe;
  }
}

SetTosArguments GetSetTosEnumForLogging(net::DiffServCodePoint dscp,
                                        net::EcnCodePoint ecn) {
  if (ecn == net::ECN_NOT_ECT) {
    switch (dscp) {
      case net::DSCP_CS0:
        return SetTosArguments::DSCP_CS0_ECN_NOT_ECT;
      case net::DSCP_CS1:
        return SetTosArguments::DSCP_CS1_ECN_NOT_ECT;
      case net::DSCP_AF41:
        return SetTosArguments::DSCP_AF41_ECN_NOT_ECT;
      case net::DSCP_AF42:
        return SetTosArguments::DSCP_AF42_ECN_NOT_ECT;
      default:
        return SetTosArguments::DSCP_OTHER_ECN_NOT_ECT;
    }
  } else if (ecn == net::ECN_ECT1) {
    switch (dscp) {
      case net::DSCP_CS0:
        return SetTosArguments::DSCP_CS0_ECN_ECT1;
      case net::DSCP_CS1:
        return SetTosArguments::DSCP_CS1_ECN_ECT1;
      case net::DSCP_AF41:
        return SetTosArguments::DSCP_AF41_ECN_ECT1;
      case net::DSCP_AF42:
        return SetTosArguments::DSCP_AF42_ECN_ECT1;
      default:
        return SetTosArguments::DSCP_OTHER_ECN_ECT1;
    }
  }

  return SetTosArguments::OTHER;
}

}  // namespace

namespace network {

P2PPendingPacket::P2PPendingPacket(
    const net::IPEndPoint& to,
    base::span<const uint8_t> content,
    const webrtc::AsyncSocketPacketOptions& options,
    uint64_t id)
    : to(to),
      data(base::MakeRefCounted<net::VectorIOBuffer>(content)),
      size(content.size()),
      packet_options(options),
      id(id) {}

P2PPendingPacket::P2PPendingPacket(const P2PPendingPacket& other) = default;
P2PPendingPacket::~P2PPendingPacket() = default;

P2PSocketUdp::P2PSocketUdp(
    Delegate* Delegate,
    mojo::PendingRemote<mojom::P2PSocketClient> client,
    mojo::PendingReceiver<mojom::P2PSocket> socket,
    P2PMessageThrottler* throttler,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    net::NetLog* net_log,
    const DatagramServerSocketFactory& socket_factory,
    std::optional<base::UnguessableToken> devtools_token)
    : P2PSocket(Delegate, std::move(client), std::move(socket), P2PSocket::UDP),
      set_tos_backoff_(&kSetTosBackoffPolicy),
      throttler_(throttler),
      traffic_annotation_(traffic_annotation),
      net_log_with_source_(
          net::NetLogWithSource::Make(net_log, net::NetLogSourceType::SOCKET)),
      throttling_token_(network::ScopedThrottlingToken::MaybeCreate(
          net_log_with_source_.source().id,
          devtools_token)),
      socket_factory_(socket_factory),
      interceptor_(ThrottlingController::GetP2PInterceptor(
          net_log_with_source_.source().id)) {
  if (interceptor_) {
    interceptor_->RegisterSocket(this);
  }
}

P2PSocketUdp::P2PSocketUdp(
    Delegate* Delegate,
    mojo::PendingRemote<mojom::P2PSocketClient> client,
    mojo::PendingReceiver<mojom::P2PSocket> socket,
    P2PMessageThrottler* throttler,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    net::NetLog* net_log,
    std::optional<base::UnguessableToken> devtools_token)
    : P2PSocketUdp(Delegate,
                   std::move(client),
                   std::move(socket),
                   throttler,
                   traffic_annotation,
                   net_log,
                   base::BindRepeating(&DefaultSocketFactory),
                   devtools_token) {}

P2PSocketUdp::~P2PSocketUdp() {
  if (interceptor_) {
    interceptor_->UnregisterSocket(this);
  }
}

void P2PSocketUdp::Init(
    const net::IPEndPoint& local_address,
    uint16_t min_port,
    uint16_t max_port,
    const P2PHostAndIPEndPoint& remote_address,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK(!socket_);
  DCHECK((min_port == 0 && max_port == 0) || min_port > 0);
  DCHECK_LE(min_port, max_port);

  socket_ = socket_factory_.Run(net_log());

  int result = -1;
  if (min_port == 0) {
    result = socket_->Listen(local_address);
  } else if (local_address.port() == 0) {
    for (unsigned port = min_port; port <= max_port && result < 0; ++port) {
      result = socket_->Listen(net::IPEndPoint(local_address.address(), port));
      if (result < 0 && port != max_port) {
        socket_ = socket_factory_.Run(net_log());
      }
    }
  } else if (local_address.port() >= min_port &&
             local_address.port() <= max_port) {
    result = socket_->Listen(local_address);
  }
  if (result < 0) {
    LOG(ERROR) << "bind() to " << local_address.address().ToString()
               << (min_port == 0
                       ? base::StringPrintf(":%d", local_address.port())
                       : base::StringPrintf(", port range [%d-%d]", min_port,
                                            max_port))
               << " failed: " << result;
    OnError();
    return;
  }

  // Setting recv socket buffer size.
  if (socket_->SetReceiveBufferSize(kUdpRecvSocketBufferSize) != net::OK) {
    LOG(WARNING) << "Failed to set socket receive buffer size to "
                 << kUdpRecvSocketBufferSize;
  }

  // Setting socket send buffer size.
  if (socket_->SetSendBufferSize(kUdpSendSocketBufferSize) != net::OK) {
    LOG(WARNING) << "Failed to set socket send buffer size to "
                 << kUdpSendSocketBufferSize;
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketUdp::Init(): unable to get local address: "
               << result;
    OnError();
    return;
  }
  VLOG(1) << "Local address: " << address.ToString();

  // NOTE: Remote address will be same as what renderer provided.
  client_->SocketCreated(address, remote_address.ip_address);

  recv_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(kUdpReadBufferSize);
  DoRead();
}

void P2PSocketUdp::DoRead() {
  while (true) {
    DCHECK(recv_buffer_);
    const int result = socket_->RecvFrom(
        recv_buffer_.get(), kUdpReadBufferSize, &recv_address_,
        base::BindOnce(&P2PSocketUdp::OnRecv, base::Unretained(this)));
    // If there's an error, this object is destroyed by the internal call to
    // P2PSocket::OnError, so do not reference this after `HandleReadResult`
    // returns false.
    if (!HandleReadResult(result)) {
      return;
    }
  }
}

void P2PSocketUdp::OnRecv(int result) {
  if (HandleReadResult(result))
    DoRead();
}

void P2PSocketUdp::MaybeDrainReceivedPackets(bool force) {
  if (pending_received_packets_.empty()) {
    return;
  }

  // Early drain pending received packets:
  // - If reaching maxmium allowed batching size for burst packets arrived.
  // - If reaching maxmium allowed batching buffering to mitigate impact on
  // latency.
  if (!force) {
    base::TimeDelta batching_buffering;
    if (pending_received_packets_.size() > 1) {
      batching_buffering = pending_received_packets_.back()->timestamp -
                           pending_received_packets_.front()->timestamp;
    }

    if (pending_received_packets_.size() < kUdpMaxBatchingRecvPackets &&
        batching_buffering < kUdpMaxBatchingRecvBuffering) {
      return;
    }
  }

  std::vector<mojom::P2PReceivedPacketPtr> received_packets;
  received_packets.swap(pending_received_packets_);

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "WebRTC.P2P.UDP.BatchingNumberOfReceivedPackets", received_packets.size(),
      1, kUdpMaxBatchingRecvPackets, kUdpMaxBatchingRecvPackets);

  TRACE_EVENT1("p2p", __func__, "number_of_packets", received_packets.size());
  client_->DataReceived(std::move(received_packets));

  // Release `IOBuffer` of received packets.
  std::vector<scoped_refptr<net::IOBuffer>>().swap(pending_received_buffers_);
}

bool P2PSocketUdp::HandleReadResult(int result) {
  if (result > 0) {
    auto data = recv_buffer_->first(static_cast<size_t>(result));

    if (!base::Contains(connected_peers_, recv_address_)) {
      P2PSocket::StunMessageType type;
      bool stun = GetStunPacketType(data, &type);
      if ((stun && IsRequestOrResponse(type))) {
        connected_peers_.insert(recv_address_);
      } else if (!stun || type == STUN_DATA_INDICATION) {
        LOG(ERROR) << "Received unexpected data packet from "
                   << recv_address_.ToString()
                   << " before STUN binding is finished.";
        return true;
      }
    }

    delegate_->DumpPacket(data, true);
    net::DscpAndEcn last_tos =
        socket_ == nullptr
            ? net::DscpAndEcn(net::DSCP_DEFAULT, net::ECN_DEFAULT)
            : socket_->GetLastTos();
    auto packet = mojom::P2PReceivedPacket::New(
        data, recv_address_,
        base::TimeTicks() + base::Nanoseconds(webrtc::TimeNanos()),
        GetEcnMarking(last_tos));

    if (interceptor_) {
      interceptor_->EnqueueReceive(std::move(packet), std::move(recv_buffer_),
                                   this);
    } else {
      // Save the packet to buffer and check if more packets available to batch
      // together. Socket is non-blocking IO, and that it immediately returns
      // 'ERR_IO_PENDING' if drained.
      pending_received_packets_.push_back(std::move(packet));
      pending_received_buffers_.push_back(std::move(recv_buffer_));
    }
    recv_buffer_ =
        base::MakeRefCounted<net::IOBufferWithSize>(kUdpReadBufferSize);

    MaybeDrainReceivedPackets(false);
  } else if (result == net::ERR_IO_PENDING) {
    MaybeDrainReceivedPackets(true);

    return false;
  } else if (result < 0 && !IsTransientError(result)) {
    MaybeDrainReceivedPackets(true);

    LOG(ERROR) << "Error when reading from UDP socket: " << result;
    OnError();
    return false;
  }

  return true;
}

bool P2PSocketUdp::DoSend(const P2PPendingPacket& packet) {
  int64_t send_time_us = webrtc::TimeMicros();

  if (!net::IsPortAllowedForIpEndpoint(packet.to)) {
    OnError();
    return false;
  }

  // The peer is considered not connected until the first incoming STUN
  // request/response. In that state the renderer is allowed to send only STUN
  // messages to that peer and they are throttled using the |throttler_|. This
  // has to be done here instead of Send() to ensure P2PMsg_OnSendComplete
  // messages are sent in correct order.
  if (!base::Contains(connected_peers_, packet.to)) {
    P2PSocket::StunMessageType type = P2PSocket::StunMessageType();
    bool stun = GetStunPacketType(packet.data->first(packet.size), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to "
                 << packet.to.ToString() << " before STUN binding is finished.";
      OnError();
      return false;
    }

    if (throttler_->DropNextPacket(packet.size) && !interceptor_) {
      VLOG(0) << "Throttling outgoing STUN message.";
      // The renderer expects P2PMsg_OnSendComplete for all packets it generates
      // and in the same order it generates them, so we need to respond even
      // when the packet is dropped.
      send_completions_.emplace_back(packet.id, packet.packet_options.packet_id,
                                     send_time_us / 1000);
      // Do not reset the socket.
      return true;
    }
  }

  TRACE_EVENT_BEGIN("p2p", "UdpAsyncSendTo", perfetto::Track(packet.id), "size",
                    packet.size);

  MaybeUpdateTos(
      static_cast<net::DiffServCodePoint>(packet.packet_options.dscp),
      packet.packet_options.ect_1 ? net::ECN_ECT1 : net::ECN_NOT_ECT);

  webrtc::ApplyPacketOptions(
      webrtc::ArrayView<uint8_t>(packet.data->bytes(), packet.size),
      packet.packet_options.packet_time_params, send_time_us);
  auto callback_binding = base::BindRepeating(
      &P2PSocketUdp::OnSend, base::Unretained(this), packet.id,
      packet.packet_options.packet_id, send_time_us / 1000);

  // TODO(crbug.com/40489281): Pass traffic annotation after
  // DatagramSocketServer is updated.
  int result = socket_->SendTo(packet.data.get(), packet.size, packet.to,
                               base::BindOnce(callback_binding));

  // sendto() may return an error, e.g. if we've received an ICMP Destination
  // Unreachable message. When this happens try sending the same packet again,
  // and just drop it if it fails again.
  if (IsTransientError(result)) {
    result = socket_->SendTo(packet.data.get(), packet.size, packet.to,
                             std::move(callback_binding));
  }

  if (result == net::ERR_IO_PENDING) {
    send_pending_ = true;
  } else {
    if (!HandleSendResult(packet.id, packet.packet_options.packet_id,
                          send_time_us / 1000, result)) {
      return false;
    }
  }

  delegate_->DumpPacket(packet.data->first(packet.size), false);

  return true;
}

void P2PSocketUdp::OnSend(uint64_t packet_id,
                          int32_t transport_sequence_number,
                          int64_t send_time_ms,
                          int result) {
  DCHECK(send_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  send_pending_ = false;

  if (!HandleSendResult(packet_id, transport_sequence_number, send_time_ms,
                        result)) {
    return;
  }

  // Send next packets if we have them waiting in the buffer.
  while (!send_queue_.empty() && !send_pending_) {
    P2PPendingPacket packet = send_queue_.front();
    send_queue_.pop_front();
    if (!DoSend(packet))
      return;
  }
}

bool P2PSocketUdp::HandleSendResult(uint64_t packet_id,
                                    int32_t transport_sequence_number,
                                    int64_t send_time_ms,
                                    int result) {
  // End the in-process "UdpAsyncSendTo" event.
  TRACE_EVENT_END("p2p", perfetto::Track(packet_id), "result", result);
  // End the "Send" event in the Global parent track - the corresponding
  // BEGIN is called in |P2PSocketClientImpl| in the renderer process.
  TRACE_EVENT_END("p2p", perfetto::Track::Global(packet_id), "result", result);
  if (result < 0) {
    if (!IsTransientError(result)) {
      LOG(ERROR) << "Error when sending data in UDP socket: " << result;
      OnError();
      return false;
    }
    VLOG(0) << "sendto() has failed twice returning a "
               " transient error "
            << GetTransientErrorName(result) << ". Dropping the packet.";
  }

  if (!interceptor_) {
    send_completions_.emplace_back(packet_id, transport_sequence_number,
                                   send_time_ms);
  }

  return true;
}

void P2PSocketUdp::Send(base::span<const uint8_t> data,
                        const P2PPacketInfo& packet_info) {
  TRACE_EVENT0("net", "P2PSocketUdp::Send");
  // If there's an error in SendPacket, `this` is destroyed by the internal call
  // to P2PSocket::OnError, so do not reference this after SendPacket returns
  // false.
  if (SendPacket(data, packet_info)) {
    ProcessSendCompletions();
  }
}

bool P2PSocketUdp::SendPacket(base::span<const uint8_t> data,
                              const P2PPacketInfo& packet_info) {
  if (data.size() > kMaximumPacketSize) {
    NOTREACHED();
  }
  if (interceptor_) {
    P2PPendingPacket packet(packet_info.destination, data,
                            packet_info.packet_options, packet_info.packet_id);
    interceptor_->EnqueueSend(std::move(packet), this);
    return true;
  }

  bool result = true;
  if (send_pending_) {
    send_queue_.push_back(P2PPendingPacket(packet_info.destination, data,
                                           packet_info.packet_options,
                                           packet_info.packet_id));
  } else {
    P2PPendingPacket packet(packet_info.destination, data,
                            packet_info.packet_options, packet_info.packet_id);
    result = DoSend(packet);
  }
  return result;
}

void P2PSocketUdp::SendBatch(
    std::vector<mojom::P2PSendPacketPtr> packet_batch) {
  TRACE_EVENT0("net", "P2PSocketUdp::SendBatch");
  for (auto& packet : packet_batch) {
    // If there's an error in SendPacket, this object is destroyed by the
    // internal call to P2PSocket::OnError, so do not reference this after
    // SendPacket returns false.
    if (!SendPacket(packet->data, packet->packet_info)) {
      return;
    }
  }
  ProcessSendCompletions();
}

void P2PSocketUdp::SendFromInterceptor(const P2PPendingPacket& packet) {
  if (send_pending_) {
    send_queue_.push_back(packet);
  } else {
    std::ignore = DoSend(packet);
  }
}

void P2PSocketUdp::SetOption(P2PSocketOption option, int32_t value) {
  switch (option) {
    case P2P_SOCKET_OPT_RCVBUF:
      socket_->SetReceiveBufferSize(value);
      break;
    case P2P_SOCKET_OPT_SNDBUF:
      socket_->SetSendBufferSize(value);
      break;
    case P2P_SOCKET_OPT_DSCP:
      socket_->SetDiffServCodePoint(static_cast<net::DiffServCodePoint>(value));
      break;
    case P2P_SOCKET_OPT_RECV_ECN:
      socket_->SetRecvTos();
      break;
    default:
      NOTREACHED();
  }
}

void P2PSocketUdp::ProcessSendCompletions() {
  TRACE_EVENT0("net", "P2PSocketUdp::ProcessSendCompletions");
  if (send_completions_.empty()) {
    return;
  }
  if (send_completions_.size() == 1) {
    client_->SendComplete(send_completions_[0]);
  } else {
    client_->SendBatchComplete(send_completions_);
  }
  send_completions_.clear();
}

void P2PSocketUdp::SendCompletionFromInterceptor(P2PSendPacketMetrics metrics) {
  client_->SendComplete(metrics);
}

void P2PSocketUdp::MaybeUpdateTos(net::DiffServCodePoint dscp,
                                  net::EcnCodePoint ecn) {
  bool dscp_changed = dscp != net::DSCP_NO_CHANGE && dscp != last_dscp_;
  bool ecn_changed = ecn != net::ECN_NO_CHANGE && ecn != last_ecn_;
  if (set_tos_backoff_.ShouldRejectRequest() ||
      (!dscp_changed && !ecn_changed)) {
    return;
  }

  int result = socket_->SetTos(dscp, ecn);
  if (result == net::OK) {
    if (dscp_changed) {
      last_dscp_ = dscp;
    }
    if (ecn_changed) {
      last_ecn_ = ecn;
    }
    // Don't throttle future attempts to set the ToS byte.
    set_tos_backoff_.Reset();
  } else if (!IsTransientError(result)) {
    // A non-transient error may mean that the OS does not support setting
    // the ToS byte we want. To avoid frequent costly retries, we throttle the
    // next attempt to call SetTos.
    base::UmaHistogramEnumeration("WebRTC.P2P.UDP.SetTosErrorCountByArgument",
                              GetSetTosEnumForLogging(dscp, ecn));
    set_tos_backoff_.InformOfRequest(false);
  }
}

void P2PSocketUdp::DisconnectInterceptor() {
  interceptor_ = nullptr;
}

void P2PSocketUdp::ReceiveFromInterceptor(mojom::P2PReceivedPacketPtr packet,
                                          scoped_refptr<net::IOBuffer> buffer) {
  pending_received_packets_.push_back(std::move(packet));
  pending_received_buffers_.push_back(std::move(buffer));
  MaybeDrainReceivedPackets(true);
}

}  // namespace network
