// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_

#include <vector>

#include "components/signin/core/browser/signin_status_metrics_provider_delegate.h"
#include "ios/chrome/browser/signin/identity_manager_factory_observer.h"

class ChromeBrowserState;

class IOSChromeSigninStatusMetricsProviderDelegate
    : public SigninStatusMetricsProviderDelegate,
      public IdentityManagerFactoryObserver {
 public:
  IOSChromeSigninStatusMetricsProviderDelegate();

  IOSChromeSigninStatusMetricsProviderDelegate(
      const IOSChromeSigninStatusMetricsProviderDelegate&) = delete;
  IOSChromeSigninStatusMetricsProviderDelegate& operator=(
      const IOSChromeSigninStatusMetricsProviderDelegate&) = delete;

  ~IOSChromeSigninStatusMetricsProviderDelegate() override;

 private:
  // SigninStatusMetricsProviderDelegate implementation.
  void Initialize() override;
  AccountsStatus GetStatusOfAllAccounts() override;
  std::vector<signin::IdentityManager*> GetIdentityManagersForAllAccounts()
      override;

  // IdentityManagerFactoryObserver implementation.
  void IdentityManagerCreated(signin::IdentityManager* manager) override;

  // Returns the loaded ChromeBrowserState instances.
  std::vector<ChromeBrowserState*> GetLoadedChromeBrowserStates();
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_IOS_CHROME_SIGNIN_STATUS_METRICS_PROVIDER_DELEGATE_H_
