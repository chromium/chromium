// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class PageContentCacheBridgeService;

// Associates PageContentCacheBridgeService instances with a ProfileIOS object.
class PageContentCacheBridgeServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the PageContentCacheBridgeService associated with `profile`.
  static PageContentCacheBridgeService* GetForProfile(ProfileIOS* profile);

  // Returns the factory instance.
  static PageContentCacheBridgeServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PageContentCacheBridgeServiceFactory>;

  PageContentCacheBridgeServiceFactory();
  ~PageContentCacheBridgeServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_BRIDGE_SERVICE_FACTORY_H_
