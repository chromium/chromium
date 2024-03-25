// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

class Browser;
class GURL;
class LegacyBookmarkModel;
class TestChromeBrowserState;

namespace bookmarks {
class BookmarkNode;
class CoreBookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

// Provides common bookmark testing infrastructure.
class BookmarkIOSUnitTestSupport : public PlatformTest {
 public:
  explicit BookmarkIOSUnitTestSupport(bool wait_for_initialization = true);
  ~BookmarkIOSUnitTestSupport() override;

 protected:
  void SetUp() override;

  // Allows subclasses to add custom logic immediately following the creation
  // of the BrowserState, before keyed services are created.
  virtual void SetUpBrowserStateBeforeCreatingServices();

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
  LegacyBookmarkModel* GetBookmarkModelForNode(
      const bookmarks::BookmarkNode* node);

  const bool wait_for_initialization_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Bookmark model for the LocalOrSyncable storage.
  raw_ptr<LegacyBookmarkModel> local_or_syncable_bookmark_model_;
  // Bookmark model for the account storage.
  raw_ptr<LegacyBookmarkModel> account_bookmark_model_;
  // Bookmark model providing a merged view.
  raw_ptr<bookmarks::CoreBookmarkModel> bookmark_model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
