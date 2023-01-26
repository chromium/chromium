// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::mojom::IdpSigninStatus;

namespace {
// See the comment in HandleResponseOrRedirect for why we are checking for
// Google-specific headers.
static constexpr char kGoogleSigninHeader[] = "Google-Accounts-SignIn";
static constexpr char kGoogleSignoutHeader[] = "Google-Accounts-SignOut";
static constexpr char kIdpSigninStatusHeader[] = "IdP-SignIn-Status";

static constexpr char kIdpHeaderValueSignin[] = "action=signin";
static constexpr char kIdpHeaderValueSignout[] = "action=signout-all";

bool IsFedCmIdpSigninStatusThrottleEnabled() {
  return GetFieldTrialParamByFeatureAsBool(
             features::kFedCm,
             features::kFedCmIdpSigninStatusFieldTrialParamName, false) ||
         GetFieldTrialParamByFeatureAsBool(
             features::kFedCm,
             features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
             true);
}

}  // namespace

namespace content {

std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateIdentityUrlLoaderThrottle(
    SetIdpStatusCallback cb) {
  if (!IsFedCmIdpSigninStatusThrottleEnabled())
    return nullptr;
  return std::make_unique<IdentityUrlLoaderThrottle>(std::move(cb));
}

IdentityUrlLoaderThrottle::IdentityUrlLoaderThrottle(SetIdpStatusCallback cb)
    : set_idp_status_cb_(std::move(cb)) {}

IdentityUrlLoaderThrottle::~IdentityUrlLoaderThrottle() = default;

void IdentityUrlLoaderThrottle::DetachFromCurrentSequence() {}

void IdentityUrlLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request_url_ = request->url;
  has_user_gesture_ = request->has_user_gesture;
}

void IdentityUrlLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK(response_head);
  return HandleResponseOrRedirect(response_url, *response_head);
}

void IdentityUrlLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  // We want to check headers for each redirect. It is common that the header
  // is on the initial load which then redirects back to a homepage.
  HandleResponseOrRedirect(request_url_, response_head);
  request_url_ = redirect_info->new_url;
}

void IdentityUrlLoaderThrottle::HandleResponseOrRedirect(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head) {
  url::Origin origin = url::Origin::Create(response_url);
  if (!network::IsOriginPotentiallyTrustworthy(origin))
    return;

  // TODO(crbug.com/1357790):
  // - Limit to toplevel frames
  // - Decide whether to limit to same-origin
  // - Decide the right behavior with respect to user gestures.

  scoped_refptr<net::HttpResponseHeaders> headers = response_head.headers;
  if (!headers)
    return;

  // We are checking two versions of the header -- a standardized one and a
  // legacy one. The legacy one is primarily used so we can gather metrics
  // from existing deployments.
  // TODO(https://crbug.com/1381501): Remove the Google headers once we can.
  std::string header;
  if (headers->GetNormalizedHeader(kGoogleSigninHeader, &header) ||
      headers->HasHeaderValue(kIdpSigninStatusHeader, kIdpHeaderValueSignin)) {
    // Mark IDP as logged in
    VLOG(1) << "IDP signed in: " << response_url.spec();
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IdpSigninRequestInitiatedByUser",
                          has_user_gesture_);
    set_idp_status_cb_.Run(origin, IdpSigninStatus::kSignedIn);
  } else if (headers->GetNormalizedHeader(kGoogleSignoutHeader, &header) ||
             headers->HasHeaderValue(kIdpSigninStatusHeader,
                                     kIdpHeaderValueSignout)) {
    // Mark IDP as logged out
    VLOG(1) << "IDP signed out: " << response_url.spec();
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IdpSignoutRequestInitiatedByUser",
                          has_user_gesture_);
    set_idp_status_cb_.Run(origin, IdpSigninStatus::kSignedOut);
  }
}

}  // namespace content
