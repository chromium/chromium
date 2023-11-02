// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_
#define NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_

#include <stdint.h>

#include <memory>

#include "net/base/io_buffer.h"
#include "net/socket/ssl_server_socket.h"
#include "net/ssl/ssl_server_config.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

class SSLServerContextImpl : public SSLServerContext {
 public:
  SSLServerContextImpl(X509Certificate* certificate,
                       EVP_PKEY* pkey,
                       const SSLServerConfig& ssl_server_config);
  SSLServerContextImpl(X509Certificate* certificate,
                       scoped_refptr<SSLPrivateKey> key,
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

  // Certificate for the server.
  scoped_refptr<X509Certificate> cert_;

  // Private key used by the server.
  // Only one representation should be set at any time.
  bssl::UniquePtr<EVP_PKEY> pkey_;
  const scoped_refptr<SSLPrivateKey> private_key_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_SERVER_SOCKET_IMPL_H_
