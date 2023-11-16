// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_
#define EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/common/api/socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace content {
class BrowserContext;
class StoragePartition;
}

namespace extensions {

class MojoDataPump;

class TCPSocket : public Socket {
 public:
  using UpgradeToTLSCallback = base::OnceCallback<void(
      int,
      mojo::PendingRemote<network::mojom::TLSClientSocket>,
      const net::IPEndPoint&,
      const net::IPEndPoint&,
      mojo::ScopedDataPipeConsumerHandle,
      mojo::ScopedDataPipeProducerHandle)>;

  // Constuctor for when |socket_mode_| is unknown. The |socket_mode_| will be
  // filled in when the consumer calls Listen/Connect.
  TCPSocket(content::BrowserContext* browser_context,
            const std::string& owner_extension_id);

  // Created using TCPServerSocket::Accept().
  TCPSocket(mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream,
            const std::optional<net::IPEndPoint>& remote_addr,
            const std::string& owner_extension_id);

  TCPSocket(const TCPSocket&) = delete;
  TCPSocket& operator=(const TCPSocket&) = delete;

  ~TCPSocket() override;

  void Connect(const net::AddressList& address,
               net::CompletionOnceCallback callback) override;
  void Disconnect(bool socket_destroying) override;
  void Bind(const std::string& address,
            uint16_t port,
            net::CompletionOnceCallback callback) override;
  void Read(int count, ReadCompletionCallback callback) override;
  void RecvFrom(int count, RecvFromCompletionCallback callback) override;
  void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
              int byte_count,
              const net::IPEndPoint& address,
              net::CompletionOnceCallback callback) override;
  void SetKeepAlive(bool enable,
                    int delay,
                    SetKeepAliveCallback callback) override;
  void SetNoDelay(bool no_delay, SetNoDelayCallback callback) override;
  void Listen(const std::string& address,
              uint16_t port,
              int backlog,
              ListenCallback callback) override;
  void Accept(AcceptCompletionCallback callback) override;

  bool IsConnected() override;

  bool GetPeerAddress(net::IPEndPoint* address) override;
  bool GetLocalAddress(net::IPEndPoint* address) override;

  Socket::SocketType GetSocketType() const override;

  void UpgradeToTLS(api::socket::SecureOptions* options,
                    UpgradeToTLSCallback callback);

  void SetStoragePartitionForTest(
      content::StoragePartition* storage_partition) {
    storage_partition_ = storage_partition;
  }

 protected:
  int WriteImpl(net::IOBuffer* io_buffer,
                int io_buffer_size,
                net::CompletionOnceCallback callback) override;

 private:
  // Connects a client TCP socket.
  void OnConnectComplete(int result,
                         const std::optional<net::IPEndPoint>& local_addr,
                         const std::optional<net::IPEndPoint>& peer_addr,
                         mojo::ScopedDataPipeConsumerHandle receive_stream,
                         mojo::ScopedDataPipeProducerHandle send_stream);

  // Connects a server TCP socket.
  void OnListenComplete(int result,
                        const std::optional<net::IPEndPoint>& local_addr);
  void OnAccept(
      int result,
      const std::optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);
  void OnWriteComplete(net::CompletionOnceCallback callback, int result);
  void OnReadComplete(int result, scoped_refptr<net::IOBuffer> io_buffer);
  void OnUpgradeToTLSComplete(
      UpgradeToTLSCallback callback,
      mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
      const net::IPEndPoint& local_addr,
      const net::IPEndPoint& peer_addr,
      int result,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      const std::optional<net::SSLInfo>& ssl_info);

  content::StoragePartition* GetStoragePartitionHelper();

  enum SocketMode {
    UNKNOWN = 0,
    CLIENT,
    SERVER,
  };

  // |this| doesn't outlive |browser_context_| because |this| is owned by
  // ApiResourceManager which is a BrowserContextKeyedAPI.
  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;

  SocketMode socket_mode_;

  // CLIENT mode.
  mojo::Remote<network::mojom::TCPConnectedSocket> client_socket_;
  // SERVER mode.
  mojo::Remote<network::mojom::TCPServerSocket> server_socket_;

  net::CompletionOnceCallback connect_callback_;
  ListenCallback listen_callback_;
  AcceptCompletionCallback accept_callback_;
  ReadCompletionCallback read_callback_;

  std::unique_ptr<MojoDataPump> mojo_data_pump_;

  std::optional<net::IPEndPoint> local_addr_;
  std::optional<net::IPEndPoint> peer_addr_;

  // Only used in tests.
  raw_ptr<content::StoragePartition, DanglingUntriaged> storage_partition_ =
      nullptr;

  // WeakPtr is used when posting tasks to |task_runner_| which might outlive
  // |this|.
  base::WeakPtrFactory<TCPSocket> weak_factory_{this};
};

// TCP Socket instances from the "sockets.tcp" namespace. These are regular
// socket objects with additional properties related to the behavior defined in
// the "sockets.tcp" namespace.
class ResumableTCPSocket : public TCPSocket {
 public:
  ResumableTCPSocket(content::BrowserContext* browser_context,
                     const std::string& owner_extension_id);
  // Created using TCPServerSocket::Accept().
  ResumableTCPSocket(
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      const std::optional<net::IPEndPoint>& remote_addr,
      const std::string& owner_extension_id);

  ~ResumableTCPSocket() override;

  // Overriden from ApiResource
  bool IsPersistent() const override;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  int buffer_size() const { return buffer_size_; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class ApiResourceManager<ResumableTCPSocket>;
  static const char* service_name() { return "ResumableTCPSocketManager"; }

  // Application-defined string - see sockets_tcp.idl.
  std::string name_;
  // Flag indicating whether the socket is left open when the application is
  // suspended - see sockets_tcp.idl.
  bool persistent_;
  // The size of the buffer used to receive data - see sockets_tcp.idl.
  int buffer_size_;
  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see sockets_tcp.idl.
  bool paused_;
};

// TCP Socket instances from the "sockets.tcpServer" namespace. These are
// regular socket objects with additional properties related to the behavior
// defined in the "sockets.tcpServer" namespace.
class ResumableTCPServerSocket : public TCPSocket {
 public:
  ResumableTCPServerSocket(content::BrowserContext* browser_context,
                           const std::string& owner_extension_id);

  // Overriden from ApiResource
  bool IsPersistent() const override;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class ApiResourceManager<ResumableTCPServerSocket>;
  static const char* service_name() {
    return "ResumableTCPServerSocketManager";
  }

  // Application-defined string - see sockets_tcp_server.idl.
  std::string name_;
  // Flag indicating whether the socket is left open when the application is
  // suspended - see sockets_tcp_server.idl.
  bool persistent_;
  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see sockets_tcp_server.idl.
  bool paused_;
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_
