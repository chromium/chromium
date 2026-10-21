// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/intelligence/persist_tab_context/model/page_content_cache_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Associates PageContentCacheService instances with a ProfileIOS object.
class PageContentCacheServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<PageContentCacheServiceFactory,
                                                PageContentCacheService> {
 public:
  explicit PageContentCacheServiceFactory(PassKey key);

  // Returns the storage path for the PageContentCacheService for `profile`.
  static base::FilePath GetStoragePathForProfile(ProfileIOS* profile);

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PERSIST_TAB_CONTEXT_MODEL_PAGE_CONTENT_CACHE_SERVICE_FACTORY_H_
