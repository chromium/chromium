// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/legacy_tls/legacy_tls_blocking_page.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "ios/components/security_interstitials/ios_blocking_page_metrics_helper.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

LegacyTLSBlockingPage::LegacyTLSBlockingPage(
    web::WebState* web_state,
    const GURL& request_url,
    std::unique_ptr<LegacyTLSControllerClient> client)
    : security_interstitials::IOSSecurityInterstitialPage(web_state,
                                                          request_url,
                                                          client.get()),
      web_state_(web_state),
      request_url_(request_url),
      controller_(std::move(client)) {
  DCHECK(web_state_);

  // Creating an interstitial without showing it (e.g. from
  // chrome://interstitials) leaks memory, so don't create it here.
}

LegacyTLSBlockingPage::~LegacyTLSBlockingPage() = default;

bool LegacyTLSBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

void LegacyTLSBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  CHECK(load_time_data);

  // Shared with SSL errors.
  security_interstitials::common_string_util::PopulateSSLLayoutStrings(
      net::ERR_SSL_OBSOLETE_VERSION, load_time_data);

  load_time_data->SetBoolean("overridable", true);
  load_time_data->SetBoolean("hide_primary_button", false);
  load_time_data->SetBoolean("bad_clock", false);
  load_time_data->SetString("type", "LEGACY_TLS");

  const std::u16string hostname(
      security_interstitials::common_string_util::GetFormattedHostName(
          request_url_));
  security_interstitials::common_string_util::PopulateLegacyTLSStrings(
      load_time_data, hostname);
}

void LegacyTLSBlockingPage::HandleScriptCommand(
    const base::DictionaryValue& message,
    const GURL& origin_url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  std::string command_string;
  if (!message.GetString("command", &command_string)) {
    LOG(ERROR) << "JS message parameter not found: command";
    return;
  }

  // Remove the command prefix so that the string value can be converted to a
  // SecurityInterstitialCommand enum value.
  std::size_t delimiter = command_string.find(".");
  if (delimiter == std::string::npos) {
    return;
  }

  // Parse the command int value from the text after the delimiter.
  int command = 0;
  if (!base::StringToInt(command_string.substr(delimiter + 1), &command)) {
    NOTREACHED() << "Command cannot be parsed to an int : " << command_string;
    return;
  }

  if (command == security_interstitials::CMD_DONT_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::DONT_PROCEED);
    controller_->GoBack();
  } else if (command == security_interstitials::CMD_PROCEED) {
    controller_->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEED);
    controller_->Proceed();
  }
}
