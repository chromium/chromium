// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_CONTROLLER_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_CONTROLLER_CLIENT_H_

#include <string>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/security_interstitials/core/controller_client.h"
#include "ios/web/public/web_state_observer.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace security_interstitials {
class MetricsHelper;

// Provides embedder-specific logic for the security error page controller.
class IOSBlockingPageControllerClient
    : public security_interstitials::ControllerClient,
      public web::WebStateObserver {
 public:
  IOSBlockingPageControllerClient(
      web::WebState* web_state,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      const std::string& app_locale);

  IOSBlockingPageControllerClient(const IOSBlockingPageControllerClient&) =
      delete;
  IOSBlockingPageControllerClient& operator=(
      const IOSBlockingPageControllerClient&) = delete;

  ~IOSBlockingPageControllerClient() override;

  // security_interstitials::ControllerClient implementation.
  void Proceed() override;
  void GoBack() override;
  bool CanGoBack() override;
  void OpenEnhancedProtectionSettings() override;

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  const std::string& GetApplicationLocale() const override;

  // security_interstitials::ControllerClient implementation.
  void OpenUrlInNewForegroundTab(const GURL& url) override;

 protected:
  // The WebState passed on initialization.
  web::WebState* web_state() const { return web_state_; }

  // security_interstitials::ControllerClient implementation.
  bool CanLaunchDateAndTimeSettings() override;
  void LaunchDateAndTimeSettings() override;
  bool CanGoBackBeforeNavigation() override;
  void GoBackAfterNavigationCommitted() override;
  void Reload() override;
  void OpenUrlInCurrentTab(const GURL& url) override;
  PrefService* GetPrefService() override;
  const std::string GetExtendedReportingPrefName() const override;

 private:
  // Closes the tab. Called in cases where a user clicks "Back to safety" and
  // it's not possible to go back.
  void Close();

  raw_ptr<web::WebState> web_state_;
  const std::string app_locale_;

  base::WeakPtrFactory<IOSBlockingPageControllerClient> weak_factory_;
};

}  // namespace security_interstitials

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_IOS_BLOCKING_PAGE_CONTROLLER_CLIENT_H_
