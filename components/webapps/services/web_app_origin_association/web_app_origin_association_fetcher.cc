// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "net/base/ip_address.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "url/gurl.h"

namespace {

constexpr size_t kMaxJsonSize = 1000000;  // 1MB max

int g_max_retry = 3;

network::SimpleURLLoader::RetryMode g_retry_mode =
    network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE;

constexpr net::NetworkTrafficAnnotationTag
    web_app_origin_association_traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "web_app_origin_association_download",
            R"(
      semantics {
          sender: "Web App Origin Association Fetcher"
          description:
            "PWAs can specify Scope Extensions in the Manifest. To verify the "
            "handlers, we download the corresponding web app origin "
            "association files."
          trigger:
            "A PWA that has Scope Extensions declared in the Manifest is "
            "installed, updated, or when DevTools displays Scope Extensions "
            "information to users."
          data:
            "Nothing."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
          "There is no setting to disable PWA installation."
          policy_exception_justification:
            "Not implemented, "
            "considered not necessary as no user data is sent."
     })");

constexpr char association_file_name[] =
    ".well-known/web-app-origin-association";

std::unique_ptr<network::SimpleURLLoader> CreateRequester(
    const GURL& url,
    network::mojom::IPAddressSpace initiator_address_space) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  // Block following redirects to prevent SSRF and origin takeover.
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;

  // Configure ClientSecurityState so that the network service's
  // LocalNetworkAccessChecker blocks access from more public to more private
  // address spaces.
  auto client_security_state = network::mojom::ClientSecurityState::New();
  client_security_state->ip_address_space = initiator_address_space;
  client_security_state->is_web_secure_context = true;
  client_security_state->local_network_access_request_policy =
      network::mojom::LocalNetworkAccessRequestPolicy::kBlock;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->client_security_state =
      std::move(client_security_state);

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request),
      web_app_origin_association_traffic_annotation);
  url_loader->SetRetryOptions(g_max_retry, g_retry_mode);
  url_loader->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
  url_loader->SetTimeoutDuration(base::Seconds(30));
  return url_loader;
}

// Fetching association file from a TLD or an otherwise invalid domain is not
// allowed. Pre-check IP literals and address spaces to reject SSRF and port
// scanning upfront.
bool ShouldFetchAssociationFile(
    const GURL& resource_url,
    network::mojom::IPAddressSpace initiator_address_space) {
  if (!resource_url.is_valid() || resource_url.is_empty()) {
    return false;
  }

  // Association files must be fetched over HTTPS.
  if (!resource_url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  if (resource_url.HostIsIPAddress()) {
    net::IPAddress address;
    if (!address.AssignFromIPLiteral(resource_url.HostNoBracketsPiece())) {
      return false;
    }

    // Link-local (including 169.254.0.0/16 cloud metadata), multicast, and
    // zero addresses must never be accessed.
    if (address.IsLinkLocal() || address.IsMulticast() || address.IsZero()) {
      return false;
    }
  }

  // Check if the target's address space is known from the URL (IP literals,
  // localhost, or .local domains).
  std::optional<network::mojom::IPAddressSpace> target_space =
      network::GetAddressSpaceFromUrl(resource_url);
  if (target_space.has_value()) {
    return !network::IsLessPublicAddressSpaceLNA(*target_space,
                                                 initiator_address_space);
  }

  const std::optional<size_t> registry_length =
      net::registry_controlled_domains::GetRegistry(
          resource_url,
          // Reject unknown registries (registries that don't have any matches
          // in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          // Skip matching private registries that allow external users to
          // specify sub-domains, e.g. glitch.me, as this is allowed.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES)
          .transform(&std::string_view::size);

  // Host cannot be a TLD or invalid.
  if (!registry_length.has_value() || *registry_length == 0 ||
      *registry_length >= resource_url.GetHost().length()) {
    return false;
  }

  return true;
}
}  // namespace

namespace webapps {

WebAppOriginAssociationFetcher::WebAppOriginAssociationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)) {}

WebAppOriginAssociationFetcher::~WebAppOriginAssociationFetcher() = default;

void WebAppOriginAssociationFetcher::SetRetryOptionsForTest(
    int max_retry,
    network::SimpleURLLoader::RetryMode retry_mode) {
  g_max_retry = max_retry;
  g_retry_mode = retry_mode;
}

void WebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
    const url::Origin& origin,
    network::mojom::IPAddressSpace initiator_address_space,
    FetchFileCallback callback) {
  const GURL resource_url = origin.GetURL().Resolve(association_file_name);
  if (!ShouldFetchAssociationFile(resource_url, initiator_address_space)) {
    // Do not proceed if |resource_url| is not valid.
    webapps::WebAppOriginAssociationMetrics::RecordFetchResult(
        webapps::WebAppOriginAssociationMetrics::FetchResult::
            kFetchFailedInvalidUrl);
    std::move(callback).Run(std::nullopt);
    return;
  }

  SendRequest(resource_url, initiator_address_space, std::move(callback));
}

void WebAppOriginAssociationFetcher::FetchWebAppOriginAssociationFile(
    const url::Origin& origin,
    FetchFileCallback callback) {
  FetchWebAppOriginAssociationFile(
      origin, network::mojom::IPAddressSpace::kUnknown, std::move(callback));
}

void WebAppOriginAssociationFetcher::SendRequest(
    const GURL& url,
    network::mojom::IPAddressSpace initiator_address_space,
    FetchFileCallback callback) {
  url_loader_ = CreateRequester(url, initiator_address_space);
  url_loader_->DownloadToString(
      shared_url_loader_factory_.get(),
      base::BindOnce(&WebAppOriginAssociationFetcher::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      kMaxJsonSize);
}

void WebAppOriginAssociationFetcher::OnResponse(
    FetchFileCallback callback,
    std::optional<std::string> response_body) {
  if (!response_body) {
    webapps::WebAppOriginAssociationMetrics::RecordFetchResult(
        webapps::WebAppOriginAssociationMetrics::FetchResult::
            kFetchFailedNoResponseBody);
  } else {
    webapps::WebAppOriginAssociationMetrics::RecordFetchResult(
        webapps::WebAppOriginAssociationMetrics::FetchResult::kFetchSucceed);
  }
  std::move(callback).Run(std::move(response_body));
}

}  // namespace webapps
