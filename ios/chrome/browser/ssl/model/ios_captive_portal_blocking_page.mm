// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/model/ios_captive_portal_blocking_page.h"

#import "base/i18n/rtl.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/captive_portal/core/captive_portal_detector.h"
#import "components/captive_portal/core/captive_portal_metrics.h"
#import "components/security_interstitials/core/controller_client.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ui/base/l10n/l10n_util.h"

IOSCaptivePortalBlockingPage::IOSCaptivePortalBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    const GURL& landing_url,
    security_interstitials::IOSBlockingPageControllerClient* client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client),
      landing_url_(landing_url) {
  captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
      captive_portal::CaptivePortalMetrics::SHOW_ALL);
}

IOSCaptivePortalBlockingPage::~IOSCaptivePortalBlockingPage() {}

bool IOSCaptivePortalBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void IOSCaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  load_time_data.Set("iconClass", "icon-offline");
  load_time_data.Set("type", "CAPTIVE_PORTAL");
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);

  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));

  std::u16string tab_title =
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_HEADING_WIFI);
  load_time_data.Set("tabTitle", tab_title);
  load_time_data.Set("heading", tab_title);

  std::u16string paragraph;
  if (landing_url_.spec() ==
      captive_portal::CaptivePortalDetector::kDefaultURL) {
    // Captive portal may intercept requests without HTTP redirects, in which
    // case the login url would be the same as the captive portal detection url.
    // Don't show the login url in that case.
    paragraph = l10n_util::GetStringUTF16(
        IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_NO_LOGIN_URL_WIFI);
  } else {
    // Portal redirection was done with HTTP redirects, so show the login URL.
    // If `languages` is empty, punycode in `login_host` will always be decoded.
    std::u16string login_host =
        url_formatter::IDNToUnicode(landing_url_.host());
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    paragraph = l10n_util::GetStringFUTF16(
        IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI, login_host);
  }
  load_time_data.Set("primaryParagraph", std::move(paragraph));
  // Explicitly specify other expected fields to empty.
  load_time_data.Set("openDetails", "");
  load_time_data.Set("closeDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("optInLink", "");
  load_time_data.Set("enhancedProtectionMessage", "");
  load_time_data.Set("show_recurrent_error_paragraph", false);
}

void IOSCaptivePortalBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  // Any command other than "open the login page" is ignored.
  if (command == security_interstitials::CMD_OPEN_LOGIN) {
    captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
        captive_portal::CaptivePortalMetrics::OPEN_LOGIN_PAGE);

    CaptivePortalTabHelper::GetOrCreateForWebState(web_state())
        ->DisplayCaptivePortalLoginPage(landing_url_);
  }
}
