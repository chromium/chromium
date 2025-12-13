// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class GoogleLogoService;
class KeyedService;
class ProfileIOS;

// Singleton that owns all GoogleLogoServices and associates them with
// profiles.
class GoogleLogoServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static GoogleLogoService* GetForProfile(ProfileIOS* profile);
  static GoogleLogoServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<GoogleLogoServiceFactory>;

  GoogleLogoServiceFactory();
  ~GoogleLogoServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
