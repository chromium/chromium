// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace power_bookmarks {
class PowerBookmarkService;
}

// Factory to create one PowerBookmarkService per profile.
class PowerBookmarkServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static power_bookmarks::PowerBookmarkService* GetForProfile(
      ProfileIOS* profile);
  static PowerBookmarkServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<PowerBookmarkServiceFactory>;

  PowerBookmarkServiceFactory();
  ~PowerBookmarkServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_
