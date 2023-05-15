// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"

class ChromeBrowserState;

namespace password_manager {
class PasswordReuseManager;
}

// Creates instances of PasswordReuseManager per ChromeBrowserState.
class IOSChromePasswordReuseManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromePasswordReuseManagerFactory* GetInstance();
  static password_manager::PasswordReuseManager* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromePasswordReuseManagerFactory>;

  IOSChromePasswordReuseManagerFactory();
  ~IOSChromePasswordReuseManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
