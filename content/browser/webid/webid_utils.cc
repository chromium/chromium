// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/web_identity.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

namespace content::webid {

void SetIdpSigninStatus(content::BrowserContext* context,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status) {
  auto* delegate = context->GetFederatedIdentityPermissionContext();
  if (!delegate) {
    // The embedder may not have a delegate (e.g. webview)
    return;
  }
  delegate->SetIdpSigninStatus(
      origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

absl::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code) {
  // Do not add error message for OK response status.
  if (http_response_code >= 200 && http_response_code <= 299)
    return absl::nullopt;

  if (http_response_code < 0) {
    // In this case, the |response_code| represents a NET_ERROR, so we should
    // use a helper function to ensure we use a meaningful message.
    return base::StringPrintf(
        "The fetch of the %s resulted in a network error: %s", endpoint_name,
        net::ErrorToShortString(http_response_code).c_str());
  }
  // In this case, the |response_code| represents an HTTP error code, which is
  // standard and hence the number by itself should be understood.
  return base::StringPrintf(
      "When fetching the %s, a %d HTTP response code was received.",
      endpoint_name, http_response_code);
}

bool IsEndpointUrlValid(const GURL& identity_provider_config_url,
                        const GURL& endpoint_url) {
  return url::Origin::Create(identity_provider_config_url)
      .IsSameOriginWith(endpoint_url);
}

}  // namespace content::webid
