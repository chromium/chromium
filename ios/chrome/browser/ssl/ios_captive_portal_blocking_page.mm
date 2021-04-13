// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_captive_portal_blocking_page.h"

#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_metrics.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSCaptivePortalBlockingPage::IOSCaptivePortalBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    const GURL& landing_url,
    base::OnceCallback<void(bool)> callback,
    security_interstitials::IOSBlockingPageControllerClient* client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client),
      landing_url_(landing_url),
      callback_(std::move(callback)) {
  captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
      captive_portal::CaptivePortalMetrics::SHOW_ALL);
}

IOSCaptivePortalBlockingPage::~IOSCaptivePortalBlockingPage() {}

bool IOSCaptivePortalBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void IOSCaptivePortalBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  load_time_data->SetString("iconClass", "icon-offline");
  load_time_data->SetString("type", "CAPTIVE_PORTAL");
  load_time_data->SetBoolean("overridable", false);
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_BUTTON_OPEN_LOGIN_PAGE));

  std::u16string tab_title =
      l10n_util::GetStringUTF16(IDS_CAPTIVE_PORTAL_HEADING_WIFI);
  load_time_data->SetString("tabTitle", tab_title);
  load_time_data->SetString("heading", tab_title);

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
    // If |languages| is empty, punycode in |login_host| will always be decoded.
    std::u16string login_host =
        url_formatter::IDNToUnicode(landing_url_.host());
    if (base::i18n::IsRTL())
      base::i18n::WrapStringWithLTRFormatting(&login_host);

    paragraph = l10n_util::GetStringFUTF16(
        IDS_CAPTIVE_PORTAL_PRIMARY_PARAGRAPH_WIFI, login_host);
  }
  load_time_data->SetString("primaryParagraph", paragraph);
  // Explicitly specify other expected fields to empty.
  load_time_data->SetString("openDetails", std::u16string());
  load_time_data->SetString("closeDetails", std::u16string());
  load_time_data->SetString("explanationParagraph", std::u16string());
  load_time_data->SetString("finalParagraph", std::u16string());
  load_time_data->SetString("recurrentErrorParagraph", std::u16string());
  load_time_data->SetString("optInLink", std::u16string());
  load_time_data->SetString("enhancedProtectionMessage", std::u16string());
  load_time_data->SetBoolean("show_recurrent_error_paragraph", false);
}

void IOSCaptivePortalBlockingPage::AfterShow() {}

void IOSCaptivePortalBlockingPage::OnDontProceed() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  std::move(callback_).Run(false);
}

void IOSCaptivePortalBlockingPage::CommandReceived(const std::string& command) {
  int command_num = 0;
  bool command_is_num = base::StringToInt(command, &command_num);
  DCHECK(command_is_num) << command;
  // Any command other than "open the login page" is ignored.
  if (command_num == security_interstitials::CMD_OPEN_LOGIN) {
    captive_portal::CaptivePortalMetrics::LogCaptivePortalBlockingPageEvent(
        captive_portal::CaptivePortalMetrics::OPEN_LOGIN_PAGE);

    CaptivePortalDetectorTabHelper::FromWebState(web_state())
        ->DisplayCaptivePortalLoginPage(landing_url_);
  }
}

void IOSCaptivePortalBlockingPage::HandleScriptCommand(
    const base::DictionaryValue& message,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  std::string command;
  if (!message.GetString("command", &command)) {
    LOG(ERROR) << "JS message parameter not found: command";
    return;
  }
  // Non-proceed commands are handled the same between committed and
  // non-committed interstitials, so the CommandReceived method can be used.
  // Remove the command prefix since it is ignored when converting the value
  // to a SecurityInterstitialCommand.
  std::size_t delimiter = command.find(".");
  if (delimiter != std::string::npos) {
    IOSCaptivePortalBlockingPage::CommandReceived(
        command.substr(delimiter + 1));
  }
}
