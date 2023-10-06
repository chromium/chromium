// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace password_manager {
class PasswordSenderService;
}

// Creates instances of PasswordSenderService per BrowserState.
class IOSChromePasswordSenderServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromePasswordSenderServiceFactory* GetInstance();
  static password_manager::PasswordSenderService* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromePasswordSenderServiceFactory>;

  IOSChromePasswordSenderServiceFactory();
  ~IOSChromePasswordSenderServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PASSWORD_SENDER_SERVICE_FACTORY_H_
