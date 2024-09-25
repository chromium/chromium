// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_socket_factory.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/webrtc/net_address_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "remoting/base/logging.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/session_options_provider.h"
#include "remoting/protocol/socket_util.h"
#include "remoting/protocol/stream_packet_socket.h"
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/rtc_base/async_dns_resolver.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/net_helpers.h"
#include "third_party/webrtc/rtc_base/network/received_packet.h"
#include "third_party/webrtc/rtc_base/socket.h"

namespace remoting::protocol {

namespace {

// Size of the buffer to allocate for RecvFrom().
const int kReceiveBufferSize = 65536;

// Maximum amount of data in the send buffers. This is necessary to
// prevent out-of-memory crashes if the caller sends data faster than
// Pepper's UDP API can handle it. This maximum should never be
// reached under normal conditions.
const int kMaxSendBufferSize = 256 * 1024;

// Creates a UDP socket and make it listen at |local_address| and |port|.
// Returns nullptr if the socket fails to listen.
std::unique_ptr<net::UDPServerSocket> CreateUdpSocketAndListen(
    const net::IPAddress& local_address,
    uint16_t port) {
  auto socket =
      std::make_unique<net::UDPServerSocket>(nullptr, net::NetLogSource());
  int result = socket->Listen(net::IPEndPoint(local_address, port));
  if (result != net::OK) {
    socket.reset();
  }
  return socket;
}

class UdpPacketSocket : public rtc::AsyncPacketSocket {
 public:
  UdpPacketSocket();

  UdpPacketSocket(const UdpPacketSocket&) = delete;
  UdpPacketSocket& operator=(const UdpPacketSocket&) = delete;

  ~UdpPacketSocket() override;

  bool Init(const rtc::SocketAddress& local_address,
            uint16_t min_port,
            uint16_t max_port);

  // rtc::AsyncPacketSocket interface.
  rtc::SocketAddress GetLocalAddress() const override;
  rtc::SocketAddress GetRemoteAddress() const override;
  int Send(const void* data,
           size_t data_size,
           const rtc::PacketOptions& options) override;
  int SendTo(const void* data,
             size_t data_size,
             const rtc::SocketAddress& address,
             const rtc::PacketOptions& options) override;
  int Close() override;
  State GetState() const override;
  int GetOption(rtc::Socket::Option option, int* value) override;
  int SetOption(rtc::Socket::Option option, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 private:
  struct PendingPacket {
    PendingPacket(const void* buffer,
                  int buffer_size,
                  const net::IPEndPoint& address,
                  const rtc::PacketOptions& options);

    scoped_refptr<net::IOBufferWithSize> data;
    net::IPEndPoint address;
    bool retried = false;
    rtc::PacketOptions options;
  };

  void OnBindCompleted(int error);

  void DoSend();
  void OnSendCompleted(int result);

  void DoRead();
  void OnReadCompleted(int result);
  void HandleReadResult(int result);

  std::unique_ptr<net::UDPServerSocket> socket_
      GUARDED_BY_CONTEXT(thread_checker_);

  State state_ = STATE_CLOSED;
  int error_ = 0;

  rtc::SocketAddress local_address_;

  // Receive buffer and address are populated by asynchronous reads.
  scoped_refptr<net::IOBuffer> receive_buffer_;
  net::IPEndPoint receive_address_;

  bool send_pending_ GUARDED_BY_CONTEXT(thread_checker_) = false;
  std::list<PendingPacket> send_queue_ GUARDED_BY_CONTEXT(thread_checker_);
  int send_queue_size_ GUARDED_BY_CONTEXT(thread_checker_) = 0;

  THREAD_CHECKER(thread_checker_);

  // Cache a WeakPtr instance to prevent calling memory barrier functions for
  // each send callback.
  base::WeakPtr<UdpPacketSocket> weak_ptr_;
  base::WeakPtrFactory<UdpPacketSocket> weak_factory_{this};
};

UdpPacketSocket::PendingPacket::PendingPacket(const void* buffer,
                                              int buffer_size,
                                              const net::IPEndPoint& address,
                                              const rtc::PacketOptions& options)
    : data(base::MakeRefCounted<net::IOBufferWithSize>(buffer_size)),
      address(address),
      options(options) {
  memcpy(data->data(), buffer, buffer_size);
}

UdpPacketSocket::UdpPacketSocket() {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

UdpPacketSocket::~UdpPacketSocket() {
  Close();
}

bool UdpPacketSocket::Init(const rtc::SocketAddress& local_address,
                           uint16_t min_port,
                           uint16_t max_port) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_LE(min_port, max_port);
  net::IPEndPoint local_endpoint;
  if (!webrtc::SocketAddressToIPEndPoint(local_address, &local_endpoint)) {
    return false;
  }

  if (min_port == 0 && max_port == 0) {
    // Just listen to any port that is available.
    socket_ = CreateUdpSocketAndListen(local_endpoint.address(), 0u);
  } else {
    // Randomly pick a port to start trying with so that we will less likely
    // pick the same port for relay. TURN server doesn't allow allocating relay
    // session from the same port until the old session is timed out.
    uint32_t port_count = max_port - min_port + 1;
    uint32_t starting_offset = base::RandGenerator(port_count);
    for (uint32_t i = 0; i < port_count; i++) {
      uint16_t port = static_cast<uint16_t>(
          min_port + ((starting_offset + i) % port_count));
      DCHECK_LE(min_port, port);
      DCHECK_LE(port, max_port);
      socket_ = CreateUdpSocketAndListen(local_endpoint.address(), port);
      if (socket_) {
        break;
      }
    }
  }

  if (!socket_.get()) {
    // Failed to bind the socket.
    return false;
  }

  if (socket_->GetLocalAddress(&local_endpoint) != net::OK ||
      !webrtc::IPEndPointToSocketAddress(local_endpoint, &local_address_)) {
    return false;
  }

  state_ = STATE_BOUND;
  DoRead();

  return true;
}

rtc::SocketAddress UdpPacketSocket::GetLocalAddress() const {
  DCHECK_EQ(state_, STATE_BOUND);
  return local_address_;
}

rtc::SocketAddress UdpPacketSocket::GetRemoteAddress() const {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
}

int UdpPacketSocket::Send(const void* data,
                          size_t data_size,
                          const rtc::PacketOptions& options) {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
}

int UdpPacketSocket::SendTo(const void* data,
                            size_t data_size,
                            const rtc::SocketAddress& address,
                            const rtc::PacketOptions& options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_BOUND) {
    NOTREACHED();
  }

