// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class IOSChromePasswordCheckManager;

// Singleton that owns weak pointer to IOSChromePasswordCheckManager.
class IOSChromePasswordCheckManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromePasswordCheckManagerFactory* GetInstance();
  static scoped_refptr<IOSChromePasswordCheckManager> GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromePasswordCheckManagerFactory>;

  IOSChromePasswordCheckManagerFactory();
  ~IOSChromePasswordCheckManagerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_CHECK_MANAGER_FACTORY_H_
