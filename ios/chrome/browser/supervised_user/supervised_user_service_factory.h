// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

class ChromeBrowserState;

// Singleton that owns SupervisedUserService object and associates
// them with ChromeBrowserState.
class SupervisedUserServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static supervised_user::SupervisedUserService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static SupervisedUserServiceFactory* GetInstance();

  SupervisedUserServiceFactory(const SupervisedUserServiceFactory&) = delete;
  SupervisedUserServiceFactory& operator=(const SupervisedUserServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<SupervisedUserServiceFactory>;

  SupervisedUserServiceFactory();
  ~SupervisedUserServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_FACTORY_H_
