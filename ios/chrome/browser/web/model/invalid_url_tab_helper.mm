// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/invalid_url_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/data_url.h"
#import "net/base/net_errors.h"
#import "net/http/http_response_headers.h"
#import "ui/base/page_transition_types.h"
#import "url/url_constants.h"

namespace {

// Returns true if URL request is valid and navigation should be allowed.
bool IsUrlRequestValid(NSURLRequest* request) {
  if (request.URL.absoluteString.length > url::kMaxURLChars) {
    return false;
  }

  if ([request.URL.scheme isEqualToString:@"data"]) {
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

void InvalidUrlTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  if (IsUrlRequestValid(request)) {
    return std::move(callback).Run(PolicyDecision::Allow());
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
    return std::move(callback).Run(PolicyDecision::CancelAndDisplayError(
        [NSError errorWithDomain:net::kNSErrorDomain
                            code:net::ERR_INVALID_URL
                        userInfo:nil]));
  }
  std::move(callback).Run(PolicyDecision::Cancel());
}

WEB_STATE_USER_DATA_KEY_IMPL(InvalidUrlTabHelper)
