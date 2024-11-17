// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_packet_socket.h"

#include "base/functional/callback.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/stun_tcp_packet_processor.h"

namespace remoting::protocol {

namespace {

// Maximum buffer size accepted for calls to Send().
constexpr int kMaxSendBufferSize = 65536;

constexpr int kReadBufferSize = 4096;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("stream_packet_socket", R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "WebRTC TCP socket for Chrome Remote Desktop data transmission. "
            "Used only by the remote desktop host and mobile client apps. The "
            "API isn't exposed to the Chrome browser or any other third party "
            "entities."
          trigger:
            "Mobile client app initiating a Chrome Remote Desktop connection, "
            "or the remote desktop host accepting a connection request."
          data:
            "Chrome Remote Desktop session data, including video and input "
            "events."
          destination: OTHER
          destination_other:
            "The Chrome Remote Desktop client/host that the user/program is "
            "connecting to."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented. 'RemoteAccessHostClientDomainList' and "
            "'RemoteAccessHostDomainList' policies can limit the domains to "
            "which a connection can be made, but they cannot be used to block "
            "the request to all domains. Please refer to help desk for other "
            "approaches to manage this feature."
        })");

rtc::SocketAddress GetAddress(
    int (net::StreamSocket::*getAddressFn)(net::IPEndPoint*) const,
    const net::StreamSocket* socket) {
  net::IPEndPoint ip_endpoint;
  rtc::SocketAddress address;
  if (!socket) {
    LOG(WARNING) << "Socket does not exist. Empty address will be returned.";
    return address;
  }
  int result = (socket->*getAddressFn)(&ip_endpoint);
  if (result != net::OK) {
    LOG(ERROR) << "Failed to get address: " << result;
    return address;
  }
  bool success = webrtc::IPEndPointToSocketAddress(ip_endpoint, &address);
  if (!success) {
    LOG(ERROR) << "failed to convert IPEndPoint to Socket address";
  }
  return address;
}

}  // namespace

StreamPacketSocket::PendingPacket::PendingPacket(
    scoped_refptr<net::DrainableIOBuffer> data,
    rtc::PacketOptions options)
    : data(data), options(options) {}

StreamPacketSocket::PendingPacket::PendingPacket(const PendingPacket&) =
    default;

StreamPacketSocket::PendingPacket::PendingPacket(PendingPacket&&) = default;

StreamPacketSocket::PendingPacket::~PendingPacket() = default;

StreamPacketSocket::StreamPacketSocket() = default;

StreamPacketSocket::~StreamPacketSocket() = default;

