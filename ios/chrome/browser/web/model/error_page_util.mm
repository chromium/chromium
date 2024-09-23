// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/error_page_util.h"

#import <Foundation/Foundation.h>

#import <string_view>

#import "base/check_op.h"
#import "base/ios/ns_error_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/error_page/common/error.h"
#import "components/error_page/common/localized_error.h"
#import "components/grit/components_resources.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/net_errors.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"
#import "ui/base/webui/web_ui_util.h"
#import "url/gurl.h"

NSString* GetErrorPage(const GURL& url,
                       NSError* error,
                       bool is_post,
                       bool is_off_the_record) {
  DCHECK_EQ(url, GURL(base::SysNSStringToUTF8(
                     error.userInfo[NSURLErrorFailingURLStringErrorKey])));
  NSError* final_error = base::ios::GetFinalUnderlyingErrorFromError(error);
  if (!final_error)
    final_error = error;
  int net_error = net::ERR_FAILED;
  if ([final_error.domain isEqualToString:net::kNSErrorDomain]) {
    net_error = final_error.code;
    DCHECK_NE(0, net_error);
  } else {
    // This function may only be called with an NSError created with
    // web::NetErrorFromError.
    NOTREACHED_IN_MIGRATION();
  }

  // Secure DNS is not supported on iOS, so we can assume there is no secure
  // DNS network error when fetching the page state.
  error_page::LocalizedError::PageState page_state =
      error_page::LocalizedError::GetPageState(
          net_error, error_page::Error::kNetErrorDomain, url, is_post,
          /*is_secure_dns_network_error=*/false,
          /*stale_copy_in_cache=*/false,
          /*can_show_network_diagnostics_dialog=*/false, is_off_the_record,
          /*offline_content_feature_enabled=*/false,
          /*auto_fetch_feature_enabled=*/false,
          /*is_kiosk_mode=*/false,
          GetApplicationContext()->GetApplicationLocale(),
          /*is_blocked_by_extension=*/false,
          /*error_page_params=*/nullptr);

  ui::ResourceScaleFactor scale_factor =
      ui::ResourceBundle::GetSharedInstance().GetMaxResourceScaleFactor();

  std::string extracted_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceStringForScale(
          IDR_NET_ERROR_HTML, scale_factor);
  std::string_view template_html(extracted_string.data(),
                                 extracted_string.size());

  if (template_html.empty())
    NOTREACHED_IN_MIGRATION()
        << "unable to load template. ID: " << IDR_NET_ERROR_HTML;
  return base::SysUTF8ToNSString(
      webui::GetLocalizedHtml(template_html, page_state.strings));
}
