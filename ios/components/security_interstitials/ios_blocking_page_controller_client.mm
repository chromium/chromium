// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/notreached.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

namespace security_interstitials {

IOSBlockingPageControllerClient::IOSBlockingPageControllerClient(
    web::WebState* web_state,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    const std::string& app_locale)
    : security_interstitials::ControllerClient(std::move(metrics_helper)),
      web_state_(web_state),
      app_locale_(app_locale),
      weak_factory_(this) {
  web_state_->AddObserver(this);
}

IOSBlockingPageControllerClient::~IOSBlockingPageControllerClient() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
}

void IOSBlockingPageControllerClient::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

bool IOSBlockingPageControllerClient::CanLaunchDateAndTimeSettings() {
  return false;
}

void IOSBlockingPageControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED_IN_MIGRATION();
}

void IOSBlockingPageControllerClient::GoBack() {
  if (CanGoBack()) {
    web_state_->GetNavigationManager()->GoBack();
  } else {
    // Closing the tab synchronously is problematic since web state is heavily
    // involved in the operation and CloseWebState interrupts it, so call
    // CloseWebState asynchronously.
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOSBlockingPageControllerClient::Close,
                                  weak_factory_.GetWeakPtr()));
  }
}

bool IOSBlockingPageControllerClient::CanGoBack() {
  return web_state_->GetNavigationManager()->CanGoBack();
}

bool IOSBlockingPageControllerClient::CanGoBackBeforeNavigation() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void IOSBlockingPageControllerClient::GoBackAfterNavigationCommitted() {
  NOTREACHED_IN_MIGRATION();
}

void IOSBlockingPageControllerClient::Proceed() {
  NOTREACHED_IN_MIGRATION();
}

void IOSBlockingPageControllerClient::Reload() {
  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             true /*check_for_repost*/);
}

void IOSBlockingPageControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void IOSBlockingPageControllerClient::OpenUrlInNewForegroundTab(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void IOSBlockingPageControllerClient::OpenEnhancedProtectionSettings() {
  NOTREACHED_IN_MIGRATION() << "Enhanced protection is not supported on iOS.";
}

const std::string& IOSBlockingPageControllerClient::GetApplicationLocale()
    const {
  return app_locale_;
}

PrefService* IOSBlockingPageControllerClient::GetPrefService() {
  return nullptr;
}

const std::string
IOSBlockingPageControllerClient::GetExtendedReportingPrefName() const {
  return std::string();
}

void IOSBlockingPageControllerClient::Close() {
  if (web_state_) {
    web_state_->CloseWebState();
  }
}

}  // namespace security_interstitials
