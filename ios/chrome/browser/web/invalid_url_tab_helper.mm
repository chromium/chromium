// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/invalid_url_tab_helper.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/net/protocol_handler_util.h"
#include "net/base/data_url.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns true if URL request is valid and navigation should be allowed.
bool IsUrlRequestValid(NSURLRequest* request) {
  if (request.URL.absoluteString.length > url::kMaxURLChars) {
    return false;
  }

  if ([request.URL.scheme isEqual:@"data"]) {
    std::string mime_type;
    std::string charset;
    std::string data;
    scoped_refptr<net::HttpResponseHeaders> headers;
    if (net::DataURL::BuildResponse(net::GURLWithNSURL(request.URL),
                                    base::SysNSStringToUTF8(request.HTTPMethod),
                                    &mime_type, &charset, &data,
                                    &headers) == net::ERR_INVALID_URL) {
      return false;
    }
  }

  return true;
}

}  // namespace

InvalidUrlTabHelper::InvalidUrlTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}
InvalidUrlTabHelper::~InvalidUrlTabHelper() = default;

web::WebStatePolicyDecider::PolicyDecision
InvalidUrlTabHelper::ShouldAllowRequest(NSURLRequest* request,
                                        const RequestInfo& request_info) {
  if (IsUrlRequestValid(request)) {
    return PolicyDecision::Allow();
  }

  // URL is invalid. Show error for certain browser-initiated navigations (f.e.
  // the user typed the URL, tapped the bookmark or onmibox suggestion) and
  // silently cancel the navigation for other navigations (f.e. the user clicked
  // the link or page made a client side redirect).

  using ui::PageTransitionCoreTypeIs;
  ui::PageTransition transition = request_info.transition_type;
  if (PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_GENERATED) ||
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    return PolicyDecision::CancelAndDisplayError([NSError
        errorWithDomain:net::kNSErrorDomain
                   code:net::ERR_INVALID_URL
               userInfo:nil]);
  }
  return PolicyDecision::Cancel();
}

WEB_STATE_USER_DATA_KEY_IMPL(InvalidUrlTabHelper)
