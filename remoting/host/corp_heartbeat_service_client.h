// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/heartbeat_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

// HeartbeatServiceClient implementation which is used for Corp hosts.
class CorpHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  CorpHeartbeatServiceClient(
      const std::string& directory_id,
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CorpHeartbeatServiceClient(const CorpHeartbeatServiceClient&) = delete;
  CorpHeartbeatServiceClient& operator=(const CorpHeartbeatServiceClient&) =
      delete;

  ~CorpHeartbeatServiceClient() override;

  // HeartbeatServiceClient implementation.
  void UpdateRemoteAccessHost(
      bool is_initial_heartbeat,
      std::optional<std::string> ftl_signaling_id,
      std::optional<std::string> offline_reason,
      UpdateRemoteAccessHostResponseCallback callback) override;
  void SendHeartbeat(SendHeartbeatResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  // The entity to update in Directory service.
  std::string directory_id_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_
