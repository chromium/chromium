// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cloud_heartbeat_service_client.h"

#include "base/functional/callback.h"
#include "base/strings/stringize_macros.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/host/host_details.h"
#include "remoting/host/version.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/empty.pb.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/remote_access_host.pb.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/remote_access_service.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {
using Empty = google::internal::remoting::cloud::v1alpha::Empty;
using RemoteAccessHost =
    google::internal::remoting::cloud::v1alpha::RemoteAccessHost;
using SendHeartbeatRequest =
    google::internal::remoting::cloud::v1alpha::SendHeartbeatRequest;
using UpdateRemoteAccessHostRequest =
    google::internal::remoting::cloud::v1alpha::UpdateRemoteAccessHostRequest;
}  // namespace

CloudHeartbeatServiceClient::CloudHeartbeatServiceClient(
    const std::string& directory_id,
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : directory_id_(directory_id),
      client_(/*api_key=*/"", oauth_token_getter, url_loader_factory) {}

CloudHeartbeatServiceClient::~CloudHeartbeatServiceClient() = default;

void CloudHeartbeatServiceClient::SendFullHeartbeat(
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
        base::BindOnce(&CloudHeartbeatServiceClient::OnReportHostOffline,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else if (is_initial_heartbeat) {
    MakeUpdateRemoteAccessHostCall(
        signaling_id, offline_reason,
        base::BindOnce(
            &CloudHeartbeatServiceClient::OnUpdateRemoteAccessHostResponse,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    client_.SendHeartbeat(
        directory_id_,
        base::BindOnce(&CloudHeartbeatServiceClient::OnSendHeartbeatResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void CloudHeartbeatServiceClient::SendLiteHeartbeat(
    HeartbeatResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_.SendHeartbeat(
      directory_id_,
      base::BindOnce(&CloudHeartbeatServiceClient::OnSendHeartbeatResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CloudHeartbeatServiceClient::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_.CancelPendingRequests();
}

void CloudHeartbeatServiceClient::OnSendHeartbeatResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<Empty>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunHeartbeatResponseCallback(std::move(callback), status);
}

void CloudHeartbeatServiceClient::OnUpdateRemoteAccessHostResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<RemoteAccessHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.ok()) {
    SendLiteHeartbeat(std::move(callback));
  } else {
    RunHeartbeatResponseCallback(std::move(callback), status);
  }
}

void CloudHeartbeatServiceClient::OnReportHostOffline(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<RemoteAccessHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunHeartbeatResponseCallback(std::move(callback), status);
}

void CloudHeartbeatServiceClient::MakeUpdateRemoteAccessHostCall(
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    CloudServiceClient::UpdateRemoteAccessHostCallback callback) {
  constexpr auto* host_version = STRINGIZE(VERSION);
  client_.UpdateRemoteAccessHost(directory_id_, host_version, signaling_id,
                                 offline_reason, GetHostOperatingSystemName(),
                                 GetHostOperatingSystemVersion(),
                                 std::move(callback));
}

void CloudHeartbeatServiceClient::RunHeartbeatResponseCallback(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cloud hosts always require session authorization and do not support
  // changing the email address of the primary user. This service client always
  // uses 'lite' heartbeats.
  // TODO: joedow - Return wait interval from the service and pass it through.
  std::move(callback).Run(status, /*wait_interval=*/std::nullopt,
                          /*primary_user_email=*/"",
                          /*require_session_authorization=*/true,
                          /*use_lite_heartbeat=*/true);
}

}  // namespace remoting
