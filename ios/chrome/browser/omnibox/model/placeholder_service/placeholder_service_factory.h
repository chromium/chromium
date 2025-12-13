// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class PlaceholderService;

namespace ios {
// Singleton that owns all PlaceholderServices and associates them with
// Profile.
class PlaceholderServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PlaceholderService* GetForProfile(ProfileIOS* profile);
  static PlaceholderServiceFactory* GetInstance();

  // Returns the default factory used to build PlaceholderServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PlaceholderServiceFactory>;

  PlaceholderServiceFactory();
  ~PlaceholderServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_
