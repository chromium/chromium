// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_

#import "components/favicon/core/favicon_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_access.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class FaviconServiceFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                        FaviconServiceFactory,
                                        favicon::FaviconService,
                                        RejectImplicitAccessIfIncognito> {
 public:
  explicit FaviconServiceFactory(PassKey key);
  // Returns the default factory used to build FaviconService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
