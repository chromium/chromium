// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/supervised_user/core/browser/child_account_service.h"

class ChromeBrowserState;

// Singleton that owns ChildAccountService object and associates
// them with ChromeBrowserState.
class ChildAccountServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static supervised_user::ChildAccountService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static ChildAccountServiceFactory* GetInstance();

  ChildAccountServiceFactory(const ChildAccountServiceFactory&) = delete;
  ChildAccountServiceFactory& operator=(const ChildAccountServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<ChildAccountServiceFactory>;

  ChildAccountServiceFactory();
  ~ChildAccountServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNT_SERVICE_FACTORY_H_
