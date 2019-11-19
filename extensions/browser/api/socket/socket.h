// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_SOCKET_H_
#define EXTENSIONS_BROWSER_API_SOCKET_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/socket/app_firewall_hole_manager.h"
#endif  // OS_CHROMEOS

namespace net {
class AddressList;
class IPEndPoint;
class Socket;
}

namespace extensions {

using SetNoDelayCallback = base::OnceCallback<void(bool)>;
using SetKeepAliveCallback = base::OnceCallback<void(bool)>;
using ReadCompletionCallback = base::OnceCallback<
    void(int, scoped_refptr<net::IOBuffer> io_buffer, bool socket_destroying)>;
using RecvFromCompletionCallback =
    base::OnceCallback<void(int,
                            scoped_refptr<net::IOBuffer> io_buffer,
                            bool socket_destroying,
                            const std::string&,
                            uint16_t)>;
using ListenCallback =
    base::OnceCallback<void(int, const std::string& error_msg)>;

using AcceptCompletionCallback = base::OnceCallback<void(
    int,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>,
    const base::Optional<net::IPEndPoint>&,
    mojo::ScopedDataPipeConsumerHandle,
    mojo::ScopedDataPipeProducerHandle)>;

// A Socket wraps a low-level socket and includes housekeeping information that
// we need to manage it in the context of an extension.
class Socket : public ApiResource {
 public:
  enum SocketType { TYPE_TCP, TYPE_UDP, TYPE_TLS };

  ~Socket() override;

  // The hostname of the remote host that this socket is connected to.  This
  // may be the empty string if the client does not intend to ever upgrade the
  // socket to TLS, and thusly has not invoked set_hostname().
  const std::string& hostname() const { return hostname_; }

  // Set the hostname of the remote host that this socket is connected to.
  // Note: This may be an IP literal. In the case of IDNs, this should be a
  // series of U-LABELs (UTF-8), not A-LABELs. IP literals for IPv6 will be
  // unbracketed.
  void set_hostname(const std::string& hostname) { hostname_ = hostname; }

#if defined(OS_CHROMEOS)
  void set_firewall_hole(
      std::unique_ptr<AppFirewallHole, content::BrowserThread::DeleteOnUIThread>
          firewall_hole) {
    firewall_hole_ = std::move(firewall_hole);
  }
#endif  // OS_CHROMEOS

  // Note: |address| contains the resolved IP address, not the hostname of
  // the remote endpoint. In order to upgrade this socket to TLS, callers
  // must also supply the hostname of the endpoint via set_hostname().
  virtual void Connect(const net::AddressList& address,
                       net::CompletionOnceCallback callback) = 0;
  // |socket_destroying| is true if disconnect is due to destruction of the
  // socket.
  virtual void Disconnect(bool socket_destroying) = 0;
  virtual void Bind(const std::string& address,
                    uint16_t port,
                    net::CompletionOnceCallback callback) = 0;

  // The |callback| will be called with the number of bytes read into the
  // buffer, or a negative number if an error occurred.
  virtual void Read(int count, ReadCompletionCallback callback) = 0;

  // The |callback| will be called with |byte_count| or a negative number if an
  // error occurred.
  void Write(scoped_refptr<net::IOBuffer> io_buffer,
             int byte_count,
             net::CompletionOnceCallback callback);

  virtual void RecvFrom(int count, RecvFromCompletionCallback callback) = 0;
  virtual void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const net::IPEndPoint& address,
                      net::CompletionOnceCallback callback) = 0;

  virtual void SetKeepAlive(bool enable,
                            int delay,
                            SetKeepAliveCallback callback);
  virtual void SetNoDelay(bool no_delay, SetNoDelayCallback callback);
  virtual void Listen(const std::string& address,
                      uint16_t port,
                      int backlog,
                      ListenCallback callback);
  virtual void Accept(AcceptCompletionCallback callback);

  virtual bool IsConnected() = 0;

  virtual bool GetPeerAddress(net::IPEndPoint* address) = 0;
  virtual bool GetLocalAddress(net::IPEndPoint* address) = 0;

  virtual SocketType GetSocketType() const = 0;

  static bool StringAndPortToIPEndPoint(const std::string& ip_address_str,
                                        uint16_t port,
                                        net::IPEndPoint* ip_end_point);
  static void IPEndPointToStringAndPort(const net::IPEndPoint& address,
                                        std::string* ip_address_str,
                                        uint16_t* port);

  static net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag();

 protected:
  explicit Socket(const std::string& owner_extension_id_);

  void WriteData();
  virtual int WriteImpl(net::IOBuffer* io_buffer,
                        int io_buffer_size,
                        net::CompletionOnceCallback callback) = 0;

  std::string hostname_;
  bool is_connected_;

 private:
  friend class ApiResourceManager<Socket>;
  static const char* service_name() { return "SocketManager"; }

  struct WriteRequest {
    WriteRequest(scoped_refptr<net::IOBuffer> io_buffer,
                 int byte_count,
                 net::CompletionOnceCallback callback);
    WriteRequest(WriteRequest&& other);
    ~WriteRequest();
    scoped_refptr<net::IOBuffer> io_buffer;
    int byte_count;
    net::CompletionOnceCallback callback;
    int bytes_written;
  };

  void OnWriteComplete(int result);

  base::queue<WriteRequest> write_queue_;
  scoped_refptr<net::IOBuffer> io_buffer_write_;

#if defined(OS_CHROMEOS)
  // Represents a hole punched in the system firewall for this socket.
  std::unique_ptr<AppFirewallHole, content::BrowserThread::DeleteOnUIThread>
      firewall_hole_;
#endif  // OS_CHROMEOS
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_SOCKET_H_
