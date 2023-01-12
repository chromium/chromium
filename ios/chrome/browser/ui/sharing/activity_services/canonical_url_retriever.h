// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"

namespace web {
class WebState;
}

class GURL;

namespace activity_services {

// Script to access the canonical URL from a web page. This script shouldn't be
// used directly, but is exposed for testing purposes.
extern const char16_t kCanonicalURLScript[];

// Callback invoked when the canonical URL has been fetched.
using CanonicalUrlRetrievedCallback = base::OnceCallback<void(const GURL& url)>;

// Retrieves the canonical URL in the web page represented by `web_state`.
// This method is asynchronous and the URL is returned by calling the
// `completion` block.
// There are a few special cases:
// 1. If there is more than one canonical URL defined, the first one
// (found through a depth-first search of the DOM) is given to `completion`.
// 2. If either no canonical URL is found, or the canonical URL is invalid, an
// empty GURL is given to `completion`.
// 3. If the canonical URL is a relative path, it is ignored and an empty GURL
// is given to `completion`.
// 4. If the the visible URL is not HTTPS, then an empty
// GURL is given to `completion`. This prevents untrusted sites from specifying
// the canonical URL.
// 5. If the canonical URL is not HTTPS, then an empty GURL is given to
// `completion`. This prevents the canonical URL from being downgraded to HTTP
// from the HTTPS visible URL.
void RetrieveCanonicalUrl(web::WebState* web_state,
                          CanonicalUrlRetrievedCallback completion);

}  // namespace activity_services

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_CANONICAL_URL_RETRIEVER_H_
