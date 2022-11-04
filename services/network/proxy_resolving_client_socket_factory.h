// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "net/socket/connect_job_factory.h"
#include "net/ssl/ssl_config.h"
#include "url/gurl.h"

namespace net {
struct CommonConnectJobParams;
class HttpNetworkSession;
class NetworkAnonymizationKey;
class URLRequestContext;
}  // namespace net

namespace network {

class ProxyResolvingClientSocket;

class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingClientSocketFactory {
 public:
  // Constructs a ProxyResolvingClientSocketFactory. This factory shares
  // network session params with |request_context|, but keeps separate socket
  // pools by instantiating and owning a separate |network_session_|.
  explicit ProxyResolvingClientSocketFactory(
      net::URLRequestContext* request_context);

  ProxyResolvingClientSocketFactory(const ProxyResolvingClientSocketFactory&) =
      delete;
  ProxyResolvingClientSocketFactory& operator=(
      const ProxyResolvingClientSocketFactory&) = delete;

  ~ProxyResolvingClientSocketFactory();

  // Creates a socket. |url|'s host and port specify where a connection will be
  // established to. The full URL will be only used for proxy resolution. Caller
  // doesn't need to explicitly sanitize the url, any sensitive data (like
  // embedded usernames and passwords), and local data (i.e. reference fragment)
  // will be sanitized by net::ProxyResolutionService before the url is
  // disclosed to the PAC script.
  //
  // |network_anonymization_key| indicates the network shard to use for storing
  // shared network state (DNS cache entries, shared H2/QUIC proxy connections,
  // etc).  Proxy connections will only be shared with other
  // ProxyResolvingClientSockets, not with standards HTTP/HTTPS requests.
  //
  // If |use_tls| is true, TLS connect will be used in addition to TCP connect.
  // The URLRequestContext's SSL configurations will be respected when
  // establishing a TLS connection.
  std::unique_ptr<ProxyResolvingClientSocket> CreateSocket(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      bool use_tls);

 private:
  std::unique_ptr<net::HttpNetworkSession> network_session_;
  std::unique_ptr<net::CommonConnectJobParams> common_connect_job_params_;
  raw_ptr<net::URLRequestContext> request_context_;
  std::unique_ptr<net::ConnectJobFactory> connect_job_factory_ =
      std::make_unique<net::ConnectJobFactory>();
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_CLIENT_SOCKET_FACTORY_H_
