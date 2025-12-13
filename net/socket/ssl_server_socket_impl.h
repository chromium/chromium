// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "net/base/io_buffer.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_server_config.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

class SSLServerContextImpl : public SSLServerContext {
 public:
  SSLServerContextImpl(std::vector<SSLServerCredential> credentials,
                       const SSLServerConfig& ssl_server_config);
  ~SSLServerContextImpl() override;

  std::unique_ptr<SSLServerSocket> CreateSSLServerSocket(
      std::unique_ptr<StreamSocket> socket) override;

 private:
  class SocketImpl;

  void Init();

  bssl::UniquePtr<SSL_CTX> ssl_ctx_;

  // Options for the SSL socket.
  SSLServerConfig ssl_server_config_;

  // Credentials for the server, in order from highest to lowest priority.
  std::vector<SSLServerCredential> credentials_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_
