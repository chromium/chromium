// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
#define NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/http/http_network_session.h"

namespace net {

class ClientSocketPoolManager;
class HttpStreamFactory;

class NET_EXPORT_PRIVATE HttpNetworkSessionPeer {
 public:
  // |session| should outlive the HttpNetworkSessionPeer.
  explicit HttpNetworkSessionPeer(HttpNetworkSession* session);
  ~HttpNetworkSessionPeer();

  void SetClientSocketPoolManager(
      std::unique_ptr<ClientSocketPoolManager> socket_pool_manager);

  void SetHttpStreamFactory(
      std::unique_ptr<HttpStreamFactory> http_stream_factory);

  HttpNetworkSession::Params* params();

 private:
  HttpNetworkSession* const session_;

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkSessionPeer);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_SESSION_PEER_H_
