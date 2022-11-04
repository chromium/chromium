// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"

namespace net {

class ClientSocketPoolManager;
class HttpStreamFactory;
class HttpNetworkSession;
struct HttpNetworkSessionParams;

class NET_EXPORT_PRIVATE HttpNetworkSessionPeer {
 public:
  // |session| should outlive the HttpNetworkSessionPeer.
  explicit HttpNetworkSessionPeer(HttpNetworkSession* session);

  HttpNetworkSessionPeer(const HttpNetworkSessionPeer&) = delete;
  HttpNetworkSessionPeer& operator=(const HttpNetworkSessionPeer&) = delete;

  ~HttpNetworkSessionPeer();

  void SetClientSocketPoolManager(
      std::unique_ptr<ClientSocketPoolManager> socket_pool_manager);

  void SetHttpStreamFactory(
      std::unique_ptr<HttpStreamFactory> http_stream_factory);

  HttpNetworkSessionParams* params();

 private:
  const raw_ptr<HttpNetworkSession> session_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
