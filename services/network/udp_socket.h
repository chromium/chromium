// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_UDP_SOCKET_H_
#define SERVICES_NETWORK_UDP_SOCKET_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/address_family.mojom.h"
#include "services/network/public/mojom/ip_endpoint.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace net {
class IOBuffer;
class IOBufferWithSize;
class NetLog;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) UDPSocket : public mojom::UDPSocket {
 public:
  // Number of Send()/SendTo() requests that are queued internally. Public for
  // testing.
  static const uint32_t kMaxPendingSendRequests = 32;
  // A socket wrapper class that allows tests to substitute the default
  // implementation (implemented using net::UDPSocket) with a test
  // implementation.
  class SocketWrapper {
   public:
    virtual ~SocketWrapper() {}
    // This wrapper class forwards the functions to a concrete udp socket
    // implementation. Please refer to udp_socket_posix.h/udp_socket_win.h for
    // definitions.
    virtual int Connect(const net::IPEndPoint& remote_addr,
                        mojom::UDPSocketOptionsPtr options,
                        net::IPEndPoint* local_addr_out) = 0;
    virtual int Bind(const net::IPEndPoint& local_addr,
                     mojom::UDPSocketOptionsPtr options,
                     net::IPEndPoint* local_addr_out) = 0;
    virtual int SendTo(
        net::IOBuffer* buf,
        int buf_len,
        const net::IPEndPoint& dest_addr,
        net::CompletionOnceCallback callback,
        const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
    virtual int Write(
        net::IOBuffer* buf,
        int buf_len,
        net::CompletionOnceCallback callback,
        const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;
    virtual int SetBroadcast(bool broadcast) = 0;
    virtual int SetSendBufferSize(int send_buffer_size) = 0;
    virtual int SetReceiveBufferSize(int receive_buffer_size) = 0;
    virtual int JoinGroup(const net::IPAddress& group_address) = 0;
    virtual int LeaveGroup(const net::IPAddress& group_address) = 0;
    virtual int RecvFrom(net::IOBuffer* buf,
                         int buf_len,
                         net::IPEndPoint* address,
                         net::CompletionOnceCallback callback) = 0;
  };

  UDPSocket(mojo::PendingRemote<mojom::UDPSocketListener> listener,
            net::NetLog* net_log);
  ~UDPSocket() override;

  // UDPSocket implementation.
  void Connect(const net::IPEndPoint& remote_addr,
               mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override;
  void Bind(const net::IPEndPoint& local_addr,
            mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override;
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override;
  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override;
  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetSendBufferSizeCallback callback) override;
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override;
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override;
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override;
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override;
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;
  void Close() override;

 private:
  friend class UDPSocketTest;

  // Represents a pending Send()/SendTo() request that is yet to be sent to the
  // |socket_|. In the case of Send(), |addr| will be not filled in.
  struct PendingSendRequest {
    PendingSendRequest();
    ~PendingSendRequest();

    std::unique_ptr<net::IPEndPoint> addr;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
    scoped_refptr<net::IOBufferWithSize> data;
    SendToCallback callback;
  };

  // Helper method to create a new SocketWrapper.
  std::unique_ptr<UDPSocket::SocketWrapper> CreateSocketWrapper() const;

  // Returns whether a successful Connect() or Bind() has been executed.
  bool IsConnectedOrBound() const;

  void DoRecvFrom(uint32_t buffer_size);
  void DoSendToOrWrite(
      const net::IPEndPoint* dest_addr,
      const base::span<const uint8_t>& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      SendToCallback callback);
  void DoSendToOrWriteBuffer(
      const net::IPEndPoint* dest_addr,
      scoped_refptr<net::IOBufferWithSize> buffer,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      SendToCallback callback);

  void OnRecvFromCompleted(uint32_t buffer_size, int net_result);
  void OnSendToCompleted(int net_result);

  net::NetLog* net_log_;

  // Whether a Bind() has been successfully executed.
  bool is_bound_;

  // Whether a Connect() has been successfully executed.
  bool is_connected_;

  // The interface which gets data from fulfilled receive requests.
  mojo::Remote<mojom::UDPSocketListener> listener_;

  std::unique_ptr<SocketWrapper> wrapped_socket_;

  // Non-null when there is a pending RecvFrom operation on socket.
  scoped_refptr<net::IOBuffer> recvfrom_buffer_;

  // Non-null when there is a pending Send/SendTo operation on socket.
  scoped_refptr<net::IOBufferWithSize> send_buffer_;
  SendToCallback send_callback_;

  // The address of the sender of a received packet. This address might not be
  // filled if an error occurred during the receiving of the packet.
  net::IPEndPoint recvfrom_address_;

  // How many more packets the client side expects to receive.
  uint32_t remaining_recv_slots_;

  // The queue owns the PendingSendRequest instances.
  base::circular_deque<std::unique_ptr<PendingSendRequest>>
      pending_send_requests_;

  DISALLOW_COPY_AND_ASSIGN(UDPSocket);
};

}  // namespace network

#endif  // SERVICES_NETWORK_UDP_SOCKET_H_
