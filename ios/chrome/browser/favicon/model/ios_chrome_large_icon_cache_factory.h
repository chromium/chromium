// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class LargeIconCache;

// Singleton that owns all LargeIconCaches and associates them with
// ProfileIOS.
class IOSChromeLargeIconCacheFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static LargeIconCache* GetForProfile(ProfileIOS* profile);
  static IOSChromeLargeIconCacheFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeLargeIconCacheFactory>;

  IOSChromeLargeIconCacheFactory();
  ~IOSChromeLargeIconCacheFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
