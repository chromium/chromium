// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/network_request_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/connection_allowlist_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace content::webid {

using DownloadCallback = NetworkRequestManager::DownloadCallback;
using ParseJsonCallback = NetworkRequestManager::ParseJsonCallback;

namespace {
constexpr char kApplicationJson[] = "application/json";
// Body content types.
constexpr char kUrlEncodedContentType[] = "application/x-www-form-urlencoded";

// Host prefix prepended to the eTLD+1 to form the FedCM well-known host:
// "web-identity.well-known.<eTLD+1>".
constexpr char kWebIdentitySubdomainHostPrefix[] = "web-identity.well-known.";

// 1 MiB is an arbitrary upper bound that should account for any reasonable
// response size that is a part of this protocol.
constexpr int maxResponseSizeInKiB = 1024;

ParseStatus GetResponseError(base::optional_ref<std::string> response_body,
                             int response_code,
                             const std::string& mime_type) {
  if (response_code == net::ERR_NETWORK_ACCESS_REVOKED) {
    return ParseStatus::kBlockedByConnectionAllowlist;
  }

  if (response_code == net::HTTP_NOT_FOUND) {
    return ParseStatus::kHttpNotFoundError;
  }

  if (!response_body) {
    return ParseStatus::kNoResponseError;
  }

  if (!blink::IsJSONMimeType(mime_type)) {
    return ParseStatus::kInvalidContentTypeError;
  }

  return ParseStatus::kSuccess;
}

void OnDownloadedJson(ParseJsonCallback parse_json_callback,
                      std::optional<std::string> response_body,
                      int response_code,
                      const std::string& mime_type,
                      bool cors_error) {
  ParseStatus parse_status =
      GetResponseError(response_body, response_code, mime_type);

  if (parse_status != ParseStatus::kSuccess) {
    std::move(parse_json_callback)
        .Run({parse_status, response_code, cors_error}, std::nullopt);
    return;
  }

  std::optional<base::DictValue> value =
      base::JSONReader::ReadDict(*response_body, base::JSON_PARSE_RFC);

  std::move(parse_json_callback)
      .Run({value ? ParseStatus::kSuccess : ParseStatus::kInvalidResponseError,
            response_code, cors_error},
           std::move(value));
}

}  // namespace

GURL ExtractEndpoint(const GURL& provider,
                     const base::DictValue& response,
                     const char* key) {
  const std::string* endpoint = response.FindString(key);
  if (!endpoint || endpoint->empty()) {
    return GURL();
  }
  return provider.Resolve(*endpoint);
}

