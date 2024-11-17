// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_config_fetcher_default.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/proto/remoting/v1/network_traversal_messages.pb.h"
#include "remoting/protocol/ice_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting::protocol {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remoting_ice_config_request",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Request used by Chrome Remote Desktop to fetch ICE (Interactive "
            "Connectivity Establishment) configuration, which contains list of "
            "STUN (Session Traversal Utilities for NAT) & TURN (Traversal "
            "Using Relay NAT) servers and TURN credentials. Please refer to "
            "https://tools.ietf.org/html/rfc5245 for more details."
          trigger:
            "When a Chrome Remote Desktop session is being connected and "
            "periodically (less frequent than once per hour) while a session "
            "is active, if the configuration is expired."
          user_data {
            type: ACCESS_TOKEN
          }
          data:
            "OAuth token for the following use cases:"
            " 1) Human user (for consumer It2Me) "
            " 2) ChromeOS Enterprise robot (for commercial It2Me) "
            " 3) Chromoting robot (for remote access, aka Me2Me) "
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts { owners: "//remoting/OWNERS" }
          }
          last_reviewed: "2023-07-28"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          chrome_policy {
            RemoteAccessHostFirewallTraversal {
              RemoteAccessHostFirewallTraversal: false
            }
          }
        })");

constexpr char kGetIceConfigPath[] = "/v1/networktraversal:geticeconfig";

}  // namespace

IceConfigFetcherDefault::IceConfigFetcherDefault(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuthTokenGetter* oauth_token_getter)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   oauth_token_getter,
                   url_loader_factory) {
  // |oauth_token_getter| is allowed to be null if the caller wants the request
  // to be unauthenticated.
  make_authenticated_requests_ = oauth_token_getter != nullptr;
}

IceConfigFetcherDefault::~IceConfigFetcherDefault() = default;

void IceConfigFetcherDefault::GetIceConfig(OnIceConfigCallback callback) {
  DCHECK(!on_ice_config_callback_);
  DCHECK(callback);

  on_ice_config_callback_ = std::move(callback);

  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kGetIceConfigPath;
  request_config->request_message =
      std::make_unique<apis::v1::GetIceConfigRequest>();
  if (!make_authenticated_requests_) {
    // TODO(joedow): Remove this after we no longer have any clients/hosts which
    // call this API in an unauthenticated fashion.
    request_config->authenticated = false;
    request_config->api_key = google_apis::GetRemotingAPIKey();
  }
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(base::BindOnce(
      &IceConfigFetcherDefault::OnResponse, base::Unretained(this)));
  http_client_.ExecuteRequest(std::move(request));
}

void IceConfigFetcherDefault::OnResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::GetIceConfigResponse> response) {
  DCHECK(!on_ice_config_callback_.is_null());

  if (!status.ok()) {
    LOG(ERROR) << "Received error code: "
               << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
    std::move(on_ice_config_callback_).Run(std::nullopt);
    return;
  }
  // TODO: joedow - Update Parse() to return nullopt instead.
  auto ice_config = IceConfig::Parse(*response);
  if (!ice_config.is_null()) {
    std::move(on_ice_config_callback_).Run(std::move(ice_config));
  } else {
    std::move(on_ice_config_callback_).Run(std::nullopt);
  }
}

}  // namespace remoting::protocol
