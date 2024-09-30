// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;
class LargeIconCache;

// Singleton that owns all LargeIconCaches and associates them with
// ProfileIOS.
class IOSChromeLargeIconCacheFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static LargeIconCache* GetForBrowserState(ProfileIOS* profile);

  static LargeIconCache* GetForProfile(ProfileIOS* profile);
  static IOSChromeLargeIconCacheFactory* GetInstance();

  IOSChromeLargeIconCacheFactory(const IOSChromeLargeIconCacheFactory&) =
      delete;
  IOSChromeLargeIconCacheFactory& operator=(
      const IOSChromeLargeIconCacheFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeLargeIconCacheFactory>;

  IOSChromeLargeIconCacheFactory();
  ~IOSChromeLargeIconCacheFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_CACHE_FACTORY_H_
