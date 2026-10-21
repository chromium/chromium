// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_

#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns a reauthentication module for a profile.
class ReauthenticationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ReauthenticationServiceFactory,
                                                ReauthenticationService> {
 public:
  explicit ReauthenticationServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_
