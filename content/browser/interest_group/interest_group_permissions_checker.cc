// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_permissions_checker.h"

#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_permissions_cache.h"
#include "content/public/browser/global_request_id.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("interest_group_well_known_fetcher", R"(
        semantics {
          sender: "Interest group well-known fetcher"
          description:
            "When a website tries to join or leave an interest group owned "
            "by another origin, a .well-known URL needs to be fetched to "
            "check if interest group owner has delegated permissions to that "
            "site to perform such an operation. "
            "See https://github.com/WICG/turtledove/blob/main/FLEDGE.md"
          trigger:
            "Cross-origin navigator.joinAdInterestGroup() and "
            "navigator.leaveAdInterestGroup() calls."
          data: "URL registered for updating this interest group."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this via Settings > Privacy and Security > Ads "
            "privacy > Site-suggested ads."
          chrome_policy {
            PrivacySandboxSiteEnabledAdsEnabled {
              PrivacySandboxSiteEnabledAdsEnabled: false
            }
          }
        })");

}  // namespace

constexpr base::TimeDelta InterestGroupPermissionsChecker::kRequestTimeout =
    base::Seconds(30);

InterestGroupPermissionsChecker::ActiveRequest::ActiveRequest() = default;
InterestGroupPermissionsChecker::ActiveRequest::~ActiveRequest() = default;

InterestGroupPermissionsChecker::PendingPermissionsCheck::
    PendingPermissionsCheck(Operation operation,
                            PermissionsCheckCallback permissions_check_callback)
    : operation(operation),
      permissions_check_callback(std::move(permissions_check_callback)) {}

InterestGroupPermissionsChecker::PendingPermissionsCheck::
    PendingPermissionsCheck(PendingPermissionsCheck&&) = default;
InterestGroupPermissionsChecker::PendingPermissionsCheck::
    ~PendingPermissionsCheck() = default;

InterestGroupPermissionsChecker::InterestGroupPermissionsChecker() = default;

InterestGroupPermissionsChecker::~InterestGroupPermissionsChecker() = default;

void InterestGroupPermissionsChecker::CheckPermissions(
    Operation operation,
    const url::Origin& frame_origin,
    const url::Origin& interest_group_owner,
    const net::NetworkIsolationKey& network_isolation_key,
    network::mojom::URLLoaderFactory& url_loader_factory,
    PermissionsCheckCallback permissions_check_callback) {
  // Only HTTPS frames can join or leave interest groups, and only HTTPS origins
  // can own interest groups.
  DCHECK_EQ(frame_origin.scheme(), url::kHttpsScheme);
  DCHECK_EQ(interest_group_owner.scheme(), url::kHttpsScheme);

  // Same origin operations are always allowed. Need to invoke callback
  // synchronously in this case, so if a page adds an interest group and then
  // runs an auction immediately, the interest group is guaranteed to have been
  // added before the auction searches for applicable interest groups.
  if (frame_origin == interest_group_owner) {
    std::move(permissions_check_callback).Run(true);
    return;
  }

  Permissions* permissions = cache_.GetPermissions(
      frame_origin, interest_group_owner, network_isolation_key);
  if (permissions) {
    // If the result is cached, there shouldn't be a pending request for it.
    DCHECK_EQ(0u, active_requests_.count({frame_origin, interest_group_owner,
                                          network_isolation_key}));
    std::move(permissions_check_callback)
        .Run(AllowsOperation(*permissions, operation));
    return;
  }

  PermissionsKey key{frame_origin, interest_group_owner, network_isolation_key};
  auto active_request = active_requests_.find(key);
  if (active_request == active_requests_.end()) {
    active_request = active_requests_
                         .emplace(std::make_pair(
                             std::move(key), std::make_unique<ActiveRequest>()))
                         .first;

    auto resource_request = std::make_unique<network::ResourceRequest>();

    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    // These requests are JSON requests made using a URLLoaderFactory matching
    // the one created for the renderer process. Therefore, CORS needs to be
    // enabled to avoid ORB blocking.
    //
    // TODO(mmenke): Figure if we really need CORS.
    resource_request->mode = network::mojom::RequestMode::kCors;
    resource_request->request_initiator = frame_origin;

    // Construct .well-known URL.
    GURL url = interest_group_owner.GetURL();
    GURL::Replacements replacements;
    replacements.SetPathStr("/.well-known/interest-group/permissions/");
    std::string query = base::StrCat(
        {"origin=", base::EscapeQueryParamValue(frame_origin.Serialize(),
                                                /*use_plus=*/false)});
    replacements.SetQueryStr(query);
    resource_request->url = url.ReplaceComponents(replacements);
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                        "application/json");

    active_request->second->simple_url_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         kTrafficAnnotation);
    active_request->second->simple_url_loader->SetTimeoutDuration(
        kRequestTimeout);
    active_request->second->simple_url_loader->SetRequestID(
        GlobalRequestID::MakeBrowserInitiated().request_id);
    active_request->second->simple_url_loader->DownloadToString(
        &url_loader_factory,
        base::BindOnce(&InterestGroupPermissionsChecker::OnRequestComplete,
                       base::Unretained(this), active_request),
        kMaxBodySize);
  }

  active_request->second->pending_checks.emplace_back(PendingPermissionsCheck{
      /*operation=*/operation,
      /*permissions_check_callback=*/std::move(permissions_check_callback)});
}

void InterestGroupPermissionsChecker::ClearCache() {
  cache_.Clear();
}

void InterestGroupPermissionsChecker::OnRequestComplete(
    ActiveRequestMap::iterator active_request,
    std::unique_ptr<std::string> response_body) {
  const auto* response_info =
      active_request->second->simple_url_loader->ResponseInfo();
  if (!response_body || !response_info ||
      !blink::IsJSONMimeType(response_info->mime_type)) {
    OnActiveRequestComplete(active_request, Permissions());
    return;
  }

  // `simple_url_loader` is no longer needed after this point.
  active_request->second->simple_url_loader.reset();

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&InterestGroupPermissionsChecker::OnJsonParsed,
                     weak_factory_.GetWeakPtr(), active_request));
}

void InterestGroupPermissionsChecker::OnJsonParsed(
    ActiveRequestMap::iterator active_request,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    OnActiveRequestComplete(active_request, Permissions());
    return;
  }

  std::optional<bool> can_join =
      result->GetDict().FindBool("joinAdInterestGroup");
  std::optional<bool> can_leave =
      result->GetDict().FindBool("leaveAdInterestGroup");
  Permissions permissions{/*can_join=*/can_join.value_or(false),
                          /*can_leave=*/can_leave.value_or(false)};
  OnActiveRequestComplete(active_request, permissions);
}

void InterestGroupPermissionsChecker::OnActiveRequestComplete(
    ActiveRequestMap::iterator active_request,
    Permissions permissions) {
  // Add permissions to cache, regardless of where they came from (failed
  // request, bad response, valid JSON).
  cache_.CachePermissions(permissions, active_request->first.frame_origin,
                          active_request->first.interest_group_owner,
                          active_request->first.network_isolation_key);

  auto pending_checks = std::move(active_request->second->pending_checks);
  active_requests_.erase(active_request);
  for (auto& pending_check : pending_checks) {
    std::move(pending_check.permissions_check_callback)
        .Run(AllowsOperation(permissions, pending_check.operation));
  }
}

bool InterestGroupPermissionsChecker::AllowsOperation(Permissions permissions,
                                                      Operation operation) {
  switch (operation) {
    case Operation::kJoin:
      return permissions.can_join;
    case Operation::kLeave:
      return permissions.can_leave;
  }
}

}  // namespace content
