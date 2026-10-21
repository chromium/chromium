// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_

#import "components/bookmarks/managed/managed_bookmark_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all ManagedBookmarkService and associates them with
// profiles.
class ManagedBookmarkServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ManagedBookmarkServiceFactory,
          bookmarks::ManagedBookmarkService> {
 public:
  explicit ManagedBookmarkServiceFactory(PassKey key);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_MANAGED_BOOKMARK_SERVICE_FACTORY_H_
