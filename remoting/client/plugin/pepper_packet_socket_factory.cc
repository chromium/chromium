// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_packet_socket_factory.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/udp_socket.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/client/plugin/pepper_address_resolver.h"
#include "remoting/client/plugin/pepper_util.h"
#include "remoting/protocol/socket_util.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/net_helpers.h"

namespace remoting {

namespace {

// Size of the buffer to allocate for RecvFrom().
const int kReceiveBufferSize = 65536;

// Maximum amount of data in the send buffers. This is necessary to
// prevent out-of-memory crashes if the caller sends data faster than
// Pepper's UDP API can handle it. This maximum should never be
// reached under normal conditions.
const int kMaxSendBufferSize = 256 * 1024;

int PepperErrorToNetError(int error) {
  switch (error) {
    case PP_OK:
      return net::OK;
    case PP_OK_COMPLETIONPENDING:
      return net::ERR_IO_PENDING;
    case PP_ERROR_ABORTED:
      return net::ERR_ABORTED;
    case PP_ERROR_BADARGUMENT:
      return net::ERR_INVALID_ARGUMENT;
    case PP_ERROR_FILENOTFOUND:
      return net::ERR_FILE_NOT_FOUND;
    case PP_ERROR_TIMEDOUT:
      return net::ERR_TIMED_OUT;
    case PP_ERROR_FILETOOBIG:
      return net::ERR_FILE_TOO_BIG;
    case PP_ERROR_NOTSUPPORTED:
      return net::ERR_NOT_IMPLEMENTED;
    case PP_ERROR_NOMEMORY:
      return net::ERR_OUT_OF_MEMORY;
    case PP_ERROR_FILEEXISTS:
      return net::ERR_FILE_EXISTS;
    case PP_ERROR_NOSPACE:
      return net::ERR_FILE_NO_SPACE;
    case PP_ERROR_CONNECTION_CLOSED:
      return net::ERR_CONNECTION_CLOSED;
    case PP_ERROR_CONNECTION_RESET:
      return net::ERR_CONNECTION_RESET;
    case PP_ERROR_CONNECTION_REFUSED:
      return net::ERR_CONNECTION_REFUSED;
    case PP_ERROR_CONNECTION_ABORTED:
      return net::ERR_CONNECTION_ABORTED;
    case PP_ERROR_CONNECTION_FAILED:
      return net::ERR_CONNECTION_FAILED;
    case PP_ERROR_NAME_NOT_RESOLVED:
      return net::ERR_NAME_NOT_RESOLVED;
    case PP_ERROR_ADDRESS_INVALID:
      return net::ERR_ADDRESS_INVALID;
    case PP_ERROR_ADDRESS_UNREACHABLE:
      return net::ERR_ADDRESS_UNREACHABLE;
    case PP_ERROR_CONNECTION_TIMEDOUT:
      return net::ERR_CONNECTION_TIMED_OUT;
    case PP_ERROR_NOACCESS:
      return net::ERR_NETWORK_ACCESS_DENIED;
    case PP_ERROR_MESSAGE_TOO_BIG:
      return net::ERR_MSG_TOO_BIG;
    case PP_ERROR_ADDRESS_IN_USE:
      return net::ERR_ADDRESS_IN_USE;
    default:
      return net::ERR_FAILED;
  }
}

class UdpPacketSocket : public rtc::AsyncPacketSocket {
 public:
  explicit UdpPacketSocket(const pp::InstanceHandle& instance);
  ~UdpPacketSocket() override;

  // |min_port| and |max_port| are set to zero if the port number
  // should be assigned by the OS.
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
  int GetOption(rtc::Socket::Option opt, int* value) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetError() const override;
  void SetError(int error) override;

 private:
  struct PendingPacket {
    PendingPacket(const void* buffer,
                  int buffer_size,
                  const pp::NetAddress& address);

    scoped_refptr<net::IOBufferWithSize> data;
    pp::NetAddress address;
    bool retried;
  };

  void OnBindCompleted(int error);

  void DoSend();
  void OnSendCompleted(int result);

  void DoRead();
  void OnReadCompleted(int result, pp::NetAddress address);
  void HandleReadResult(int result, pp::NetAddress address);

  pp::InstanceHandle instance_;

  pp::UDPSocket socket_;

  State state_;
  int error_;

  rtc::SocketAddress local_address_;

