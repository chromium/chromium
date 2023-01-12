// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {

class BookmarkPathCacheTest : public BookmarkIOSUnitTestSupport {
 protected:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    [BookmarkPathCache registerBrowserStatePrefs:prefs_.registry()];
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(BookmarkPathCacheTest, TestPathCache) {
  // Try to store and retrieve a cache.
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobile_node, @"f1");
  int64_t folder_id = f1->id();
  int topmost_row = 23;
  [BookmarkPathCache cacheBookmarkTopMostRowWithPrefService:&prefs_
                                                   folderId:folder_id
                                                 topMostRow:topmost_row];

  int64_t result_folder_id;
  int result_topmost_row;
  [BookmarkPathCache
      getBookmarkTopMostRowCacheWithPrefService:&prefs_
                                          model:bookmark_model_
                                       folderId:&result_folder_id
                                     topMostRow:&result_topmost_row];
  EXPECT_EQ(folder_id, result_folder_id);
  EXPECT_EQ(topmost_row, result_topmost_row);
}

TEST_F(BookmarkPathCacheTest, TestPathCacheWhenFolderDeleted) {
  // Try to store and retrieve a cache after the cached path is deleted.
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobile_node, @"f1");
  int64_t folder_id = f1->id();
  int topmost_row = 23;
  [BookmarkPathCache cacheBookmarkTopMostRowWithPrefService:&prefs_
                                                   folderId:folder_id
                                                 topMostRow:topmost_row];

  // Delete the folder.
  bookmark_model_->Remove(f1);

  int64_t unused_folder_id;
  int unused_topmost_row;
  BOOL result = [BookmarkPathCache
      getBookmarkTopMostRowCacheWithPrefService:&prefs_
                                          model:bookmark_model_
                                       folderId:&unused_folder_id
                                     topMostRow:&unused_topmost_row];
  ASSERT_FALSE(result);
}

}  // anonymous namespace
