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
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/error_page/common/error.h"
#import "components/error_page/common/localized_error.h"
#import "components/grit/components_resources.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"
#import "ui/base/webui/web_ui_util.h"
#import "url/gurl.h"

bool IsDinoEasterEggEnabled(web::WebState* web_state) {
  return ProfileIOS::FromBrowserState(web_state->GetBrowserState())
      ->GetPrefs()
      ->GetBoolean(prefs::kAllowDinosaurEasterEgg);
}

NSString* GetErrorPage(const GURL& url,
                       NSError* error,
                       bool is_post,
                       bool is_off_the_record,
                       bool is_dino_easter_egg_enabled) {
  DCHECK_EQ(url,
            net::GURLWithNSURL(error.userInfo[NSURLErrorFailingURLErrorKey]));
  NSError* final_error = base::ios::GetFinalUnderlyingErrorFromError(error);
  if (!final_error) {
    final_error = error;
  }
  int net_error = net::ERR_FAILED;
  if ([final_error.domain isEqualToString:net::kNSErrorDomain]) {
    net_error = final_error.code;
    DCHECK_NE(0, net_error);
  } else {
    // This function may only be called with an NSError created with
    // web::NetErrorFromError.
    NOTREACHED();
  }

  // Secure DNS is not supported on iOS, so we can assume there is no secure
  // DNS network error when fetching the page state.
  error_page::LocalizedError::PageState page_state =
      error_page::LocalizedError::GetPageState(
          net_error, error_page::Error::kNetErrorDomain, url, is_post,
          /*is_secure_dns_network_error=*/false,
          /*stale_copy_in_cache=*/false,
          /*can_show_network_diagnostics_dialog=*/false, is_off_the_record,
          /*auto_fetch_feature_enabled=*/false,
          /*is_kiosk_mode=*/false,
          std::string(GetApplicationContext()
                          ->GetApplicationLocaleStorage()
                          ->GetTag()
                          .tag_string()),
          /*is_blocked_by_extension=*/false,
          /*error_page_params=*/nullptr);

  if (!is_dino_easter_egg_enabled) {
    page_state.strings.Set(
        "disabledEasterEgg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_FUN_DISABLED));
  }

  ui::ResourceScaleFactor scale_factor =
      ui::ResourceBundle::GetSharedInstance().GetMaxResourceScaleFactor();

  std::string extracted_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceStringForScale(
          IDR_NET_ERROR_HTML, scale_factor);
  std::string_view template_html(extracted_string.data(),
                                 extracted_string.size());

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << IDR_NET_ERROR_HTML;
  }

  return base::SysUTF8ToNSString(
      webui::GetLocalizedHtml(template_html, page_state.strings));
}