  if (error_ != 0) {
    return error_;
  }

  net::IPEndPoint endpoint;
  if (!webrtc::SocketAddressToIPEndPoint(address, &endpoint)) {
    return EINVAL;
  }

  if (send_queue_size_ >= kMaxSendBufferSize) {
    return EWOULDBLOCK;
  }

  send_queue_.emplace_back(data, data_size, endpoint, options);
  send_queue_size_ += data_size;

  DoSend();
  return data_size;
}

int UdpPacketSocket::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  state_ = STATE_CLOSED;
  socket_.reset();
  weak_ptr_.reset();
  return 0;
}

rtc::AsyncPacketSocket::State UdpPacketSocket::GetState() const {
  return state_;
}

int UdpPacketSocket::GetOption(rtc::Socket::Option option, int* value) {
  // This method is never called by libjingle.
  NOTIMPLEMENTED();
  return -1;
}

int UdpPacketSocket::SetOption(rtc::Socket::Option option, int value) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state_ != STATE_BOUND) {
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
      // OPT_NODELAY is only for TCP sockets.
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

int UdpPacketSocket::GetError() const {
  return error_;
}

void UdpPacketSocket::SetError(int error) {
  error_ = error;
}

void UdpPacketSocket::DoSend() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // SendTo() usually completes synchronously however if the socket is not able
  // to send, it will return ERR_IO_PENDING. In that case, we break out of the
  // send loop to allow it time to finish sending packets. Once the socket is
  // ready, it will call the OnSendCompleted callback at which point we can
  // start working through the pending packet queue again.
  while (!send_pending_ && !send_queue_.empty() && error_ == 0) {
    PendingPacket& packet = send_queue_.front();
    cricket::ApplyPacketOptions(
        packet.data->bytes(), packet.data->size(),
        packet.options.packet_time_params,
        (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds());
    int result = socket_->SendTo(
        packet.data.get(), packet.data->size(), packet.address,
        base::BindOnce(&UdpPacketSocket::OnSendCompleted, weak_ptr_));
    if (result != net::ERR_IO_PENDING) {
      OnSendCompleted(result);
    } else {
      send_pending_ = true;
    }
  }
}

void UdpPacketSocket::OnSendCompleted(int result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If |send_pending_| is true, that means OnSendCompleted was run via the
  // callback we provide to the socket because it is able to process send
  // packets again. In that case, we want to call DoSend() so that any packets
  // which were queued while the socket was busy will be sent immediately.
  bool run_from_callback = send_pending_;
  send_pending_ = false;

  if (result < 0) {
    SocketErrorAction action = GetSocketErrorAction(result);
    switch (action) {
      case SOCKET_ERROR_ACTION_FAIL:
        LOG(ERROR) << "Send failed on a UDP socket: " << result;
        error_ = EINVAL;
        return;

      case SOCKET_ERROR_ACTION_RETRY:
        // Retry resending only once.
        if (!send_queue_.front().retried) {
          send_queue_.front().retried = true;
          if (run_from_callback) {
            DoSend();
          }
          return;
        }
        break;

      case SOCKET_ERROR_ACTION_IGNORE:
        break;
    }
  }

  // Don't need to worry about partial sends because this is a datagram socket.
  send_queue_size_ -= send_queue_.front().data->size();

  // Speculative fix for the intermittent crashes we've seen in this method.
  // TODO: joedow - Rewrite this comment if popping from the queue before
  // signaling packet sent does indeed solve the intermittent crashes.
  const rtc::SentPacket sent_packet(send_queue_.front().options.packet_id,
                                    rtc::TimeMillis());
  send_queue_.pop_front();
  SignalSentPacket(this, sent_packet);
  if (run_from_callback) {
    DoSend();
  }
}

void UdpPacketSocket::DoRead() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  int result = 0;
  while (result >= 0) {
    receive_buffer_ =
        base::MakeRefCounted<net::IOBufferWithSize>(kReceiveBufferSize);
    result = socket_->RecvFrom(
        receive_buffer_.get(), kReceiveBufferSize, &receive_address_,
        base::BindOnce(&UdpPacketSocket::OnReadCompleted, weak_ptr_));
    HandleReadResult(result);
  }
}

