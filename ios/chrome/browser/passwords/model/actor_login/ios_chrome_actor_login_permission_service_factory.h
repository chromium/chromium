// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_

#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `ActorLoginPermissionService` instances and
// associates them with `ProfileIOS` instances.
class IOSChromeActorLoginPermissionServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeActorLoginPermissionServiceFactory,
          actor_login::ActorLoginPermissionService> {
 public:
  explicit IOSChromeActorLoginPermissionServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_ACTOR_LOGIN_IOS_CHROME_ACTOR_LOGIN_PERMISSION_SERVICE_FACTORY_H_