std::optional<GURL> ComputeWellKnownUrl(const GURL& provider,
                                        const std::string& path) {
  GURL well_known_url;
  if (net::IsLocalhost(provider) || IsPreservePortsForTestingEnabled()) {
    well_known_url = provider.GetWithEmptyPath();
  } else {
    std::string etld_plus_one = GetDomainAndRegistry(
        provider, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (etld_plus_one.empty()) {
      return std::nullopt;
    }
    well_known_url = GURL(provider.GetScheme() + "://" + etld_plus_one);
  }

  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return well_known_url.ReplaceComponents(replacements);
}

std::optional<GURL> ComputeWebIdentitySubdomainWellKnownUrl(
    const GURL& provider,
    const std::string& path) {
  // The subdomain form is only meaningful for hosts that have a registrable
  // domain. Localhost / test ports / IP literals continue to use the apex
  // (legacy) URL.
  if (net::IsLocalhost(provider) || provider.HostIsIPAddress() ||
      IsPreservePortsForTestingEnabled()) {
    return std::nullopt;
  }

  GURL site_url = net::SchemefulSite(provider).GetURL();
  if (!site_url.is_valid() || site_url.host().empty()) {
    return std::nullopt;
  }

  GURL::Replacements replacements;
  std::string subdomain_host =
      base::StrCat({kWebIdentitySubdomainHostPrefix, site_url.host()});
  replacements.SetHostStr(subdomain_host);
  replacements.SetPathStr(path);
  return site_url.ReplaceComponents(replacements);
}

NetworkRequestManager::NetworkRequestManager(
    const url::Origin& relying_party_origin,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    network::mojom::ClientSecurityStatePtr client_security_state,
    network::mojom::RequestDestination destination,
    FrameTreeNodeId frame_tree_node_id,
    WeakDocumentPtr initiator_document)
    : relying_party_origin_(relying_party_origin),
      loader_factory_(loader_factory),
      client_security_state_(std::move(client_security_state)),
      destination_(destination),
      frame_tree_node_id_(frame_tree_node_id),
      initiator_document_(std::move(initiator_document)) {}

NetworkRequestManager::~NetworkRequestManager() = default;

void NetworkRequestManager::DownloadJsonAndParse(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::optional<std::string> url_encoded_post_data,
    ParseJsonCallback parse_json_callback,
    bool allow_http_error_results) {
  DownloadUrl(std::move(resource_request), std::move(url_encoded_post_data),
              base::BindOnce(&OnDownloadedJson, std::move(parse_json_callback)),
              /*max_download_size=*/maxResponseSizeInKiB * 1024,
              allow_http_error_results);
}

void NetworkRequestManager::DownloadUrl(
    std::unique_ptr<network::ResourceRequest> resource_request,
    std::optional<std::string> url_encoded_post_data,
    DownloadCallback callback,
    size_t max_download_size,
    bool allow_http_error_results) {
  RenderFrameHost* render_frame_host =
      initiator_document_.AsRenderFrameHostIfValid();

  if (!render_frame_host) {
    // If the initiator frame of the invoking API has gone (e.g. user closes the
    // tab). The request is aborted. This is because:
    // 1. The connection allowlist is stored in the initiator frame's policy
    //    container. Since the frame has been destroyed, there is no connection
    //    allowlist to check.
    // 2. It is safe to abort the requests because they are no longer useful
    //    as the initiator frame owning the API that requires the requests has
    //    been destroyed.
    if (callback) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&NetworkRequestManager::OnRequestBlocked,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), net::ERR_ABORTED));
    }

    return;
  }

  if (url_encoded_post_data) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                        kUrlEncodedContentType);
  }

  // Prepare DevTools instrumentation for the request upfront.
  auto request_id = base::UnguessableToken::Create();
  devtools_instrumentation::MaybeAssignResourceRequestId(
      frame_tree_node_id_, request_id.ToString(), *resource_request);
  if (resource_request->devtools_request_id.has_value()) {
    devtools_instrumentation::WillSendFedCmNetworkRequest(
        frame_tree_node_id_, *resource_request, url_encoded_post_data);
  }

  if (!content::FrameConnectionAllowlistAllowsRequestAndReportIfNeeded(
          render_frame_host, resource_request->url,
          /*is_redirect=*/false)) {
    // The request URL is not allowed by the initiator frame's connection
    // allowlist. See: https://github.com/WICG/connection-allowlists.

    if (resource_request->devtools_request_id.has_value()) {
      // Notify the dev tools that the request is blocked with network error:
      // `net::ERR_NETWORK_ACCESS_REVOKED`.
      devtools_instrumentation::DidReceiveFedCmNetworkResponse(
          frame_tree_node_id_, request_id.ToString(), resource_request->url,
          /*response_head=*/nullptr, /*response_body=*/"",
          network::URLLoaderCompletionStatus(net::ERR_NETWORK_ACCESS_REVOKED));
    }

    if (callback) {
      // Run the download callback asynchronously.
      //
      // The callback must not be run synchronously. This is because
      // `ConfigFetcher::Start()` initiates multiple network requests in a loop
      // iterating over its member `fetch_results_`.
      // If the download callback is run synchronously here:
      // 1. The download callback, which is the completion handler (either
      //    `OnWellKnownFetched` or `OnConfigFetched`), runs immediately.
      // 2. Assume this is the last request which is a config request and it is
      //    blocked by the connection allowlist. In `OnConfigFetched`,
      //    `ConfigFetcher::RunCallbackIfDone()` will be called and triggers
      //    `AccountsFetcher::OnAllConfigAndWellKnownFetched()`.
      // 3. That function deletes the `ConfigFetcher` object because there are
      //    no pending requests.
      // 4. When the stack unwinds back to `ConfigFetcher::Start()`, the loop
      //    tries to continue but the `ConfigFetcher` has already been deleted.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&NetworkRequestManager::OnRequestBlocked,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         net::ERR_NETWORK_ACCESS_REVOKED));
    }

    return;
  }

  network::ResourceRequest* resource_request_ptr = resource_request.get();

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       CreateTrafficAnnotation());

  network::SimpleURLLoader* url_loader_ptr = url_loader.get();

  if (resource_request_ptr->devtools_request_id.has_value()) {
    urlloader_devtools_request_id_map_[url_loader_ptr] = request_id;
  }

  if (url_encoded_post_data) {
    url_loader->AttachStringForUpload(*url_encoded_post_data,
                                      kUrlEncodedContentType);
    if (allow_http_error_results) {
      url_loader->SetAllowHttpErrorResults(true);
    }
  }

  // Callback is a member of NetworkRequestManager in order to cancel callback
  // if NetworkRequestManager object is destroyed prior to callback being run.
  url_loader_ptr->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&NetworkRequestManager::OnDownloadedUrl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(callback)),
      max_download_size);
}

