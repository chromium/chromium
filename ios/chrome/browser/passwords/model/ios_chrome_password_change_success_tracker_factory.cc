// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_chrome_password_change_success_tracker_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
IOSChromePasswordChangeSuccessTrackerFactory*
IOSChromePasswordChangeSuccessTrackerFactory::GetInstance() {
  static base::NoDestructor<IOSChromePasswordChangeSuccessTrackerFactory>
      instance;
  return instance.get();
}

// static
password_manager::PasswordChangeSuccessTracker*
IOSChromePasswordChangeSuccessTrackerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<password_manager::PasswordChangeSuccessTracker*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromePasswordChangeSuccessTrackerFactory::
    IOSChromePasswordChangeSuccessTrackerFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordChangeSuccessTracker",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromePasswordChangeSuccessTrackerFactory::
    ~IOSChromePasswordChangeSuccessTrackerFactory() = default;

std::unique_ptr<KeyedService>
IOSChromePasswordChangeSuccessTrackerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<password_manager::PasswordChangeSuccessTrackerImpl>(
      ChromeBrowserState::FromBrowserState(context)->GetPrefs());
}
