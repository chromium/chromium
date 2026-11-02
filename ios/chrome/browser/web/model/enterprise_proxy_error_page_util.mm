// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/enterprise_proxy_error_page_util.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>

#import "base/check.h"
#import "base/ios/ns_error_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_blocking_page.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/web/model/error_page_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/net_errors.h"
#import "url/gurl.h"

namespace {

// Returns the Secure Gateway interstitial HTML for the disguised error recorded
// for `navigation_id`, or nil if there is no interstitial to show.
NSString* GetSecureGatewayErrorPageHtml(web::WebState* web_state,
                                        int64_t navigation_id) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  enterprise_net::EnterpriseProxyErrorService* error_service =
      EnterpriseProxyErrorServiceFactoryIOS::GetForProfile(profile);
  // Navigations only fail with the enterprise proxy error through
  // `ProxyServiceController`, which doesn't exist without this service.
  CHECK(error_service);
  std::optional<enterprise_net::EnterpriseProxyErrorData> error_data =
      error_service->TakeDisguisedError(navigation_id);
  if (!error_data) {
    return nil;
  }
  // Empty when enterprise proxy error handling is disabled.
  base::DictValue error_page_params =
      error_service->GetErrorPageParams(*error_data);
  if (error_page_params.empty()) {
    return nil;
  }

  std::unique_ptr<EnterpriseProxyBlockingPage> page =
      EnterpriseProxyBlockingPage::Create(web_state, *error_data,
                                          std::move(error_page_params));
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));
  return base::SysUTF8ToNSString(error_page_content);
}

// Returns the generic error page HTML for the navigation error `error`. The
// managed proxy didn't let the request through, so the page reports a failed
// tunnel connection. That net error is appended as the final underlying error
// because `GetErrorPage()` only renders net errors.
NSString* GetGenericErrorPageHtml(const GURL& url,
                                  NSError* error,
                                  bool is_post,
                                  bool is_off_the_record) {
  NSError* tunnel_error =
      [NSError errorWithDomain:net::kNSErrorDomain
                          code:net::ERR_TUNNEL_CONNECTION_FAILED
                      userInfo:nil];
  return GetErrorPage(
      url, base::ios::ErrorWithAppendedUnderlyingError(error, tunnel_error),
      is_post, is_off_the_record);
}

}  // namespace

NSString* GetEnterpriseProxyErrorPageHtml(web::WebState* web_state,
                                          const GURL& url,
                                          NSError* error,
                                          bool is_post,
                                          bool is_off_the_record,
                                          int64_t navigation_id) {
  NSString* secure_gateway_html =
      GetSecureGatewayErrorPageHtml(web_state, navigation_id);
  if (secure_gateway_html) {
    return secure_gateway_html;
  }
  // Falls back to a generic error page, e.g. when no disguised error was
  // recorded for the navigation.
  return GetGenericErrorPageHtml(url, error, is_post, is_off_the_record);
}
