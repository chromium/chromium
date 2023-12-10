// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_TLS_SOCKET_H_
#define EXTENSIONS_BROWSER_API_SOCKET_TLS_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "extensions/browser/api/socket/mojo_data_pump.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/tls_socket.mojom.h"

namespace extensions {

class MojoDataPump;

// TLS Sockets from the chrome.socket and chrome.sockets.tcp APIs. A regular
// TCPSocket is converted to a TLSSocket via chrome.socket.secure() or
// chrome.sockets.tcp.secure(). The inheritance here is for interface API
// compatibility, not for the implementation that comes with it. TLSSocket
// does not use its superclass's socket state, so all methods are overridden
// here to prevent any access of ResumableTCPSocket's socket state. Except
// for the implementation of a write queue in Socket::Write() (a super-super
// class of ResumableTCPSocket). That implementation only queues and
// serializes invocations to WriteImpl(), implemented here, and does not
// touch any socket state.
class TLSSocket : public ResumableTCPSocket {
 public:
  TLSSocket(mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
            const net::IPEndPoint& local_addr,
            const net::IPEndPoint& peer_addr,
            mojo::ScopedDataPipeConsumerHandle receive_stream,
            mojo::ScopedDataPipeProducerHandle send_stream,
            const std::string& owner_extension_id);

  TLSSocket(const TLSSocket&) = delete;
  TLSSocket& operator=(const TLSSocket&) = delete;

  ~TLSSocket() override;

  // Fails.
  void Connect(const net::AddressList& address,
               net::CompletionOnceCallback callback) override;
  // Forwards.
  void Disconnect(bool socket_destroying) override;

  // Attempts to read |count| bytes of decrypted data from the TLS socket,
  // invoking |callback| with the actual number of bytes read, or a network
  // error code if an error occurred.
  void Read(int count, ReadCompletionCallback callback) override;

  // Fails. TLSSocket is only a client.
  void Listen(const std::string& address,
              uint16_t port,
              int backlog,
              ListenCallback callback) override;

  // Forwards.
  bool IsConnected() override;

  bool GetPeerAddress(net::IPEndPoint* address) override;
  bool GetLocalAddress(net::IPEndPoint* address) override;

  // Returns TYPE_TLS.
  SocketType GetSocketType() const override;

 private:
  int WriteImpl(net::IOBuffer* io_buffer,
                int io_buffer_size,
                net::CompletionOnceCallback callback) override;
  void OnWriteComplete(net::CompletionOnceCallback callback, int result);
  void OnReadComplete(int result, scoped_refptr<net::IOBuffer> io_buffer);

  mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;
  std::optional<net::IPEndPoint> local_addr_;
  std::optional<net::IPEndPoint> peer_addr_;
  std::unique_ptr<MojoDataPump> mojo_data_pump_;
  ReadCompletionCallback read_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_TLS_SOCKET_H_
