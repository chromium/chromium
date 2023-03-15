// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/bookmarks_test_util.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

bool BookmarksLoaded() {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          GetOriginalBrowserState());
  return bookmarkModel->loaded();
}

bool ClearBookmarks() {
  ChromeBrowserState* browserState = GetOriginalBrowserState();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          browserState);
  BOOL removeSucceeded = RemoveAllUserBookmarksIOS(browserState);
  return removeSucceeded && !bookmarkModel->HasBookmarks();
}

}  // namespace chrome_test_util
