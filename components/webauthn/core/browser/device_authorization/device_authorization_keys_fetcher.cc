// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/device_authorization/device_authorization_keys_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/sync_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace webauthn {

DeviceAuthorizationKeysFetcher::DeviceAuthorizationKeysFetcher(
    version_info::Channel channel)
    : channel_(channel) {}

DeviceAuthorizationKeysFetcher::~DeviceAuthorizationKeysFetcher() = default;

void DeviceAuthorizationKeysFetcher::FetchDeviceAuthorizationKeys(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    FetchKeysCallback callback) {
  CHECK(url_loader_factory);
  CHECK(identity_manager);
  CHECK(callback);
  if (endpoint_fetcher_) {
    std::move(callback).Run(nullptr);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_authorization_key_fetch", R"(
        semantics {
          sender: "Password Manager"
          description:
            "A network request to retrieve user's device authorization keys "
            "for password manager passkeys"
          trigger:
            "Triggered when the user accesses or creates a passkey in the "
            "password manager, alternatively as a one-time operation for a "
            "device on startup"
          data:
            "OAuth2 access token"
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "rgod@google.com"
            }
            contacts {
              email: "thomasth@google.com"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
          last_reviewed: "2026-08-12"
        }
        policy {
          cookies_allowed: NO
          setting: "Users can disable Sync in Chrome settings."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
          }
        }
      )");

  endpoint_fetcher_ = std::make_unique<endpoint_fetcher::EndpointFetcher>(
      url_loader_factory, identity_manager,
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kPost, traffic_annotation)
          .SetAuthType(endpoint_fetcher::OAUTH)
          .SetConsentLevel(signin::ConsentLevel::kSignin)
          .SetOAuthConsumerId(
              signin::OAuthConsumerId::kDeviceAuthorizationRequest)
          .SetUrl(GURL(kDeviceAuthorizationKeyEndpointUrl))
          .SetHeaders(std::vector<
                      endpoint_fetcher::EndpointFetcher::RequestParams::Header>{
              {"User-Agent", syncer::MakeUserAgentForSync(channel_)}})
          .Build());
  endpoint_fetcher_->Fetch(
      base::BindOnce(&DeviceAuthorizationKeysFetcher::OnFetchCompleted,
                     base::Unretained(this), std::move(callback)));
}

void DeviceAuthorizationKeysFetcher::OnFetchCompleted(
    FetchKeysCallback callback,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  endpoint_fetcher_.reset();
  // TODO(crbug.com/405036154): Record metrics.
  std::move(callback).Run(std::move(response));
}

}  // namespace webauthn
