
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_heartbeat_service_client.h"

#include <string_view>

#include "base/strings/stringize_macros.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/host/host_details.h"
#include "remoting/host/version.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

CorpHeartbeatServiceClient::CorpHeartbeatServiceClient(
    const std::string& directory_id,
    const std::string& refresh_token,
    const std::string& service_account_email,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : directory_id_(directory_id),
      client_(refresh_token, service_account_email, url_loader_factory) {}

CorpHeartbeatServiceClient::~CorpHeartbeatServiceClient() = default;

void CorpHeartbeatServiceClient::SendFullHeartbeat(
    bool is_initial_heartbeat,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    HeartbeatResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (offline_reason.has_value() && !offline_reason->empty()) {
    // We ignore 'is_initial_heartbeat' because the host is offline so we're
    // just updating the Directory to indicate that, we don't need to send a
    // heartbeat afterwards.
    MakeUpdateRemoteAccessHostCall(
        signaling_id, offline_reason,
        base::BindOnce(&CorpHeartbeatServiceClient::OnReportHostOffline,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else if (is_initial_heartbeat) {
    MakeUpdateRemoteAccessHostCall(
        signaling_id, offline_reason,
        base::BindOnce(
            &CorpHeartbeatServiceClient::OnUpdateRemoteAccessHostResponse,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    SendLiteHeartbeat(std::move(callback));
  }
}

void CorpHeartbeatServiceClient::SendLiteHeartbeat(
    HeartbeatResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_.SendHeartbeat(
      directory_id_,
      base::BindOnce(&CorpHeartbeatServiceClient::OnSendHeartbeatResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CorpHeartbeatServiceClient::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_.CancelPendingRequests();
}

void CorpHeartbeatServiceClient::OnSendHeartbeatResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<Empty>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunHeartbeatResponseCallback(std::move(callback), status);
}

void CorpHeartbeatServiceClient::OnUpdateRemoteAccessHostResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<internal::RemoteAccessHostV1Proto>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status.ok()) {
    SendLiteHeartbeat(std::move(callback));
  } else {
    RunHeartbeatResponseCallback(std::move(callback), status);
  }
}

void CorpHeartbeatServiceClient::OnReportHostOffline(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<internal::RemoteAccessHostV1Proto>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunHeartbeatResponseCallback(std::move(callback), status);
}

void CorpHeartbeatServiceClient::MakeUpdateRemoteAccessHostCall(
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    CorpServiceClient::UpdateRemoteAccessHostCallback callback) {
  constexpr char kHostVersion[] = STRINGIZE(VERSION);
  client_.UpdateRemoteAccessHost(directory_id_, kHostVersion, signaling_id,
                                 offline_reason, GetHostOperatingSystemName(),
                                 GetHostOperatingSystemVersion(),
                                 std::move(callback));
}

void CorpHeartbeatServiceClient::RunHeartbeatResponseCallback(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO: joedow - Return wait interval from the service and pass it through.
  // TODO: joedow - Return primary email from the service and pass it through.
  std::move(callback).Run(status, /*wait_interval=*/std::nullopt,
                          /*primary_user_email=*/"",
                          /*require_session_authorization=*/std::nullopt,
                          /*use_lite_heartbeat=*/true);
}

}  // namespace remoting
