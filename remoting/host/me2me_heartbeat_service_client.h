// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/heartbeat_service_client.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class OAuthTokenGetter;

// HeartbeatServiceClient implementation which is used for public Me2Me hosts.
class Me2MeHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  // TODO: joedow - Remove |set_fqdn| when all Corp heartbeat traffic goes
  // through the Corp API.
  Me2MeHeartbeatServiceClient(
      const std::string& directory_id,
      bool set_fqdn,
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

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
  // DirectoryServiceClient::LegacyHeartbeatCallback with
  // HeartbeatServiceClient::HeartbeatResponseCallback bound.
  void OnLegacyHeartbeatResponse(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::HeartbeatResponse> response);
  // DirectoryServiceClient::SendHeartbeatCallback with
  // HeartbeatServiceClient::HeartbeatResponseCallback bound.
  void OnSendHeartbeatResponse(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<apis::v1::SendHeartbeatResponse> response);

  // The entity to update in Directory service.
  std::string directory_id_;

  bool set_fqdn_;

  DirectoryServiceClient client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<Me2MeHeartbeatServiceClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_ME2ME_HEARTBEAT_SERVICE_CLIENT_H_
