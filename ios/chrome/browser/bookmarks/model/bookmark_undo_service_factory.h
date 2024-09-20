// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class BookmarkUndoService;

namespace ios {
// Singleton that owns all FaviconServices and associates them with
// ProfileIOS.
class BookmarkUndoServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static BookmarkUndoService* GetForProfile(ProfileIOS* profile);
  static BookmarkUndoService* GetForProfileIfExists(ProfileIOS* profile);
  static BookmarkUndoServiceFactory* GetInstance();

  BookmarkUndoServiceFactory(const BookmarkUndoServiceFactory&) = delete;
  BookmarkUndoServiceFactory& operator=(const BookmarkUndoServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<BookmarkUndoServiceFactory>;

  BookmarkUndoServiceFactory();
  ~BookmarkUndoServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_UNDO_SERVICE_FACTORY_H_
