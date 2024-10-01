// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/cloud_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/heartbeat_service_client.h"

namespace google::internal::remoting::cloud::v1alpha {
class Empty;
class RemoteAccessHost;
}  // namespace google::internal::remoting::cloud::v1alpha

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
  void OnSendHeartbeatResponse(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<::google::internal::remoting::cloud::v1alpha::Empty>);

  void OnUpdateRemoteAccessHostResponse(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<
          ::google::internal::remoting::cloud::v1alpha::RemoteAccessHost>);

  void OnReportHostOffline(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<
          ::google::internal::remoting::cloud::v1alpha::RemoteAccessHost>);

  void MakeUpdateRemoteAccessHostCall(
      std::optional<std::string> signaling_id,
      std::optional<std::string> offline_reason,
      CloudServiceClient::UpdateRemoteAccessHostCallback callback);

  void RunHeartbeatResponseCallback(HeartbeatResponseCallback callback,
                                    const ProtobufHttpStatus& status);

  // The entity to update in Directory service.
  std::string directory_id_;

  CloudServiceClient client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CloudHeartbeatServiceClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLOUD_HEARTBEAT_SERVICE_CLIENT_H_