  // Used to scan ports when necessary. Both values are set to 0 when
  // the port number is assigned by OS.
  uint16_t min_port_;
  uint16_t max_port_;

  std::vector<char> receive_buffer_;

  bool send_pending_;
  std::list<PendingPacket> send_queue_;
  int send_queue_size_;

  pp::CompletionCallbackFactory<UdpPacketSocket> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(UdpPacketSocket);
};

UdpPacketSocket::PendingPacket::PendingPacket(const void* buffer,
                                              int buffer_size,
                                              const pp::NetAddress& address)
    : data(base::MakeRefCounted<net::IOBufferWithSize>(buffer_size)),
      address(address),
      retried(true) {
  memcpy(data->data(), buffer, buffer_size);
}

UdpPacketSocket::UdpPacketSocket(const pp::InstanceHandle& instance)
    : instance_(instance),
      socket_(instance),
      state_(STATE_CLOSED),
      error_(0),
      min_port_(0),
      max_port_(0),
      send_pending_(false),
      send_queue_size_(0),
      callback_factory_(this) {
}

UdpPacketSocket::~UdpPacketSocket() {
  Close();
}

bool UdpPacketSocket::Init(const rtc::SocketAddress& local_address,
                           uint16_t min_port,
                           uint16_t max_port) {
  if (socket_.is_null()) {
    return false;
  }

  local_address_ = local_address;
  max_port_ = max_port;
  min_port_ = min_port;

  pp::NetAddress pp_local_address;
  if (!SocketAddressToPpNetAddressWithPort(
          instance_, local_address_, &pp_local_address, min_port_)) {
    return false;
  }

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&UdpPacketSocket::OnBindCompleted);
  int result = socket_.Bind(pp_local_address, callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  state_ = STATE_BINDING;

  return true;
}

void UdpPacketSocket::OnBindCompleted(int result) {
  DCHECK(state_ == STATE_BINDING || state_ == STATE_CLOSED);

  if (result == PP_ERROR_ABORTED) {
    // Socket is being destroyed while binding.
    return;
  }

  if (result == PP_OK) {
    pp::NetAddress address = socket_.GetBoundAddress();
    PpNetAddressToSocketAddress(address, &local_address_);
    state_ = STATE_BOUND;
    SignalAddressReady(this, local_address_);
    DoRead();
    return;
  }

  if (min_port_ < max_port_) {
    // Try to bind to the next available port.
    ++min_port_;
    pp::NetAddress pp_local_address;
    if (SocketAddressToPpNetAddressWithPort(
            instance_, local_address_, &pp_local_address, min_port_)) {
      pp::CompletionCallback callback =
          callback_factory_.NewCallback(&UdpPacketSocket::OnBindCompleted);
      int result = socket_.Bind(pp_local_address, callback);
      DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
    }
  } else {
    LOG(ERROR) << "Failed to bind UDP socket to " << local_address_.ToString()
               << ", error: " << result;
  }
}

rtc::SocketAddress UdpPacketSocket::GetLocalAddress() const {
  DCHECK_EQ(state_, STATE_BOUND);
  return local_address_;
}

rtc::SocketAddress UdpPacketSocket::GetRemoteAddress() const {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
  return rtc::SocketAddress();
}

int UdpPacketSocket::Send(const void* data, size_t data_size,
                          const rtc::PacketOptions& options) {
  // UDP sockets are not connected - this method should never be called.
  NOTREACHED();
  return EWOULDBLOCK;
}

int UdpPacketSocket::SendTo(const void* data,
                            size_t data_size,
                            const rtc::SocketAddress& address,
                            const rtc::PacketOptions& options) {
  if (state_ != STATE_BOUND) {
    // TODO(sergeyu): StunPort may try to send stun request before we
    // are bound. Fix that problem and change this to DCHECK.
    return EINVAL;
  }

  if (error_ != 0) {
    return error_;
  }

  pp::NetAddress pp_address;
  if (!SocketAddressToPpNetAddress(instance_, address, &pp_address)) {
    return EINVAL;
  }

  if (send_queue_size_ >= kMaxSendBufferSize) {
    return EWOULDBLOCK;
  }

  send_queue_.push_back(PendingPacket(data, data_size, pp_address));
  send_queue_size_ += data_size;
  DoSend();
  return data_size;
}

int UdpPacketSocket::Close() {
  state_ = STATE_CLOSED;
  socket_.Close();
  return 0;
}

rtc::AsyncPacketSocket::State UdpPacketSocket::GetState() const {
  return state_;
}

int UdpPacketSocket::GetOption(rtc::Socket::Option opt, int* value) {
  // Options are not supported for Pepper UDP sockets.
  return -1;
}

int UdpPacketSocket::SetOption(rtc::Socket::Option opt, int value) {
  // Options are not supported for Pepper UDP sockets.
  return -1;
}

int UdpPacketSocket::GetError() const {
  return error_;
}

void UdpPacketSocket::SetError(int error) {
  error_ = error;
}

void UdpPacketSocket::DoSend() {
  if (send_pending_ || send_queue_.empty())
    return;

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&UdpPacketSocket::OnSendCompleted);
  int result = socket_.SendTo(
      send_queue_.front().data->data(), send_queue_.front().data->size(),
      send_queue_.front().address,
      callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  send_pending_ = true;
}

void UdpPacketSocket::OnSendCompleted(int result) {
  if (result == PP_ERROR_ABORTED) {
    // Send is aborted when the socket is being destroyed.
    // |send_queue_| may be already destroyed, it's not safe to access
    // it here.
    return;
  }

  send_pending_ = false;

  if (result < 0) {
    int net_error = PepperErrorToNetError(result);
    SocketErrorAction action = GetSocketErrorAction(net_error);
    switch (action) {
      case SOCKET_ERROR_ACTION_FAIL:
        LOG(ERROR) << "Send failed on a UDP socket: " << result;
        error_ = EINVAL;
        return;

      case SOCKET_ERROR_ACTION_RETRY:
        // Retry resending only once.
        if (!send_queue_.front().retried) {
          send_queue_.front().retried = true;
          DoSend();
          return;
        }
        break;

      case SOCKET_ERROR_ACTION_IGNORE:
        break;
    }
  }

  send_queue_size_ -= send_queue_.front().data->size();
  send_queue_.pop_front();
  DoSend();
}

void UdpPacketSocket::DoRead() {
  receive_buffer_.resize(kReceiveBufferSize);
  pp::CompletionCallbackWithOutput<pp::NetAddress> callback =
      callback_factory_.NewCallbackWithOutput(
          &UdpPacketSocket::OnReadCompleted);
  int result =
      socket_.RecvFrom(&receive_buffer_[0], receive_buffer_.size(), callback);
  DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);
}

void UdpPacketSocket::OnReadCompleted(int result, pp::NetAddress address) {
  HandleReadResult(result, address);
  if (result > 0) {
    DoRead();
  }
}

void UdpPacketSocket::HandleReadResult(int result, pp::NetAddress address) {
  if (result > 0) {
    rtc::SocketAddress socket_address;
    PpNetAddressToSocketAddress(address, &socket_address);
    SignalReadPacket(this, &receive_buffer_[0], result, socket_address,
                     rtc::TimeMicros());
  } else if (result != PP_ERROR_ABORTED) {
    LOG(ERROR) << "Received error when reading from UDP socket: " << result;
  }
}

}  // namespace

PepperPacketSocketFactory::PepperPacketSocketFactory(
    const pp::InstanceHandle& instance)
    : pp_instance_(instance) {
}

PepperPacketSocketFactory::~PepperPacketSocketFactory() {
}

rtc::AsyncPacketSocket* PepperPacketSocketFactory::CreateUdpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port) {
  std::unique_ptr<UdpPacketSocket> result(new UdpPacketSocket(pp_instance_));
  if (!result->Init(local_address, min_port, max_port))
    return nullptr;
  return result.release();
}

rtc::AsyncPacketSocket* PepperPacketSocketFactory::CreateServerTcpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  // We don't use TCP sockets for remoting connections.
  NOTREACHED();
  return nullptr;
}

rtc::AsyncPacketSocket* PepperPacketSocketFactory::CreateClientTcpSocket(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::ProxyInfo& proxy_info,
    const std::string& user_agent,
    const rtc::PacketSocketTcpOptions& opts) {
  // We don't use TCP sockets for remoting connections.
  NOTREACHED();
  return nullptr;
}

rtc::AsyncResolverInterface*
PepperPacketSocketFactory::CreateAsyncResolver() {
  return new PepperAddressResolver(pp_instance_);
}

}  // namespace remoting
