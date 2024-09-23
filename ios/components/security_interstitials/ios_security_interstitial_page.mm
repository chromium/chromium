// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_security_interstitial_page.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "components/grit/components_resources.h"
#import "components/security_interstitials/core/common_string_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/ui_util/dynamic_type_util.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/webui/web_ui_util.h"

namespace security_interstitials {

namespace {
// Adjusts the interstitial page's template parameter "fontsize" by system font
// size multiplier.
void AdjustFontSize(base::Value::Dict& load_time_data) {
  std::string* value = load_time_data.FindString("fontsize");
  DCHECK(value);
  std::string old_size = *value;
  // `old_size` should be in form of "75%".
  DCHECK(old_size.size() > 1 && old_size.back() == '%');
  double new_size = 75.0;
  bool converted =
      base::StringToDouble(old_size.substr(0, old_size.size() - 1), &new_size);
  DCHECK(converted);
  new_size *= ui_util::SystemSuggestedFontSizeMultiplier();
  load_time_data.Set("fontsize", base::StringPrintf("%.0lf%%", new_size));
}
}  // namespace

IOSSecurityInterstitialPage::IOSSecurityInterstitialPage(
    web::WebState* web_state,
    const GURL& request_url,
    IOSBlockingPageControllerClient* client)
    : web_state_(web_state),
      request_url_(request_url),
      client_(client) {
}

IOSSecurityInterstitialPage::~IOSSecurityInterstitialPage() {}

std::string IOSSecurityInterstitialPage::GetHtmlContents() const {
  base::Value::Dict load_time_data;
  // Interstitial pages on iOS get reloaded to prevent loading from cache, since
  // loading from cache breaks JavaScript commands. Set as `load_time_data`
  // for safety.
  load_time_data.Set("url_to_reload", request_url_.spec());
  PopulateInterstitialStrings(load_time_data);
  webui::SetLoadTimeDataDefaults(client_->GetApplicationLocale(),
                                 &load_time_data);
  AdjustFontSize(load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SECURITY_INTERSTITIAL_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetLocalizedHtml(html, load_time_data);
}

bool IOSSecurityInterstitialPage::ShouldDisplayURL() const {
  return true;
}

std::string_view IOSSecurityInterstitialPage::GetInterstitialType() const {
  return "";
}

void IOSSecurityInterstitialPage::ShowInfobar() {}

std::u16string IOSSecurityInterstitialPage::GetFormattedHostName() const {
  return security_interstitials::common_string_util::GetFormattedHostName(
      request_url_);
}

}  // namespace security_interstitials
