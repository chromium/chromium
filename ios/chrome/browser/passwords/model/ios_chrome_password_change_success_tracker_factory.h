// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace password_manager {
class PasswordChangeSuccessTracker;
}

// Creates instances of PasswordChangeSuccessTracker per ChromeBrowserState.
// Returns no instance in the incognito mode.
class IOSChromePasswordChangeSuccessTrackerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromePasswordChangeSuccessTrackerFactory* GetInstance();
  static password_manager::PasswordChangeSuccessTracker* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromePasswordChangeSuccessTrackerFactory>;

  IOSChromePasswordChangeSuccessTrackerFactory();
  ~IOSChromePasswordChangeSuccessTrackerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHANGE_SUCCESS_TRACKER_FACTORY_H_
