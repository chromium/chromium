// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
#define CONTENT_BROWSER_WEBID_WEBID_UTILS_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace blink::mojom {
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {
class BrowserContext;
enum class IdpSigninStatus;

namespace webid {

void SetIdpSigninStatus(content::BrowserContext* context,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status);

// Computes string to display in developer tools console for a FedCM endpoint
// request with the passed-in `endpoint_name` and which returns the passed-in
// `http_response_code`. Returns absl::nullopt if the `http_response_code` does
// not represent an error in the fetch.
absl::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code);

// Returns whether a FedCM endpoint URL is valid given the passed-in config
// endpoint URL.
bool IsEndpointUrlValid(const GURL& identity_provider_config_url,
                        const GURL& endpoint_url);

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
