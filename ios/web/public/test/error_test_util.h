// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#import <Foundation/Foundation.h>

#include "net/cert/cert_status_flags.h"

class GURL;

namespace web {

class WebState;

namespace testing {

// Creates an NSURLErrorDomain error for NSURLErrorNetworkConnectionLost with
// its expected underlying error chain.
NSError* CreateConnectionLostError();

// Creates Chrome specific error from a regular NSError. Returned error has the
// same format and structure as errors provided in ios/web callbacks.
NSError* CreateTestNetError(NSError* error);

// Creates an NSError using the domains and codes in `domain_code_pairs`.  The
// returned NSError will use the domain and code from the first pair in the
// list.  Each subsequent pair in the list will be used to create the underlying
// error for the previous pair in the list.  Returns nil if `domain_code_pairs`
// is empty.
NSError* CreateErrorWithUnderlyingErrorChain(
    const std::vector<std::pair<NSErrorDomain, NSInteger>>& domain_code_pairs);

// Builds the text for an error page in TestWebClient.  `error` must be
// non-null.
std::string GetErrorText(WebState* web_state,
                         const GURL& url,
                         NSError* error,
                         bool is_post,
                         bool is_off_the_record,
                         net::CertStatus cert_status);

}  // namespace testing
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_
