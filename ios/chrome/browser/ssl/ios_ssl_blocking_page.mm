// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"
#include "ios/chrome/browser/interstitials/ios_chrome_metrics_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/security/ssl_status.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::SSLErrorOptionsMask;
using security_interstitials::SSLErrorUI;

namespace {
IOSChromeMetricsHelper* CreateMetricsHelper(web::WebState* web_state,
                                            const GURL& request_url,
                                            bool overridable) {
  // Set up the metrics helper for the SSLErrorUI.
  security_interstitials::MetricsHelper::ReportDetails reporting_info;
  reporting_info.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";
  return new IOSChromeMetricsHelper(web_state, request_url, reporting_info);
}

}  // namespace

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
IOSSSLBlockingPage::IOSSSLBlockingPage(web::WebState* web_state,
                                       int cert_error,
                                       const net::SSLInfo& ssl_info,
                                       const GURL& request_url,
                                       int options_mask,
                                       const base::Time& time_triggered,
                                       base::OnceCallback<void(bool)> callback)
    : IOSSecurityInterstitialPage(web_state, request_url),
      callback_(std::move(callback)),
      ssl_info_(ssl_info),
      overridable_(IsOverridable(options_mask)),
      controller_(new IOSChromeControllerClient(
          web_state,
          base::WrapUnique(CreateMetricsHelper(web_state,
                                               request_url,
                                               IsOverridable(options_mask))))) {
  // Override prefs for the SSLErrorUI.
  if (overridable_)
    options_mask |= SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;
  else
    options_mask &= ~SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED;

  ssl_error_ui_.reset(new SSLErrorUI(request_url, cert_error, ssl_info,
                                     options_mask, time_triggered, GURL(),
                                     controller_.get()));

  // Creating an interstitial without showing (e.g. from chrome://interstitials)
  // it leaks memory, so don't create it here.
}

bool IOSSSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

IOSSSLBlockingPage::~IOSSSLBlockingPage() {
  if (!callback_.is_null()) {
    // The page is closed without the user having chosen what to do, default to
    // deny.
    NotifyDenyCertificate();
  }
}

void IOSSSLBlockingPage::AfterShow() {
  controller_->SetWebInterstitial(web_interstitial());
}

void IOSSSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) const {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
}

// This handles the commands sent from the interstitial JavaScript.
void IOSSSLBlockingPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);
  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void IOSSSLBlockingPage::OnProceed() {
  // Accepting the certificate resumes the loading of the page.
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(true);
}

void IOSSSLBlockingPage::OnDontProceed() {
  NotifyDenyCertificate();
}

void IOSSSLBlockingPage::OverrideItem(web::NavigationItem* item) {
  item->SetTitle(l10n_util::GetStringUTF16(IDS_SSL_V2_TITLE));

  item->GetSSL().security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  item->GetSSL().cert_status = ssl_info_.cert_status;
  // On iOS cert may be null when it is not provided by API callback or can not
  // be parsed.
  if (ssl_info_.cert) {
    item->GetSSL().certificate = ssl_info_.cert;
  }
}

void IOSSSLBlockingPage::NotifyDenyCertificate() {
  // It's possible that callback_ may not exist if the user clicks "Proceed"
  // followed by pressing the back button before the interstitial is hidden.
  // In that case the certificate will still be treated as allowed.
  if (callback_.is_null())
    return;

  std::move(callback_).Run(false);
}

// static
bool IOSSSLBlockingPage::IsOverridable(int options_mask) {
  return (options_mask & SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
         !(options_mask & SSLErrorOptionsMask::STRICT_ENFORCEMENT);
}
