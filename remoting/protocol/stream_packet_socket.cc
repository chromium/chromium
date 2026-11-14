// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_packet_socket.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/protocol/stun_tcp_packet_processor.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

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

webrtc::SocketAddress GetAddress(
    int (net::StreamSocket::*getAddressFn)(net::IPEndPoint*) const,
    const net::StreamSocket* socket) {
  net::IPEndPoint ip_endpoint;
  webrtc::SocketAddress address;
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
    webrtc::AsyncSocketPacketOptions options)
    : data(data), options(options) {}

StreamPacketSocket::PendingPacket::PendingPacket(const PendingPacket&) =
    default;

StreamPacketSocket::PendingPacket::PendingPacket(PendingPacket&&) = default;

StreamPacketSocket::PendingPacket::~PendingPacket() = default;

StreamPacketSocket::StreamPacketSocket() = default;

StreamPacketSocket::~StreamPacketSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool StreamPacketSocket::Init(std::unique_ptr<net::StreamSocket> socket,
                              StreamPacketProcessor* packet_processor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    const webrtc::SocketAddress& local_address,
    const webrtc::SocketAddress& remote_address,
    const webrtc::PacketSocketTcpOptions& tcp_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int tls_opts =
      tcp_options.opts & (webrtc::PacketSocketFactory::OPT_TLS |
                          webrtc::PacketSocketFactory::OPT_TLS_FAKE |
                          webrtc::PacketSocketFactory::OPT_TLS_INSECURE);

  if (tls_opts) {
    NOTIMPLEMENTED();
    return false;
  }

  if (!(tcp_options.opts & webrtc::PacketSocketFactory::OPT_STUN)) {
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
      net::AddressList(remote_endpoint),
      /* socket_performance_watcher= */ nullptr,
      /* network_quality_estimator= */ nullptr,
      /* net_log= */ nullptr, net::NetLogSource(),
      // Currently, there is no use case for targeting a network when using
      // StreamPacketSocket. This makes it safe to always target the default
      // network. Consider supporting a target network if a need arises.
      net::handles::kInvalidNetworkHandle);

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

webrtc::SocketAddress StreamPacketSocket::GetLocalAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAddress(&net::StreamSocket::GetLocalAddress, socket_.get());
}

webrtc::SocketAddress StreamPacketSocket::GetRemoteAddress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAddress(&net::StreamSocket::GetPeerAddress, socket_.get());
}

int StreamPacketSocket::Send(const void* data,
                             size_t data_size,
                             const webrtc::AsyncSocketPacketOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STATE_CONNECTED) {
    SetError(ENOTCONN);
    return -1;
  }

  if (data_size > kMaxSendBufferSize) {
    SetError(EMSGSIZE);
    return -1;
  }

  auto packet = packet_processor_->Pack(
      UNSAFE_TODO(base::span(static_cast<const uint8_t*>(data), data_size)));
  // Reject empty packets since StreamSocket::Write() requires a positive length
  // and BytesRemaining() > 0 is used below to detect unsent data.
  if (!packet || packet->size() == 0) {
    SetError(EINVAL);
    return -1;
  }
  auto buffer =
      base::MakeRefCounted<net::DrainableIOBuffer>(packet, packet->size());
  send_queue_.emplace_back(buffer, options);
  DoWrite();
  if (buffer->BytesRemaining() > 0 && state_ != STATE_CONNECTED) {
    DCHECK_NE(error_, 0);
    return -1;
  }
  return data_size;
}

int StreamPacketSocket::SendTo(
    const void* data,
    size_t data_size,
    const webrtc::SocketAddress& address,
    const webrtc::AsyncSocketPacketOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != STATE_CONNECTED || address != GetRemoteAddress()) {
    LOG(ERROR) << "The socket is not connected to the remote address.";
    SetError(ENOTCONN);
    return -1;
  }

  return Send(data, data_size, options);
}

int StreamPacketSocket::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  close_weak_factory_.InvalidateWeakPtrs();
  socket_.reset();
  state_ = STATE_CLOSED;
  send_queue_.clear();
  send_pending_ = false;
  read_buffer_.reset();
  return 0;
}

webrtc::AsyncPacketSocket::State StreamPacketSocket::GetState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return state_;
}

int StreamPacketSocket::GetOption(webrtc::Socket::Option option, int* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This method is never called by libjingle.
  NOTIMPLEMENTED();
  return -1;
}

int StreamPacketSocket::SetOption(webrtc::Socket::Option option, int value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return -1;
  }

  switch (option) {
    case webrtc::Socket::OPT_DONTFRAGMENT:
      NOTIMPLEMENTED();
      return -1;

    case webrtc::Socket::OPT_RCVBUF: {
      int net_error = socket_->SetReceiveBufferSize(value);
      return (net_error == net::OK) ? 0 : -1;
    }

    case webrtc::Socket::OPT_SNDBUF: {
      int net_error = socket_->SetSendBufferSize(value);
      return (net_error == net::OK) ? 0 : -1;
    }

    case webrtc::Socket::OPT_NODELAY:
      // Should call TCPClientSocket::SetNoDelay directly.
      NOTIMPLEMENTED();
      return -1;

    case webrtc::Socket::OPT_IPV6_V6ONLY:
      NOTIMPLEMENTED();
      return -1;

    case webrtc::Socket::OPT_DSCP:
      NOTIMPLEMENTED();
      return -1;

    case webrtc::Socket::OPT_RTP_SENDTIME_EXTN_ID:
      NOTIMPLEMENTED();
      return -1;

    default:
      NOTIMPLEMENTED() << "Unexpected socket option: " << option;
      return -1;
  }
}

