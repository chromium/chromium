// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_USER_PERMISSION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_USER_PERMISSION_SERVICE_FACTORY_H_

#import <memory>

#import "components/device_signals/core/browser/user_permission_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

class IOSUserPermissionServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSUserPermissionServiceFactory,
          device_signals::UserPermissionService> {
 public:
  explicit IOSUserPermissionServiceFactory(PassKey key);

 private:
  // TypedProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_SIGNALS_MODEL_IOS_USER_PERMISSION_SERVICE_FACTORY_H_
