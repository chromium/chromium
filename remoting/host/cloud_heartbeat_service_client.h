// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/heartbeat_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

// HeartbeatServiceClient implementation which is used for Cloud hosts.
class CloudHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  CloudHeartbeatServiceClient(
      const std::string& directory_id,
      const std::string& api_key,
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CloudHeartbeatServiceClient(const CloudHeartbeatServiceClient&) = delete;
  CloudHeartbeatServiceClient& operator=(const CloudHeartbeatServiceClient&) =
      delete;

  ~CloudHeartbeatServiceClient() override;

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

  // The customer API_KEY to use for calling the Cloud API.
  std::string api_key_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_
