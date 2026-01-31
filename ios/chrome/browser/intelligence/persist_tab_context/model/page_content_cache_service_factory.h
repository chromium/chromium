// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class PageContentCacheService;

// Associates PageContentCacheService instances with a ProfileIOS object.
class PageContentCacheServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the PageContentCacheService associated with `profile`.
  static PageContentCacheService* GetForProfile(ProfileIOS* profile);

  // Returns the storage path for the PageContentCacheService for `profile`.
  static base::FilePath GetStoragePathForProfile(ProfileIOS* profile);

  // Returns the factory instance.
  static PageContentCacheServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PageContentCacheServiceFactory>;

  PageContentCacheServiceFactory();
  ~PageContentCacheServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_
