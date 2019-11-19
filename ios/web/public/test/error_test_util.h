// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_

#include <string>

class GURL;

@class NSError;

namespace web {

class WebState;

namespace testing {

// Creates Chrome specific error from a regular NSError. Returned error has the
// same format and structure as errors provided in ios/web callbacks.
NSError* CreateTestNetError(NSError* error);

// Builds the text for an error page in TestWebClient.
std::string GetErrorText(WebState* web_state,
                         const GURL& url,
                         const std::string& error_domain,
                         long error_code,
                         bool is_post,
                         bool is_off_the_record,
                         bool has_ssl_info);

}  // namespace testing
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_ERROR_TEST_UTIL_H_
