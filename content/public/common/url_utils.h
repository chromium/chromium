// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_UTILS_H_
#define CONTENT_PUBLIC_COMMON_URL_UTILS_H_

#include "content/common/content_export.h"
#include "url/origin.h"

class GURL;

namespace content {

// Returns true if the url has a scheme for WebUI. See also
// WebUIControllerFactory::UseWebUIForURL in the browser process.
CONTENT_EXPORT bool HasWebUIScheme(const GURL& url);

// Returns true if the origin is a WebUI.
CONTENT_EXPORT bool HasWebUIOrigin(const url::Origin& origin);

// Returns true if the origin is allowed to access the PDF internal plugin.
CONTENT_EXPORT bool IsPdfInternalPluginAllowedOrigin(const url::Origin& origin);

// Check whether we can do the saving page operation for the specified URL.
CONTENT_EXPORT bool IsSavableURL(const GURL& url);

// Helper function to determine if the navigation to |url| should make a request
// to the network stack. A request should not be sent for JavaScript URLs, or
// about:srcdoc, or chrome://crash. In these cases, no request needs to be sent.
//
// See also `network::IsURLHandledByNetworkService` which answers if a request
// is handled by a URLLoaderFactory returned by
// `network::mojom::NetworkContext::CreateURLLoaderFactory`.  For example, for
// "file:" URLs IsURLHandledByNetworkStack will return true, but
// IsURLHandledByNetworkService will return false.
CONTENT_EXPORT bool IsURLHandledByNetworkStack(const GURL& url);

// Determines whether it is safe to redirect from |from_url| to |to_url|.
CONTENT_EXPORT bool IsSafeRedirectTarget(const GURL& from_url,
                                         const GURL& to_url);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_UTILS_H_
