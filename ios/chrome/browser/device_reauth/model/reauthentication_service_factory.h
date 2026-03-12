// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class ReauthenticationService;

// Singleton that owns a reauthentication module for a profile.
class ReauthenticationServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ReauthenticationServiceFactory* GetInstance();
  static ReauthenticationService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ReauthenticationServiceFactory>;

  ReauthenticationServiceFactory();
  ~ReauthenticationServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_REAUTHENTICATION_SERVICE_FACTORY_H_
