// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

class Browser;
class GURL;
class TestChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

// Provides common bookmark testing infrastructure.
class BookmarkIOSUnitTestSupport : public PlatformTest {
 public:
  BookmarkIOSUnitTestSupport();
  ~BookmarkIOSUnitTestSupport() override;

 protected:
  void SetUp() override;
  const bookmarks::BookmarkNode* AddBookmark(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title);
  const bookmarks::BookmarkNode* AddBookmark(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title,
      const GURL& url);
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title);
  void ChangeTitle(const std::u16string& title,
                   const bookmarks::BookmarkNode* node);
  bookmarks::BookmarkModel* GetBookmarkModelForNode(
      const bookmarks::BookmarkNode* node);

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Bookmark model for the LocalOrSyncable storage.
  bookmarks::BookmarkModel* local_or_syncable_bookmark_model_;
  // Bookmark model for the account storage.
  bookmarks::BookmarkModel* account_bookmark_model_;
  bookmarks::ManagedBookmarkService* managed_bookmark_service_;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
