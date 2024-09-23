// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;

class BookmarksUtilsTest : public BookmarkIOSUnitTestSupport {
 protected:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    prefs_ = profile_->GetPrefs();
    account_folder_node_ =
        AddFolder(bookmark_model_->account_mobile_node(), u"Account folder");
    local_folder_node_ =
        AddFolder(bookmark_model_->mobile_node(), u"Local folder");
    local_bookmark_node_ = AddBookmark(local_folder_node_, u"Bookmark");
  }

  void SetDefaultBookmarkFolderPrefsHelper(int64_t folder_id) {
    prefs_->SetInt64(prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
                     folder_id);
    // Used for metrics only, when a node isn't found.
    prefs_->SetInteger(prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
                       static_cast<int>(BookmarkStorageType::kLocalOrSyncable));
  }

  const bookmarks::BookmarkNode* GetDefaultBookmarkFolderHelper() {
    return GetDefaultBookmarkFolder(prefs_, bookmark_model_);
  }

  raw_ptr<PrefService> prefs_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> account_folder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> local_folder_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> local_bookmark_node_ = nullptr;
  base::HistogramTester histogram_tester_;
};

// Tests GetDefaultBookmarkFolder() when no default folder was set and account
// bookmarks exist.
TEST_F(BookmarksUtilsTest,
       GetDefaultBookmarkFolderWithNoValueSetAndExistingAccountBookmarks) {
  // Test default folder, with no value set before.
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, bookmark_model_->account_mobile_node());
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kUnset, 1);
}

// Tests GetDefaultBookmarkFolder() when no default folder was set and account
// bookmarks do not exist.
TEST_F(BookmarksUtilsTest,
       GetDefaultBookmarkFolderWithNoValueSetAndWithoutAccountBookmarks) {
  bookmark_model_->RemoveAccountPermanentFolders();
  // Test default folder, with no value set before.
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, bookmark_model_->mobile_node());
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kUnset, 1);
}

// Tests when an id of -1 (kLastUsedBookmarkFolderNone) is set as the default
// folder.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithValueSetToMinusOne) {
  SetDefaultBookmarkFolderPrefsHelper(-1);
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, bookmark_model_->account_mobile_node());
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kUnset, 1);
}

// Tests when an unknown id is set as the default folder.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithWrongValue) {
  SetDefaultBookmarkFolderPrefsHelper(123);
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, bookmark_model_->account_mobile_node());
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kMissingLocalFolderSet, 1);
}

// Tests when the folder is set to a local bookmark.
TEST_F(BookmarksUtilsTest,
       GetDefaultBookmarkFolderWithDefaultFolderSetToLocalFolder) {
  SetDefaultBookmarkFolderPrefsHelper(local_folder_node_->id());
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, local_folder_node_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kExistingLocalFolderSet, 1);
}

// Tests when the folder is set to a local bookmark.
TEST_F(BookmarksUtilsTest,
       GetDefaultBookmarkFolderWithDefaultFolderSetToAnAccountFolder) {
  SetDefaultBookmarkFolderPrefsHelper(account_folder_node_->id());
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, account_folder_node_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kExistingAccountFolderSet, 1);
}

// Test when a bookmark node is set as the default folder.
// See crbug.com/1450146.
TEST_F(BookmarksUtilsTest, GetDefaultBookmarkFolderWithDefaultBookmarkSet) {
  SetDefaultBookmarkFolderPrefsHelper(local_bookmark_node_->id());
  const bookmarks::BookmarkNode* default_folder_node =
      GetDefaultBookmarkFolderHelper();
  EXPECT_EQ(default_folder_node, bookmark_model_->account_mobile_node());
  histogram_tester_.ExpectUniqueSample(
      "IOS.Bookmarks.DefaultBookmarkFolderOutcome",
      DefaultBookmarkFolderOutcomeForMetrics::kMissingLocalFolderSet, 1);
}

TEST_F(BookmarksUtilsTest, PrimaryPermanentNodes) {
  EXPECT_THAT(PrimaryPermanentNodes(bookmark_model_,
                                    BookmarkStorageType::kLocalOrSyncable),
              ElementsAre(bookmark_model_->mobile_node(),
                          bookmark_model_->bookmark_bar_node(),
                          bookmark_model_->other_node()));
  EXPECT_THAT(
      PrimaryPermanentNodes(bookmark_model_, BookmarkStorageType::kAccount),
      ElementsAre(bookmark_model_->account_mobile_node(),
                  bookmark_model_->account_bookmark_bar_node(),
                  bookmark_model_->account_other_node()));

  bookmark_model_->RemoveAccountPermanentFolders();
  EXPECT_THAT(
      PrimaryPermanentNodes(bookmark_model_, BookmarkStorageType::kAccount),
      IsEmpty());
}

}  // namespace
