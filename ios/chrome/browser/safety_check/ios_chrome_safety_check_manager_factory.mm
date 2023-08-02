// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

// static
IOSChromeSafetyCheckManager*
IOSChromeSafetyCheckManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<IOSChromeSafetyCheckManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromeSafetyCheckManagerFactory*
IOSChromeSafetyCheckManagerFactory::GetInstance() {
  static base::NoDestructor<IOSChromeSafetyCheckManagerFactory> instance;
  return instance.get();
}

IOSChromeSafetyCheckManagerFactory::IOSChromeSafetyCheckManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "SafetyCheckManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromePasswordCheckManagerFactory::GetInstance());
}

IOSChromeSafetyCheckManagerFactory::~IOSChromeSafetyCheckManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSChromeSafetyCheckManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  CHECK(IsSafetyCheckMagicStackEnabled());

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  const scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  return std::make_unique<IOSChromeSafetyCheckManager>(
      browser_state->GetPrefs(), GetApplicationContext()->GetLocalState(),
      IOSChromePasswordCheckManagerFactory::GetForBrowserState(browser_state),
      task_runner);
}

web::BrowserState* IOSChromeSafetyCheckManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
