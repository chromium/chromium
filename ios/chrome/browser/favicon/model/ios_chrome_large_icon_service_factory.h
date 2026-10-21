// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_

#import "components/favicon/core/large_icon_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all LargeIconService and associates them with
// ProfileIOS.
class IOSChromeLargeIconServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeLargeIconServiceFactory,
          favicon::LargeIconService> {
 public:
  explicit IOSChromeLargeIconServiceFactory(PassKey key);

  // Returns the default factory used to build LargeIconServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
