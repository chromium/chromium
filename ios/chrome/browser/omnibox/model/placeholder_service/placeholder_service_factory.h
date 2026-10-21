// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all PlaceholderServices and associates them with
// Profile.
class PlaceholderServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PlaceholderServiceFactory,
                                                PlaceholderService> {
 public:
  explicit PlaceholderServiceFactory(PassKey key);

  // Returns the default factory used to build PlaceholderServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_PLACEHOLDER_SERVICE_PLACEHOLDER_SERVICE_FACTORY_H_
