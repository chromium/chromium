// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_path_cache.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
  const BookmarkNode* f1 = AddFolder(mobile_node, u"f1");
  int64_t folder_id = f1->id();
  int topmost_row = 23;
  [BookmarkPathCache
      cacheBookmarkTopMostRowWithPrefService:&prefs_
                                    folderId:folder_id
                                   inStorage:BookmarkStorageType::
                                                 kLocalOrSyncable
                                  topMostRow:topmost_row];

  int64_t result_folder_id;
  int result_topmost_row;
  [BookmarkPathCache
      bookmarkTopMostRowCacheWithPrefService:&prefs_
                               bookmarkModel:bookmark_model_
                                    folderId:&result_folder_id
                                  topMostRow:&result_topmost_row];
  EXPECT_EQ(folder_id, result_folder_id);
  EXPECT_EQ(topmost_row, result_topmost_row);
}

TEST_F(BookmarkPathCacheTest, TestPathCacheWhenFolderDeleted) {
  // Try to store and retrieve a cache after the cached path is deleted.
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobile_node, u"f1");
  int64_t folder_id = f1->id();
  int topmost_row = 23;
  [BookmarkPathCache
      cacheBookmarkTopMostRowWithPrefService:&prefs_
                                    folderId:folder_id
                                   inStorage:BookmarkStorageType::
                                                 kLocalOrSyncable
                                  topMostRow:topmost_row];

  // Delete the folder.
  bookmark_model_->Remove(f1, bookmarks::metrics::BookmarkEditSource::kOther,
                          FROM_HERE);

  int64_t unused_folder_id;
  int unused_topmost_row;
  BOOL result = [BookmarkPathCache
      bookmarkTopMostRowCacheWithPrefService:&prefs_
                               bookmarkModel:bookmark_model_
                                    folderId:&unused_folder_id
                                  topMostRow:&unused_topmost_row];
  ASSERT_FALSE(result);
}

}  // anonymous namespace
