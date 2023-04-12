// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks
class Browser;
class TestChromeBrowserState;

// Provides common bookmark testing infrastructure.
class BookmarkIOSUnitTestSupport : public PlatformTest {
 public:
  BookmarkIOSUnitTestSupport();
  ~BookmarkIOSUnitTestSupport() override;

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
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Bookmark model for the profile storage.
  bookmarks::BookmarkModel* profile_bookmark_model_;
  bookmarks::ManagedBookmarkService* managed_bookmark_service_;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
