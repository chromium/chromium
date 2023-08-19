// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace {

class BookmarksUtilsTest : public BookmarkIOSUnitTestSupport {
 protected:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    prefs_ = chrome_browser_state_->GetPrefs();
    mobile_node_ = local_or_syncable_bookmark_model_->mobile_node();
    folder_node_ = AddFolder(mobile_node_, u"Folder");
    bookmark_node_ = AddBookmark(folder_node_, u"Bookmark");
  }

  void SetDefaultBookmarkFolderPrefsHelper(int64_t folder_id) {
    prefs_->SetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
                     folder_id);
    prefs_->SetInteger(
        prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
        static_cast<int>(bookmarks::StorageType::kLocalOrSyncable));
  }
  const bookmarks::BookmarkNode* GetDefaultBookmarkFolderHelper() {
    return GetDefaultBookmarkFolder(
        prefs_, /*is_account_bookmark_model_available*/ false,
        local_or_syncable_bookmark_model_, account_bookmark_model_);
  }

  PrefService* prefs_ = nullptr;
  const bookmarks::BookmarkNode* mobile_node_ = nullptr;
  const bookmarks::BookmarkNode* folder_node_ = nullptr;
  const bookmarks::BookmarkNode* bookmark_node_ = nullptr;
};

// Tests GetDefaultBookmarkFolder() when not default folder was set.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithNoValueSet) {
  // Test default folder, with no value set before.
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, mobile_node_);
}

// Tests when an unknown id is set as the default folder.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithWrongValue) {
  SetDefaultBookmarkFolderPrefsHelper(-1);
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, mobile_node_);
}

// Tests when the folder is set.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithDefaultFolderSet) {
  SetDefaultBookmarkFolderPrefsHelper(folder_node_->id());
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, folder_node_);
}

// Test when a bookmark node is set as the default folder.
// See crbug.com/1450146.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithDefaultBookmarkSet) {
  SetDefaultBookmarkFolderPrefsHelper(bookmark_node_->id());
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, mobile_node_);
}

}  // namespace