bool StreamPacketSocket::Init(std::unique_ptr<net::StreamSocket> socket,
                              StreamPacketProcessor* packet_processor) {
  DCHECK(socket);
  DCHECK(packet_processor);
  socket_ = std::move(socket);
  packet_processor_ = packet_processor;
  state_ = STATE_CONNECTING;
  int result = socket_->Connect(base::BindOnce(
      &StreamPacketSocket::OnConnectCompleted, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    OnConnectCompleted(result);
  }
  return result == net::OK || result == net::ERR_IO_PENDING;
}

bool StreamPacketSocket::InitClientTcp(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::PacketSocketTcpOptions& tcp_options) {
  int tls_opts =
      tcp_options.opts & (rtc::PacketSocketFactory::OPT_TLS |
                          rtc::PacketSocketFactory::OPT_TLS_FAKE |
                          rtc::PacketSocketFactory::OPT_TLS_INSECURE);

  if (tls_opts) {
    NOTIMPLEMENTED();
    return false;
  }

  if (!(tcp_options.opts & rtc::PacketSocketFactory::OPT_STUN)) {
    // Currently only STUN/TURN packet is supported.
    // TODO(yuweih): Add support for P2P TCP connections.
    NOTIMPLEMENTED();
    return false;
  }

  net::IPEndPoint local_endpoint;
  if (!webrtc::SocketAddressToIPEndPoint(local_address, &local_endpoint)) {
    return false;
  }

  net::IPEndPoint remote_endpoint;
  if (!webrtc::SocketAddressToIPEndPoint(remote_address, &remote_endpoint)) {
    return false;
  }

  auto socket = std::make_unique<net::TCPClientSocket>(
      net::AddressList(remote_endpoint), nullptr, nullptr, nullptr,
      net::NetLogSource());

  int result = socket->Bind(local_endpoint);
  if (result != net::OK) {
    // Allow BindSocket to fail if we're binding to the ANY address, since this
    // is mostly redundant in the first place. The socket will be bound when we
    // call Connect() instead.
    if (local_address.IsAnyIP()) {
      LOG(WARNING) << "TCP bind failed with error " << result
                   << "; ignoring since socket is using 'any' address.";
    } else {
      LOG(WARNING) << "TCP bind failed with error " << result;
      return false;
    }
  }

  // Set TCP_NODELAY for improved performance.
  socket->SetNoDelay(true);

  return Init(std::move(socket), StunTcpPacketProcessor::GetInstance());
}

rtc::SocketAddress StreamPacketSocket::GetLocalAddress() const {
  return GetAddress(&net::StreamSocket::GetLocalAddress, socket_.get());
}

rtc::SocketAddress StreamPacketSocket::GetRemoteAddress() const {
  return GetAddress(&net::StreamSocket::GetPeerAddress, socket_.get());
}

int StreamPacketSocket::Send(const void* data,
                             size_t data_size,
                             const rtc::PacketOptions& options) {
  if (state_ != STATE_CONNECTED) {
    SetError(ENOTCONN);
    return -1;
  }

  if (data_size > kMaxSendBufferSize) {
    SetError(EMSGSIZE);
    return -1;
  }

  auto packet = packet_processor_->Pack(reinterpret_cast<const uint8_t*>(data),
                                        data_size);
  if (!packet) {
    SetError(EINVAL);
    return -1;
  }
  send_queue_.emplace_back(
      base::MakeRefCounted<net::DrainableIOBuffer>(packet, packet->size()),
      options);
  DoWrite();
  return data_size;
}

int StreamPacketSocket::SendTo(const void* data,
                               size_t data_size,
                               const rtc::SocketAddress& address,
                               const rtc::PacketOptions& options) {
  if (state_ != STATE_CONNECTED || address != GetRemoteAddress()) {
    LOG(ERROR) << "The socket is not connected to the remote address.";
    SetError(ENOTCONN);
    return -1;
  }

  return Send(data, data_size, options);
}

int StreamPacketSocket::Close() {
  socket_.reset();
  state_ = STATE_CLOSED;
  send_queue_.clear();
  send_pending_ = false;
  read_buffer_.reset();
  return 0;
}

rtc::AsyncPacketSocket::State StreamPacketSocket::GetState() const {
  return state_;
}

int StreamPacketSocket::GetOption(rtc::Socket::Option option, int* value) {
  // This method is never called by libjingle.
  NOTIMPLEMENTED();
  return -1;
}

int StreamPacketSocket::SetOption(rtc::Socket::Option option, int value) {
  if (!socket_) {
    NOTREACHED();
  }

  switch (option) {
    case rtc::Socket::OPT_DONTFRAGMENT:
      NOTIMPLEMENTED();
      return -1;

    case rtc::Socket::OPT_RCVBUF: {
      int net_error = socket_->SetReceiveBufferSize(value);
      return (net_error == net::OK) ? 0 : -1;
    }

    case rtc::Socket::OPT_SNDBUF: {
      int net_error = socket_->SetSendBufferSize(value);
      return (net_error == net::OK) ? 0 : -1;
    }

    case rtc::Socket::OPT_NODELAY:
      // Should call TCPClientSocket::SetNoDelay directly.
      NOTREACHED();

    case rtc::Socket::OPT_IPV6_V6ONLY:
      NOTIMPLEMENTED();
      return -1;

    case rtc::Socket::OPT_DSCP:
      NOTIMPLEMENTED();
      return -1;

    case rtc::Socket::OPT_RTP_SENDTIME_EXTN_ID:
      NOTIMPLEMENTED();
      return -1;

    default:
      NOTIMPLEMENTED() << "Unexpected socket option: " << option;
      return -1;
  }

  NOTREACHED();
}

int StreamPacketSocket::GetError() const {
  return error_;
}

void StreamPacketSocket::SetError(int error) {
  error_ = error;
}

void StreamPacketSocket::OnConnectCompleted(int result) {
  if (result != net::OK) {
    CloseWithNetError(result);
    return;
  }
  state_ = STATE_CONNECTED;
  SignalConnect(this);
  DoRead();
}

void StreamPacketSocket::DoWrite() {
  if (!socket_ || send_pending_ || send_queue_.empty()) {
    return;
  }

  while (!send_queue_.empty()) {
    PendingPacket& packet = send_queue_.front();
    if (packet.data->BytesConsumed() == 0) {
      // Only apply packet options when we are about to send the head of the
      // packet.
      packet_processor_->ApplyPacketOptions(packet.data->bytes(),
                                            packet.data->size(),
                                            packet.options.packet_time_params);
    }
    int result = socket_->Write(
        packet.data.get(), packet.data->BytesRemaining(),
        base::BindOnce(&StreamPacketSocket::OnAsyncWriteCompleted,
                       base::Unretained(this)),
        kTrafficAnnotation);
    if (result == net::ERR_IO_PENDING) {
      send_pending_ = true;
      return;
    }
    if (!HandleWriteResult(result)) {
      return;
    }
  }

  SignalReadyToSend(this);
}

bool StreamPacketSocket::HandleWriteResult(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  send_pending_ = false;
  if (result < 0) {
    CloseWithNetError(result);
    return false;
  }
  DCHECK(!send_queue_.empty());
  PendingPacket& packet = send_queue_.front();
  packet.data->DidConsume(result);
  if (packet.data->BytesRemaining() == 0) {
    // Pop the queue before SignalSentPacket just in case SignalSentPacket
    // ends up reentrant. This is a speculative fix for a hardening crash when
    // send_queue_.pop_front() was called after SignalSentPacket.
    const rtc::SentPacket sent_packet(packet.options.packet_id,
                                      rtc::TimeMillis());
    send_queue_.pop_front();
    SignalSentPacket(this, sent_packet);
  }
  return true;
}

void StreamPacketSocket::OnAsyncWriteCompleted(int result) {
  if (HandleWriteResult(result)) {
    DoWrite();
  }
}

void StreamPacketSocket::DoRead() {
  if (!socket_) {
    LOG(ERROR) << "Can't read more data since the socket no longer exists.";
    return;
  }
  while (true) {
    if (!read_buffer_.get()) {
      read_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
      read_buffer_->SetCapacity(kReadBufferSize);
    } else if (read_buffer_->RemainingCapacity() < kReadBufferSize) {
      // Make sure that we always have at least kReadBufferSize of
      // remaining capacity in the read buffer. Normally all packets
      // are smaller than kReadBufferSize, so this is not really
      // required.
      read_buffer_->SetCapacity(read_buffer_->capacity() + kReadBufferSize -
                                read_buffer_->RemainingCapacity());
    }
    int result =
        socket_->Read(read_buffer_.get(), read_buffer_->RemainingCapacity(),
                      base::BindOnce(&StreamPacketSocket::OnAsyncReadCompleted,
                                     base::Unretained(this)));
    if (result == net::ERR_IO_PENDING || !HandleReadResult(result)) {
      return;
    }
  }
}

bool StreamPacketSocket::HandleReadResult(int result) {
  if (result < 0) {
    CloseWithNetError(result);
    return false;
  } else if (result == 0) {
    LOG(WARNING) << "Remote peer has shut down the socket.";
    CloseWithNetError(net::ERR_CONNECTION_CLOSED);
    return false;
  }

  read_buffer_->set_offset(read_buffer_->offset() + result);
  base::span<uint8_t> span = read_buffer_->span_before_offset();
  while (!span.empty()) {
    size_t bytes_consumed = 0;
    auto packet =
        packet_processor_->Unpack(span.data(), span.size(), &bytes_consumed);
    if (packet) {
      NotifyPacketReceived(rtc::ReceivedPacket(
          rtc::MakeArrayView(packet->bytes(), packet->size()),
          GetRemoteAddress(), webrtc::Timestamp::Micros(rtc::TimeMicros())));
    }
    if (!bytes_consumed) {
      break;
    }
    span = span.subspan(bytes_consumed);
  }
  // We've consumed all complete packets from the buffer; now move any remaining
  // bytes to the head of the buffer and set offset to reflect this.
  if (!span.empty()) {
    read_buffer_->everything().copy_prefix_from(span);
    read_buffer_->set_offset(span.size());
  }

  return true;
}

void StreamPacketSocket::OnAsyncReadCompleted(int result) {
  if (HandleReadResult(result)) {
    DoRead();
  }
}

void StreamPacketSocket::CloseWithNetError(int net_error) {
  DCHECK_GT(0, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  LOG(ERROR) << "Stream socket received net error: " << net_error;
  switch (net_error) {
    case net::ERR_SOCKET_NOT_CONNECTED:
      error_ = ENOTCONN;
      break;
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_CLOSED:
      error_ = ECONNRESET;
      break;
    default:
      error_ = EINVAL;
  }

  Close();
  SignalClose(this, error_);
}

}  // namespace remoting::protocol
