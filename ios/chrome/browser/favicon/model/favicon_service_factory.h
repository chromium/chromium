// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

enum class ServiceAccessType;

namespace favicon {
class FaviconService;
}

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class FaviconServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358299863): Remove when fully migrated.
  static favicon::FaviconService* GetForBrowserState(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  static favicon::FaviconService* GetForProfile(ProfileIOS* profile,
                                                ServiceAccessType access_type);
  static FaviconServiceFactory* GetInstance();
  // Returns the default factory used to build FaviconService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  FaviconServiceFactory(const FaviconServiceFactory&) = delete;
  FaviconServiceFactory& operator=(const FaviconServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<FaviconServiceFactory>;

  FaviconServiceFactory();
  ~FaviconServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_SERVICE_FACTORY_H_
