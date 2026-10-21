// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_

#import "components/power_bookmarks/core/power_bookmark_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory to create one PowerBookmarkService per profile.
class PowerBookmarkServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PowerBookmarkServiceFactory,
          power_bookmarks::PowerBookmarkService> {
 public:
  explicit PowerBookmarkServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_POWER_BOOKMARKS_MODEL_POWER_BOOKMARK_SERVICE_FACTORY_H_