int StreamPacketSocket::GetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_;
}

void StreamPacketSocket::SetError(int error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  error_ = error;
}

void StreamPacketSocket::OnConnectCompleted(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != net::OK) {
    CloseWithNetError(result);
    return;
  }
  state_ = STATE_CONNECTED;
  base::WeakPtr<StreamPacketSocket> self = weak_factory_.GetWeakPtr();
  NotifyConnect(this);
  if (!self || state_ != STATE_CONNECTED) {
    return;
  }
  DoRead();
}

void StreamPacketSocket::DoWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_do_write_) {
    return;
  }

  // Manage `in_do_write_` manually instead of using `base::AutoReset` because
  // an observer callback may destroy `this`, and `AutoReset` would write to
  // destroyed memory on unwind.
  in_do_write_ = true;
  base::WeakPtr<StreamPacketSocket> self = weak_factory_.GetWeakPtr();
  // `NotifyReadyToSend()` may re-entrantly enqueue more packets via `Send()`.
  // Keep draining until listeners stop enqueueing, a write goes pending, or the
  // socket closes.
  do {
    while (socket_ && !send_pending_ && !send_queue_.empty()) {
      PendingPacket& packet = send_queue_.front();
      int result = socket_->Write(
          packet.data.get(), packet.data->BytesRemaining(),
          base::BindOnce(&StreamPacketSocket::OnAsyncWriteCompleted,
                         base::Unretained(this)),
          kTrafficAnnotation);
      if (result == net::ERR_IO_PENDING) {
        send_pending_ = true;
        break;
      }
      if (!HandleWriteResult(result)) {
        break;
      }
    }

    if (!self) {
      return;
    }
    if (socket_ && !send_pending_ && send_queue_.empty()) {
      NotifyReadyToSend(this);
      if (!self) {
        return;
      }
    }
  } while (socket_ && !send_pending_ && !send_queue_.empty());
  in_do_write_ = false;
}

bool StreamPacketSocket::HandleWriteResult(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    const webrtc::SentPacketInfo sent_packet(packet.options.packet_id,
                                             webrtc::TimeMillis());
    send_queue_.pop_front();
    base::WeakPtr<StreamPacketSocket> self = weak_factory_.GetWeakPtr();
    NotifySentPacket(this, sent_packet);
    if (!self || state_ != STATE_CONNECTED) {
      return false;
    }
  }
  return true;
}

void StreamPacketSocket::OnAsyncWriteCompleted(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!in_do_write_);
  // Managed manually instead of via `base::AutoReset` in case `this` is
  // destroyed during `HandleWriteResult()` observer notifications.
  in_do_write_ = true;
  base::WeakPtr<StreamPacketSocket> self = weak_factory_.GetWeakPtr();
  bool ok = HandleWriteResult(result);
  if (!self) {
    return;
  }
  in_do_write_ = false;
  if (ok) {
    DoWrite();
  }
}

void StreamPacketSocket::DoRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    LOG(ERROR) << "Can't read more data since the socket no longer exists.";
    return;
  }
  while (socket_) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result < 0) {
    CloseWithNetError(result);
    return false;
  } else if (result == 0) {
    LOG(WARNING) << "Remote peer has shut down the socket.";
    CloseWithNetError(net::ERR_CONNECTION_CLOSED);
    return false;
  }

  base::WeakPtr<StreamPacketSocket> self = weak_factory_.GetWeakPtr();
  scoped_refptr<net::GrowableIOBuffer> read_buffer = read_buffer_;
  read_buffer->set_offset(read_buffer->offset() + result);
  base::span<uint8_t> span = read_buffer->span_before_offset();
  while (!span.empty()) {
    size_t bytes_consumed = 0;
    auto packet = packet_processor_->Unpack(span, &bytes_consumed);
    if (packet) {
      NotifyPacketReceived(webrtc::ReceivedIpPacket(
          packet->span(), GetRemoteAddress(),
          webrtc::Timestamp::Micros(webrtc::TimeMicros())));
      if (!self || state_ != STATE_CONNECTED) {
        return false;
      }
    }
    if (!bytes_consumed) {
      break;
    }
    span = span.subspan(bytes_consumed);
  }
  // We've consumed all complete packets from the buffer; now move any remaining
  // bytes to the head of the buffer and set offset to reflect this.
  read_buffer->everything().copy_prefix_from(span);
  read_buffer->set_offset(span.size());

  return true;
}

void StreamPacketSocket::OnAsyncReadCompleted(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HandleReadResult(result)) {
    DoRead();
  }
}

void StreamPacketSocket::CloseWithNetError(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(0, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);
  if (state_ == STATE_CLOSED) {
    return;
  }
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

  // Notify listeners asynchronously so active Send() or read callbacks unwind
  // first.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&StreamPacketSocket::RunNotifyClosed,
                                close_weak_factory_.GetWeakPtr(), error_));
}

void StreamPacketSocket::RunNotifyClosed(int error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, STATE_CLOSED);
  NotifyClosed(error);
}

}  // namespace remoting::protocol
