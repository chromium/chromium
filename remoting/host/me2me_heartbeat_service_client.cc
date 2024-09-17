// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/me2me_heartbeat_service_client.h"

#include "base/notreached.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/protobuf_http_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

Me2MeHeartbeatServiceClient::Me2MeHeartbeatServiceClient(
    const std::string& directory_id,
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::optional<std::string> fqdn)
    : directory_id_(directory_id) {}

Me2MeHeartbeatServiceClient::~Me2MeHeartbeatServiceClient() = default;

void Me2MeHeartbeatServiceClient::UpdateRemoteAccessHost(
    bool is_initial_heartbeat,
    std::optional<std::string> ftl_signaling_id,
    std::optional<std::string> offline_reason,
    UpdateRemoteAccessHostResponseCallback callback) {
  NOTIMPLEMENTED();
}

void Me2MeHeartbeatServiceClient::SendHeartbeat(
    SendHeartbeatResponseCallback callback) {
  NOTIMPLEMENTED();
}

void Me2MeHeartbeatServiceClient::CancelPendingRequests() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
