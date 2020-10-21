// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/network/origin_policy/origin_policy_fetcher.h"

#include <utility>

#include "base/strings/strcat.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "services/network/origin_policy/origin_policy_parser.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

OriginPolicyFetcher::OriginPolicyFetcher(
    OriginPolicyManager* owner_policy_manager,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    mojom::URLLoaderFactory* factory,
    mojom::OriginPolicyManager::RetrieveOriginPolicyCallback callback)
    : owner_policy_manager_(owner_policy_manager),
      fetch_url_(GetPolicyURL(origin)),
      isolation_info_(isolation_info),
      callback_(std::move(callback)) {
  DCHECK(callback_);
  DCHECK(!isolation_info.IsEmpty());
  // Policy requests shouldn't update frame origins on redirect.
  DCHECK_EQ(net::IsolationInfo::RequestType::kOther,
            isolation_info.request_type());
  // While they use CredentialsMode::kOmit, so it shouldn't matter, policy
  // requests should have a null SiteForCookies.
  DCHECK(isolation_info.site_for_cookies().IsNull());
  FetchPolicy(factory);
}

OriginPolicyFetcher::~OriginPolicyFetcher() = default;

// static
GURL OriginPolicyFetcher::GetPolicyURL(const url::Origin& origin) {
  return GURL(base::StrCat({origin.Serialize(), kOriginPolicyWellKnown}));
}

void OriginPolicyFetcher::FetchPolicy(mojom::URLLoaderFactory* factory) {
  // Create the traffic annotation
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("origin_policy_loader", R"(
        semantics {
          sender: "Origin Policy URL Loader Throttle"
          description:
            "Fetches the Origin Policy from an origin."
          trigger:
            "The server has used the Origin-Policy header to request that an "
            "origin policy be applied."
          data:
            "None; the URL itself contains the origin."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings. Servers "
            "opt in or out of this mechanism."
          policy_exception_justification:
            "Not implemented, considered not useful."})");

  FetchCallback done = base::BindOnce(&OriginPolicyFetcher::OnPolicyHasArrived,
                                      base::Unretained(this));
  SimpleURLLoader::OnResponseStartedCallback check_content_type =
      base::BindOnce(&OriginPolicyFetcher::OnResponseStarted,
                     base::Unretained(this));

  // Create and configure the SimpleURLLoader for the policy.
  std::unique_ptr<ResourceRequest> policy_request =
      std::make_unique<ResourceRequest>();
  policy_request->url = fetch_url_;
  policy_request->request_initiator = isolation_info_.frame_origin().value();
  policy_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  policy_request->redirect_mode = network::mojom::RedirectMode::kError;

  // Set the IsolationInfo for the load of the Origin Policy manifest.
  policy_request->trusted_params = network::ResourceRequest::TrustedParams();
  policy_request->trusted_params->isolation_info = isolation_info_;

  url_loader_ =
      SimpleURLLoader::Create(std::move(policy_request), traffic_annotation);

  url_loader_->SetOnResponseStartedCallback(std::move(check_content_type));
  // Start the download, and pass the callback for when we're finished.
  url_loader_->DownloadToString(factory, std::move(done),
                                kOriginPolicyMaxPolicySize);
}

void OriginPolicyFetcher::OnResponseStarted(
    const GURL& final_url,
    const mojom::URLResponseHead& response_head) {
  std::string mime_type;
  // If the manifest's return code isn't success (2xx) or if the manifest's
  // mimetype is incorrect, reject it and return with kNoPolicyApplies.
  if (!response_head.headers ||
      response_head.headers->response_code() / 100 != 2 ||
      (response_head.headers->GetMimeType(&mime_type) &&
       mime_type != "application/originpolicy+json")) {
    // The manifest file returned with incorrect content-type, or unsuccessful
    // return code, so bail out.
    OriginPolicy result;
    result.state = OriginPolicyState::kNoPolicyApplies;
    // Do not add code after this call as it will destroy this object.
    // When |this| is destroyed, |url_loader_| will be release, so when this
    // callback returns the SimpleURLLoader will bail out gracefully.
    // This also means that OnPolicyHasArrived() below will not be called.
    owner_policy_manager_->FetcherDone(this, result, std::move(callback_));
  }
}

void OriginPolicyFetcher::OnPolicyHasArrived(
    std::unique_ptr<std::string> policy_content) {
  OriginPolicy result;
  result.state = policy_content ? OriginPolicyState::kLoaded
                                : OriginPolicyState::kCannotLoadPolicy;

  if (policy_content)
    result.contents = OriginPolicyParser::Parse(*policy_content);

  result.policy_url = fetch_url_;

  // Do not add code after this call as it will destroy this object.
  owner_policy_manager_->FetcherDone(this, result, std::move(callback_));
}

}  // namespace network
