// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/internal_debug_pages_disabled/internal_debug_pages_disabled_ui.h"

#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "components/grit/internal_debug_pages_disabled_resources.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

web::WebUIIOSDataSource* CreateHTMLSource(const std::string& host_name) {
  web::WebUIIOSDataSource* source = web::WebUIIOSDataSource::Create(host_name);
  source->AddLocalizedString("pageHeading",
                             IDS_INTERNAL_DEBUG_PAGES_DISABLED_HEADING);
  std::u16string body = l10n_util::GetStringFUTF16(
      IDS_INTERNAL_DEBUG_PAGES_DISABLED_BODY,
      base::StrCat({base::ASCIIToUTF16(kChromeUIChromeURLsURL),
                    u"#internal-debug-pages"}));
  source->AddString("pageBody", body);
  source->AddResourcePath("", IDR_INTERNAL_DEBUG_PAGES_DISABLED_APP_HTML);
  return source;
}

}  // namespace

InternalDebugPagesDisabledUI::InternalDebugPagesDisabledUI(
    web::WebUIIOS* web_ui,
    const std::string& host_name)
    : web::WebUIIOSController(web_ui, host_name) {
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateHTMLSource(host_name));
}
