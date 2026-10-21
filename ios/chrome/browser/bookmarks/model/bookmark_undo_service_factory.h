// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_

#import "components/undo/bookmark_undo_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class BookmarkUndoServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<BookmarkUndoServiceFactory,
                                                BookmarkUndoService> {
 public:
  explicit BookmarkUndoServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
