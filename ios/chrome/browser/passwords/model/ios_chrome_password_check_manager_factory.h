// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_

#import "base/memory/weak_ptr.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class IOSChromePasswordCheckManager;

// Singleton that owns weak pointer to IOSChromePasswordCheckManager.
class IOSChromePasswordCheckManagerFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static scoped_refptr<IOSChromePasswordCheckManager> GetForBrowserState(
      ProfileIOS* profile);

  static scoped_refptr<IOSChromePasswordCheckManager> GetForProfile(
      ProfileIOS* profile);
  static IOSChromePasswordCheckManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromePasswordCheckManagerFactory>;

  IOSChromePasswordCheckManagerFactory();
  ~IOSChromePasswordCheckManagerFactory() override;

  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
