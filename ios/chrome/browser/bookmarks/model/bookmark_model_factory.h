// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_FACTORY_H_

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

class BookmarkModelFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BookmarkModelFactory,
                                                bookmarks::BookmarkModel> {
 public:
  explicit BookmarkModelFactory(PassKey key);

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_MODEL_FACTORY_H_
