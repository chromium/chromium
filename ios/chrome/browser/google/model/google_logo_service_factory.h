// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_

#import "ios/chrome/browser/google/model/google_logo_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all GoogleLogoServices and associates them with
// profiles.
class GoogleLogoServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<GoogleLogoServiceFactory,
                                                GoogleLogoService> {
 public:
  explicit GoogleLogoServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_MODEL_GOOGLE_LOGO_SERVICE_FACTORY_H_
