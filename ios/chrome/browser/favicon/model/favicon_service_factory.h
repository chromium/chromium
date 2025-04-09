// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
enum class ServiceAccessType;

namespace favicon {
class FaviconService;
}

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class FaviconServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static favicon::FaviconService* GetForProfile(ProfileIOS* profile,
                                                ServiceAccessType access_type);
  static FaviconServiceFactory* GetInstance();
  // Returns the default factory used to build FaviconService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<FaviconServiceFactory>;

  FaviconServiceFactory();
  ~FaviconServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
