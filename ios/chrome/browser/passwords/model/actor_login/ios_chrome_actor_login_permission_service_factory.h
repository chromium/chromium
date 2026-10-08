// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace actor_login {
class ActorLoginPermissionService;
}  // namespace actor_login

// Singleton that owns all `ActorLoginPermissionService` instances and
// associates them with `ProfileIOS` instances.
class IOSChromeActorLoginPermissionServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static actor_login::ActorLoginPermissionService* GetForProfile(
      ProfileIOS* profile);

  static IOSChromeActorLoginPermissionServiceFactory* GetInstance();

  IOSChromeActorLoginPermissionServiceFactory(
      const IOSChromeActorLoginPermissionServiceFactory&) = delete;
  IOSChromeActorLoginPermissionServiceFactory& operator=(
      const IOSChromeActorLoginPermissionServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeActorLoginPermissionServiceFactory>;

  IOSChromeActorLoginPermissionServiceFactory();
  ~IOSChromeActorLoginPermissionServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
