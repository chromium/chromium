// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/cloud_heartbeat_service_client.h"

#include "base/notreached.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "remoting/base/protobuf_http_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

CloudHeartbeatServiceClient::CloudHeartbeatServiceClient(
    const std::string& directory_id,
    const std::string& api_key,
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : directory_id_(directory_id), api_key_(api_key) {}

CloudHeartbeatServiceClient::~CloudHeartbeatServiceClient() = default;

void CloudHeartbeatServiceClient::SendFullHeartbeat(
    bool is_initial_heartbeat,
    std::optional<std::string> signaling_id,
    std::optional<std::string> offline_reason,
    HeartbeatResponseCallback callback) {
  NOTIMPLEMENTED();
}

void CloudHeartbeatServiceClient::SendLiteHeartbeat(
    HeartbeatResponseCallback callback) {
  NOTIMPLEMENTED();
}

void CloudHeartbeatServiceClient::CancelPendingRequests() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
