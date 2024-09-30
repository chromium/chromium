// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_heartbeat_service_client.h"

#include "base/notreached.h"
#include "remoting/base/directory_service_client.h"
#include "remoting/base/logging.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/host/host_details.h"
#include "remoting/proto/remoting/v1/directory_messages.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

Me2MeHeartbeatServiceClient::Me2MeHeartbeatServiceClient(
    const std::string& directory_id,
    bool set_fqdn,
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : directory_id_(directory_id),
      set_fqdn_(set_fqdn),
      client_(oauth_token_getter, url_loader_factory) {}

Me2MeHeartbeatServiceClient::~Me2MeHeartbeatServiceClient() = default;

void Me2MeHeartbeatServiceClient::SendFullHeartbeat(
    bool is_initial_heartbeat,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    HeartbeatResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string os_name = GetHostOperatingSystemName();
  std::string os_version = GetHostOperatingSystemVersion();

  client_.LegacyHeartbeat(
      directory_id_, signaling_id, offline_reason, is_initial_heartbeat,
      set_fqdn_, os_name, os_version,
      base::BindOnce(&Me2MeHeartbeatServiceClient::OnLegacyHeartbeatResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void Me2MeHeartbeatServiceClient::SendLiteHeartbeat(
    HeartbeatResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_.SendHeartbeat(
      directory_id_,
      base::BindOnce(&Me2MeHeartbeatServiceClient::OnSendHeartbeatResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void Me2MeHeartbeatServiceClient::CancelPendingRequests() {
  client_.CancelPendingRequests();
}

void Me2MeHeartbeatServiceClient::OnLegacyHeartbeatResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::HeartbeatResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta wait_interval =
      base::Seconds(response->set_interval_seconds());
  bool use_lite_heartbeat = false;
  if (response->use_lite_heartbeat()) {
    use_lite_heartbeat = true;
  }
  std::string primary_email;
  if (!response->primary_user_email().empty()) {
    primary_email = response->primary_user_email();
  }
  bool require_session_auth = false;
  if (response->has_require_session_authorization()) {
    require_session_auth = response->require_session_authorization();
  }

  std::move(callback).Run(status, std::make_optional(wait_interval),
                          primary_email, require_session_auth,
                          std::make_optional(use_lite_heartbeat));
}

void Me2MeHeartbeatServiceClient::OnSendHeartbeatResponse(
    HeartbeatResponseCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::SendHeartbeatResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta wait_interval =
      base::Seconds(response->wait_interval_seconds());
  std::move(callback).Run(status, std::make_optional(wait_interval), "",
                          std::nullopt, std::nullopt);
}

}  // namespace remoting
