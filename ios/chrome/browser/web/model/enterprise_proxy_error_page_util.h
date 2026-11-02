// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_ENTERPRISE_PROXY_ERROR_PAGE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_ENTERPRISE_PROXY_ERROR_PAGE_UTIL_H_

#import <stdint.h>

class GURL;
@class NSError;
@class NSString;

namespace web {
class WebState;
}  // namespace web

// Returns the error page HTML to display in `web_state` for the navigation
// `navigation_id` to `url`, which failed with `error` because its managed proxy
// auth challenge could not be answered. The final underlying error of `error`
// is in `kEnterpriseProxyErrorDomain`.
//
// Takes the disguised error recorded for the navigation from the
// `EnterpriseProxyErrorService` of the profile, so a later call for the same
// navigation doesn't find it. The page is then the Secure Gateway interstitial
// for that error, and the interstitial is associated with the navigation in the
// `IOSBlockingPageTabHelper` of `web_state`. Without a recorded disguised
// error, or when enterprise proxy error handling is disabled, the page is the
// generic error page for a failed tunnel connection. The profile must have an
// `EnterpriseProxyErrorService`, and `web_state` an `IOSBlockingPageTabHelper`.
NSString* GetEnterpriseProxyErrorPageHtml(web::WebState* web_state,
                                          const GURL& url,
                                          NSError* error,
                                          bool is_post,
                                          bool is_off_the_record,
                                          int64_t navigation_id);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_ENTERPRISE_PROXY_ERROR_PAGE_UTIL_H_
