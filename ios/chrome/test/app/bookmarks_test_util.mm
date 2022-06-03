// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/bookmarks_test_util.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

bool BookmarksLoaded() {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(GetOriginalBrowserState());
  return bookmarkModel->loaded();
}

bool ClearBookmarks() {
  ChromeBrowserState* browserState = GetOriginalBrowserState();
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);
  BOOL removeSucceeded = RemoveAllUserBookmarksIOS(browserState);
  return removeSucceeded && !bookmarkModel->HasBookmarks();
}

}  // namespace chrome_test_util
