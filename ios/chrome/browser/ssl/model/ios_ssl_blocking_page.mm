// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/model/ios_ssl_blocking_page.h"

#import <utility>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "components/security_interstitials/core/ssl_error_options_mask.h"
#import "components/security_interstitials/core/ssl_error_ui.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/web_state.h"
#import "net/base/net_errors.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using security_interstitials::SSLErrorOptionsMask;
using security_interstitials::SSLErrorUI;

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
IOSSSLBlockingPage::IOSSSLBlockingPage(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
        client)
    : IOSSecurityInterstitialPage(web_state, request_url, client.get()),
      web_state_(web_state),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(options_mask)),
      controller_(std::move(client)) {
  DCHECK(web_state_);
  // Override prefs for the SSLErrorUI.
  if (overridable_)
    options_mask |= SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  else
    options_mask &= ~SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;

  ssl_error_ui_.reset(new SSLErrorUI(request_url, cert_error, ssl_info,
                                     options_mask, time_triggered, GURL(),
                                     controller_.get()));

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  safe_browsing::SafeBrowsingMetricsCollector* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        safe_browsing::SafeBrowsingMetricsCollector::EventType::
            SECURITY_SENSITIVE_SSL_INTERSTITIAL);
  }

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool IOSSSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

IOSSSLBlockingPage::~IOSSSLBlockingPage() {
}

void IOSSSLBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
}

// static
bool IOSSSLBlockingPage::IsOverridable(int options_mask) {
  return (options_mask & SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
         !(options_mask & SSLErrorOptionsMask::STRICT_ENFORCEMENT);
}

void IOSSSLBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  // If a proceed command is received, allowlist the certificate and reload
  // the page to re-initiate the original navigation.
  if (command == security_interstitials::CMD_PROCEED) {
    web_state_->GetSessionCertificatePolicyCache()->RegisterAllowedCertificate(
        ssl_info_.cert, request_url().host(), ssl_info_.cert_status);
    web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                               /*check_for_repost=*/true);
    return;
  }

  ssl_error_ui_->HandleCommand(command);
}
