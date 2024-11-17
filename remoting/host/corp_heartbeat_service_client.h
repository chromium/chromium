// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/base/corp_service_client.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/heartbeat_service_client.h"
#include "remoting/proto/empty.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

// HeartbeatServiceClient implementation which is used for Corp hosts.
class CorpHeartbeatServiceClient : public HeartbeatServiceClient {
 public:
  CorpHeartbeatServiceClient(
      const std::string& directory_id,
      const std::string& refresh_token,
      const std::string& service_account_email,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  CorpHeartbeatServiceClient(const CorpHeartbeatServiceClient&) = delete;
  CorpHeartbeatServiceClient& operator=(const CorpHeartbeatServiceClient&) =
      delete;

  ~CorpHeartbeatServiceClient() override;

  // HeartbeatServiceClient implementation.
  void SendFullHeartbeat(bool is_initial_heartbeat,
                         std::optional<std::string> signaling_id,
                         std::optional<std::string> offline_reason,
                         HeartbeatResponseCallback callback) override;
  void SendLiteHeartbeat(HeartbeatResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  void OnSendHeartbeatResponse(HeartbeatResponseCallback callback,
                               const ProtobufHttpStatus& status,
                               std::unique_ptr<Empty>);
  void OnUpdateRemoteAccessHostResponse(
      HeartbeatResponseCallback callback,
      const ProtobufHttpStatus& status,
      std::unique_ptr<internal::RemoteAccessHostV1Proto>);
  void OnReportHostOffline(HeartbeatResponseCallback callback,
                           const ProtobufHttpStatus& status,
                           std::unique_ptr<internal::RemoteAccessHostV1Proto>);
  void MakeUpdateRemoteAccessHostCall(
      std::optional<std::string> signaling_id,
      std::optional<std::string> offline_reason,
      CorpServiceClient::UpdateRemoteAccessHostCallback callback);
  void RunHeartbeatResponseCallback(HeartbeatResponseCallback callback,
                                    const ProtobufHttpStatus& status);

  // The entity to update in Directory service.
  std::string directory_id_;

  CorpServiceClient client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CorpHeartbeatServiceClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_HEARTBEAT_SERVICE_CLIENT_H_
