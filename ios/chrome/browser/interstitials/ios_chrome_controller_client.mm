// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"

#include "base/logging.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/security/web_interstitial.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeControllerClient::IOSChromeControllerClient(
    web::WebState* web_state,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper)
    : security_interstitials::ControllerClient(std::move(metrics_helper)),
      web_state_(web_state),
      web_interstitial_(nullptr) {}

IOSChromeControllerClient::~IOSChromeControllerClient() {}

void IOSChromeControllerClient::SetWebInterstitial(
    web::WebInterstitial* web_interstitial) {
  web_interstitial_ = web_interstitial;
}

bool IOSChromeControllerClient::CanLaunchDateAndTimeSettings() {
  return false;
}

void IOSChromeControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED();
}

void IOSChromeControllerClient::GoBack() {
  web_state_->GetNavigationManager()->GoBack();
}

bool IOSChromeControllerClient::CanGoBack() {
  return web_state_->GetNavigationManager()->CanGoBack();
}

void IOSChromeControllerClient::GoBackAfterNavigationCommitted() {
  NOTREACHED();
}

void IOSChromeControllerClient::Proceed() {
  DCHECK(web_interstitial_);
  web_interstitial_->Proceed();
}

void IOSChromeControllerClient::Reload() {
  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             true /*check_for_repost*/);
}

void IOSChromeControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void IOSChromeControllerClient::OpenUrlInNewForegroundTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

const std::string& IOSChromeControllerClient::GetApplicationLocale() const {
  return GetApplicationContext()->GetApplicationLocale();
}

PrefService* IOSChromeControllerClient::GetPrefService() {
  return ios::ChromeBrowserState::FromBrowserState(
             web_state_->GetBrowserState())
      ->GetPrefs();
}

const std::string IOSChromeControllerClient::GetExtendedReportingPrefName()
    const {
  return std::string();
}