void NetworkRequestManager::OnDownloadedUrl(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    DownloadCallback callback,
    std::optional<std::string> response_body) {
  auto* response_info = url_loader->ResponseInfo();
  // Use the HTTP response code, if available. If it is not available, use the
  // NetError(). Note that it is acceptable to put these in the same int because
  // NetErrors are not positive, so they do not conflict with HTTP error codes.
  int response_code = response_info && response_info->headers
                          ? response_info->headers->response_code()
                          : url_loader->NetError();

  std::optional<network::URLLoaderCompletionStatus> status =
      url_loader->CompletionStatus();

  // Notify DevTools about the response
  auto it = urlloader_devtools_request_id_map_.find(url_loader.get());
  if (it != urlloader_devtools_request_id_map_.end()) {
    auto request_id = it->second;
    const std::string& response_body_str =
        response_body.value_or(std::string());
    auto completion_status = status.value_or(
        network::URLLoaderCompletionStatus(url_loader->NetError()));

    devtools_instrumentation::DidReceiveFedCmNetworkResponse(
        frame_tree_node_id_, request_id.ToString(), url_loader->GetFinalURL(),
        response_info, response_body_str, completion_status);

    // Remove the entry from the map
    urlloader_devtools_request_id_map_.erase(it);
  }

  if (!callback) {
    // For the metrics endpoint, we do not care about the result.
    return;
  }

  std::string mime_type;
  if (response_info && response_info->headers) {
    response_info->headers->GetMimeType(&mime_type);
  }

  // Check for CORS error
  bool cors_error = false;
  if (status && status.value().cors_error_status.has_value()) {
    cors_error = true;
  }

  std::move(callback).Run(std::move(response_body), response_code,
                          std::move(mime_type), cors_error);
}

void NetworkRequestManager::OnRequestBlocked(DownloadCallback callback,
                                             int response_code) {
  if (callback) {
    std::move(callback).Run(/*response_body=*/std::nullopt, response_code,
                            /*mime_type=*/"", /*cors_error=*/false);
  }
}

std::unique_ptr<network::ResourceRequest>
NetworkRequestManager::CreateUncredentialedResourceRequest(
    const GURL& target_url,
    bool send_origin,
    bool follow_redirects) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url = target_url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kApplicationJson);
  resource_request->destination = destination_;
  // See https://github.com/fedidcg/FedCM/issues/379 for why the Origin header
  // is sent instead of the Referrer header.
  if (send_origin) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                        relying_party_origin_.Serialize());
    DCHECK(!follow_redirects);
  }
  if (follow_redirects &&
      content::FrameConnectionAllowlistAllowsRequestAndReportIfNeeded(
          initiator_document_.AsRenderFrameHostIfValid(), resource_request->url,
          /*is_redirect=*/true)) {
    // Only follow redirects if the initiator frame's connection allowlist
    // allows redirects. Otherwise, set the `redirect_mode` to `kError` so that
    // redirects will not be followed.
    // TODO(crbug.com/482728970): The connection allowlist check on redirect for
    // `NetworkRequestManager` is implemented in a different way compared with
    // the usual approach used for most other network requests. Instead of
    // checking the connection allowlists redirect directive when the redirect
    // takes place, `NetworkRequestManager` decides whether the resource request
    // follows the redirect during request initiation. This is because the
    // request will use the URLLoaderFactory associated with the browser
    // process, which makes it difficult to retrieve the connection allowlist
    // during the redirect. For connection allowlists reporting, special
    // handling may be required.
    resource_request->redirect_mode = network::mojom::RedirectMode::kFollow;
  } else {
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  }
  resource_request->request_initiator = url::Origin();
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/relying_party_origin_,
      /*frame_origin=*/url::Origin::Create(target_url), net::SiteForCookies(),
      /*nonce=*/std::nullopt,
      net::NetworkIsolationPartition::kFedCmUncredentialedRequests);
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();
  return resource_request;
}

std::unique_ptr<network::ResourceRequest>
NetworkRequestManager::CreateCredentialedResourceRequest(
    const GURL& target_url,
    CredentialedResourceRequestType type) const {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  auto target_origin = url::Origin::Create(target_url);
  auto site_for_cookies = net::SiteForCookies::FromOrigin(target_origin);

  // Setting the initiator to relying_party_origin_ ensures that we don't send
  // SameSite=Strict cookies.
  resource_request->request_initiator = relying_party_origin_;

  resource_request->destination = destination_;
  resource_request->url = target_url;
  resource_request->site_for_cookies = site_for_cookies;
  // TODO(crbug.com/40284123): Figure out why when using CORS we still need to
  // explicitly pass the Origin header.
  if (type != CredentialedResourceRequestType::kNoOrigin) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                        relying_party_origin_.Serialize());
  }
  if (type == CredentialedResourceRequestType::kOriginWithCORS) {
    resource_request->mode = network::mojom::RequestMode::kCors;
    resource_request->request_initiator = relying_party_origin_;
  }
  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kApplicationJson);

  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  net::IsolationInfo::RequestType request_type =
      net::IsolationInfo::RequestType::kOther;
  if (IsSameSiteLaxEnabled()) {
    // We use kMainFrame so that we can send SameSite=Lax cookies.
    request_type = net::IsolationInfo::RequestType::kMainFrame;
  }
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      request_type, /*top_frame_origin=*/target_origin,
      /*frame_origin=*/target_origin, site_for_cookies);
  DCHECK(client_security_state_);
  resource_request->trusted_params->client_security_state =
      client_security_state_.Clone();
  return resource_request;
}

}  // namespace content::webid
