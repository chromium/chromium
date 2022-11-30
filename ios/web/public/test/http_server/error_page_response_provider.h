// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_ERROR_PAGE_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_ERROR_PAGE_RESPONSE_PROVIDER_H_

#include <map>
#include <string>

#import "ios/web/public/test/http_server/html_response_provider.h"
#include "url/gurl.h"

// A HtmlResponseProvider that supports the following additional URLs:
// - GetDnsFailureUrl - triggers a DNS error.
class ErrorPageResponseProvider : public HtmlResponseProvider {
 public:
  ErrorPageResponseProvider() : HtmlResponseProvider() {}
  explicit ErrorPageResponseProvider(
      const std::map<GURL, std::string>& responses)
      : HtmlResponseProvider(responses) {}
  // Returns a URL that causes a DNS failure.
  static GURL GetDnsFailureUrl();
};

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_ERROR_PAGE_RESPONSE_PROVIDER_H_
