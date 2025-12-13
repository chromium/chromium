// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace bookmarks {
class ManagedBookmarkService;
}

// Singleton that owns all ManagedBookmarkService and associates them with
// profiles.
class ManagedBookmarkServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static bookmarks::ManagedBookmarkService* GetForProfile(ProfileIOS* profile);
  static ManagedBookmarkServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<ManagedBookmarkServiceFactory>;

  ManagedBookmarkServiceFactory();
  ~ManagedBookmarkServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
