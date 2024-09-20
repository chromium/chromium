// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace password_manager {
class PasswordReuseManager;
}

// Creates instances of PasswordReuseManager per profile.
class IOSChromePasswordReuseManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static password_manager::PasswordReuseManager* GetForBrowserState(
      ProfileIOS* profile);

  static password_manager::PasswordReuseManager* GetForProfile(
      ProfileIOS* profile);
  static IOSChromePasswordReuseManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromePasswordReuseManagerFactory>;

  IOSChromePasswordReuseManagerFactory();
  ~IOSChromePasswordReuseManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_REUSE_MANAGER_FACTORY_H_