void UdpPacketSocket::OnReadCompleted(int result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  HandleReadResult(result);
  if (result >= 0) {
    DoRead();
  }
}

void UdpPacketSocket::HandleReadResult(int result) {
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  if (result > 0) {
    rtc::SocketAddress address;
    if (!webrtc::IPEndPointToSocketAddress(receive_address_, &address)) {
      NOTREACHED() << "Failed to convert address received from RecvFrom().";
    }
    rtc::ReceivedPacket packet(
        rtc::MakeArrayView(receive_buffer_->bytes(), result), address,
        webrtc::Timestamp::Micros(rtc::TimeMicros()));
    NotifyPacketReceived(packet);
  } else {
    LOG(ERROR) << "Received error when reading from UDP socket: " << result;
  }
}

}  // namespace

ChromiumPacketSocketFactory::ChromiumPacketSocketFactory(
    base::WeakPtr<SessionOptionsProvider> session_options_provider)
    : session_options_provider_(session_options_provider) {}

ChromiumPacketSocketFactory::~ChromiumPacketSocketFactory() = default;

rtc::AsyncPacketSocket* ChromiumPacketSocketFactory::CreateUdpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port) {
  if (session_options_provider_ &&
      session_options_provider_->session_options().GetBoolValue(
          "Disable-UDP")) {
    HOST_LOG
        << "Disable-UDP experiment is enabled. UDP socket won't be created.";
    return nullptr;
  }
  std::unique_ptr<UdpPacketSocket> result = std::make_unique<UdpPacketSocket>();
  if (!result->Init(local_address, min_port, max_port)) {
    return nullptr;
  }
  return result.release();
}

rtc::AsyncListenSocket* ChromiumPacketSocketFactory::CreateServerTcpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  // TCP sockets are not supported.
  // TODO(yuweih): Implement server side TCP support crbug.com/600032 .
  NOTIMPLEMENTED();
  return nullptr;
}

rtc::AsyncPacketSocket* ChromiumPacketSocketFactory::CreateClientTcpSocket(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::PacketSocketTcpOptions& opts) {
  auto socket = std::make_unique<StreamPacketSocket>();
  if (!socket->InitClientTcp(local_address, remote_address, opts)) {
    return nullptr;
  }
  return socket.release();
}

std::unique_ptr<webrtc::AsyncDnsResolverInterface>
ChromiumPacketSocketFactory::CreateAsyncDnsResolver() {
  return std::make_unique<webrtc::AsyncDnsResolver>();
}

}  // namespace remoting::protocol
