// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/webid/identity_url_loader_throttle.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/common/features.h"
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
static constexpr char kSetLoginHeader[] = "Set-Login";

static constexpr char kSetLoginHeaderValueLoggedIn[] = "logged-in";
static constexpr char kSetLoginHeaderValueLoggedOut[] = "logged-out";

}  // namespace

namespace content {

std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateIdentityUrlLoaderThrottle(
    SetIdpStatusCallback cb) {
  return std::make_unique<IdentityUrlLoaderThrottle>(std::move(cb));
}

IdentityUrlLoaderThrottle::IdentityUrlLoaderThrottle(SetIdpStatusCallback cb)
    : set_idp_status_cb_(std::move(cb)) {}

IdentityUrlLoaderThrottle::~IdentityUrlLoaderThrottle() = default;

void IdentityUrlLoaderThrottle::DetachFromCurrentSequence() {
  set_idp_status_cb_ = base::BindRepeating(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         SetIdpStatusCallback original_cb, const url::Origin& origin,
         blink::mojom::IdpSigninStatus status) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(original_cb), origin, status));
      },
      base::SequencedTaskRunner::GetCurrentDefault(),
      std::move(set_idp_status_cb_));
}

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

  // TODO(crbug.com/40236764):
  // - Limit to toplevel frames
  // - Decide whether to limit to same-origin
  // - Decide the right behavior with respect to user gestures.

  scoped_refptr<net::HttpResponseHeaders> headers = response_head.headers;
  if (!headers)
    return;

  std::string header;
  if (HeaderHasToken(*headers, kSetLoginHeader, kSetLoginHeaderValueLoggedIn)) {
    // Mark IDP as logged in
    VLOG(1) << "IDP signed in: " << response_url.spec();
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IdpSigninRequestInitiatedByUser",
                          has_user_gesture_);
    set_idp_status_cb_.Run(origin, IdpSigninStatus::kSignedIn);
  } else if (HeaderHasToken(*headers, kSetLoginHeader,
                            kSetLoginHeaderValueLoggedOut)) {
    // Mark IDP as logged out
    VLOG(1) << "IDP signed out: " << response_url.spec();
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.IdpSignoutRequestInitiatedByUser",
                          has_user_gesture_);
    set_idp_status_cb_.Run(origin, IdpSigninStatus::kSignedOut);
  }
}

// static
bool IdentityUrlLoaderThrottle::HeaderHasToken(
    const net::HttpResponseHeaders& headers,
    std::string_view header_name,
    std::string_view token) {
  if (!headers.HasHeader(header_name)) {
    return false;
  }

  std::string value;
  headers.GetNormalizedHeader(header_name, &value);

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      value, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return base::Contains(tokens, token);
}

}  // namespace content
