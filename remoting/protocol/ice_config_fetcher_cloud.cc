// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config_fetcher_cloud.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/checked_math.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/google/internal/remoting/cloud/v1alpha/network_traversal_service.pb.h"
#include "remoting/protocol/ice_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
using google::internal::remoting::cloud::v1alpha::GenerateIceConfigResponse;
}

namespace remoting::protocol {

IceConfigFetcherCloud::IceConfigFetcherCloud(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuthTokenGetter* oauth_token_getter)
    : service_client_(oauth_token_getter, url_loader_factory) {}

IceConfigFetcherCloud::~IceConfigFetcherCloud() = default;

void IceConfigFetcherCloud::GetIceConfig(OnIceConfigCallback callback) {
  CHECK(callback);

  service_client_.GenerateIceConfig(
      base::BindOnce(&IceConfigFetcherCloud::OnResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IceConfigFetcherCloud::OnResponse(
    OnIceConfigCallback callback,
    const ProtobufHttpStatus& status,
    std::unique_ptr<GenerateIceConfigResponse> response) {
  if (!status.ok()) {
    LOG(ERROR) << "GenerateIceConfig request failed.  Error code: "
               << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
    std::move(callback).Run(std::nullopt);
    return;
  }

  IceConfig ice_config;
  for (const auto& server : response->stun_servers()) {
    for (const auto& url : server.urls()) {
      ice_config.AddStunServer(url);
    }
  }
  // Holds the lowest allowed bitrate for any given TURN server. This is because
  // we don't know which server is used for the P2P connection so we can't apply
  // a specific bitrate limit based on that so instead we just choose the lowest
  // positive value provided.
  std::optional<int64_t> max_bitrate_kbps;
  for (const auto& server : response->turn_servers()) {
    for (const auto& url : server.urls()) {
      ice_config.AddServer(url, server.username(), server.credential());
    }
    if (server.has_max_rate_kbps() && server.max_rate_kbps() > 0) {
      max_bitrate_kbps =
          std::min(max_bitrate_kbps.value_or(server.max_rate_kbps()),
                   server.max_rate_kbps());
    }
  }
  if (max_bitrate_kbps.has_value()) {
    ice_config.max_bitrate_kbps =
        base::CheckedNumeric<int>(*max_bitrate_kbps).ValueOrDie();
  }
  if (ice_config.stun_servers.empty() && ice_config.turn_servers.empty()) {
    ice_config.expiration_time = base::Time::Now();
  } else {
    const auto& lifetime_duration = response->lifetime_duration();
    base::TimeDelta lifetime = base::Seconds(lifetime_duration.seconds()) +
                               base::Nanoseconds(lifetime_duration.nanos());
    ice_config.expiration_time = base::Time::Now() + lifetime;
  }

  std::move(callback).Run(std::move(ice_config));
}

}  // namespace remoting::protocol
