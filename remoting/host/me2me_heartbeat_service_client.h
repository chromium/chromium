// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/heartbeat_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

// HeartbeatServiceClient implementation which is used for public Me2Me hosts.
class Me2MeHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  // TODO: joedow - Remove |fqdn| when all Corp heartbeat traffic goes through
  // the Corp API.
  Me2MeHeartbeatServiceClient(
      const std::string& directory_id,
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::optional<std::string> fqdn);

  Me2MeHeartbeatServiceClient(const Me2MeHeartbeatServiceClient&) = delete;
  Me2MeHeartbeatServiceClient& operator=(const Me2MeHeartbeatServiceClient&) =
      delete;

  ~Me2MeHeartbeatServiceClient() override;

  // HeartbeatServiceClient implementation.
  void SendFullHeartbeat(bool is_initial_heartbeat,
                         std::optional<std::string> signaling_id,
                         std::optional<std::string> offline_reason,
                         HeartbeatResponseCallback callback) override;
  void SendLiteHeartbeat(HeartbeatResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  // The entity to update in Directory service.
  std::string directory_id_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_
