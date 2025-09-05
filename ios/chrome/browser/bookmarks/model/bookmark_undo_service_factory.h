// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class BookmarkUndoService;
class ProfileIOS;

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class BookmarkUndoServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static BookmarkUndoService* GetForProfile(ProfileIOS* profile);
  static BookmarkUndoService* GetForProfileIfExists(ProfileIOS* profile);
  static BookmarkUndoServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<BookmarkUndoServiceFactory>;

  BookmarkUndoServiceFactory();
  ~BookmarkUndoServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
