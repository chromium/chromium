// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/error_test_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/error_translation_util.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace testing {

NSError* CreateConnectionLostError() {
  return CreateErrorWithUnderlyingErrorChain(
      {{@"NSURLErrorDomain", NSURLErrorNetworkConnectionLost},
       {@"kCFErrorDomainCFNetwork", kCFURLErrorNetworkConnectionLost},
       {net::kNSErrorDomain, net::ERR_CONNECTION_CLOSED}});
}

NSError* CreateTestNetError(NSError* error) {
  return NetErrorFromError(error);
}

NSError* CreateErrorWithUnderlyingErrorChain(
    const std::vector<std::pair<NSErrorDomain, NSInteger>>& domain_code_pairs) {
  if (domain_code_pairs.empty())
    return nil;

  NSError* error = nil;
  for (int i = domain_code_pairs.size() - 1; i >= 0; --i) {
    NSDictionary* user_info = error ? @{NSUnderlyingErrorKey : error} : nil;
    error = [NSError errorWithDomain:domain_code_pairs[i].first
                                code:domain_code_pairs[i].second
                            userInfo:user_info];
  }
  return error;
}

std::string GetErrorText(WebState* web_state,
                         const GURL& url,
                         NSError* error,
                         bool is_post,
                         bool is_off_the_record,
                         net::CertStatus cert_status) {
  // Construct the error text representation.
  std::string error_text = "{";
  while (error) {
    error_text += base::StringPrintf(
        "{%s, %ld}", base::SysNSStringToUTF8(error.domain).c_str(), error.code);
    error = error.userInfo[NSUnderlyingErrorKey];
    if (error)
      error_text += " => ";
  }
  error_text += "}";

  return base::StringPrintf("web_state: %p url: %s error_chain: %s post: "
                            "%d otr: %d cert_status: %d",
                            web_state, url.spec().c_str(), error_text.c_str(),
                            is_post, is_off_the_record, cert_status);
}

}  // namespace testing
}  // namespace web
