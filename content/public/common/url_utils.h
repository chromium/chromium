// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_UTILS_H_
#define CONTENT_PUBLIC_COMMON_URL_UTILS_H_

#include "content/common/content_export.h"

class GURL;

namespace content {

// Returns true if the url has a scheme for WebUI.  See also
// WebUIControllerFactory::UseWebUIForURL in the browser process.
CONTENT_EXPORT bool HasWebUIScheme(const GURL& url);

// Check whether we can do the saving page operation for the specified URL.
CONTENT_EXPORT bool IsSavableURL(const GURL& url);

// Helper function to determine if the navigation to |url| should make a request
// to the network stack. A request should not be sent for JavaScript URLs or
// about:blank. In these cases, no request needs to be sent.
CONTENT_EXPORT bool IsURLHandledByNetworkStack(const GURL& url);

// Returns whether the given url is either a debugging url handled in the
// renderer process, such as one that crashes or hangs the renderer, or a
// javascript: URL that operates on the current page in the renderer.  Such URLs
// do not represent actual navigations and can be loaded in any SiteInstance.
CONTENT_EXPORT bool IsRendererDebugURL(const GURL& url);

// Helper function to determine if a request for |url| refers to a network
// resource (as opposed to a local browser resource like files or blobs). Used
// when the Network Service is enabled.
//
// Note that this is not equivalent to the above function, as several
// non-network schemes are handled by the network stack when the Network Service
// is disabled.
bool CONTENT_EXPORT IsURLHandledByNetworkService(const GURL& url);

// Determines whether it is safe to redirect from |from_url| to |to_url|.
CONTENT_EXPORT bool IsSafeRedirectTarget(const GURL& from_url,
                                         const GURL& to_url);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_UTILS_H_
