// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_IOS_UNITTEST_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_IOS_UNITTEST_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

class TestChromeBrowserState;

// Provides common bookmark testing infrastructure.
class BookmarkIOSUnitTest : public PlatformTest {
 public:
  BookmarkIOSUnitTest();
  ~BookmarkIOSUnitTest() override;

 protected:
  void SetUp() override;
  const bookmarks::BookmarkNode* AddBookmark(
      const bookmarks::BookmarkNode* parent,
      NSString* title);
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      NSString* title);
  void ChangeTitle(NSString* title, const bookmarks::BookmarkNode* node);

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  bookmarks::BookmarkModel* _bookmarkModel;
  bookmarks::ManagedBookmarkService* _managedBookmarkService;
};

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_IOS_UNITTEST_H_
