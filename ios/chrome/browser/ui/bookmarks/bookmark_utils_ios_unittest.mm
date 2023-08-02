// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#import <iterator>
#import <memory>
#import <string>
#import <vector>

#import "base/ranges/algorithm.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/sync/base/features.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"

using bookmarks::BookmarkNode;

namespace {

std::vector<std::u16string> GetBookmarkTitles(
    const std::vector<std::unique_ptr<BookmarkNode>>& nodes) {
  std::vector<std::u16string> result;
  base::ranges::transform(nodes, std::back_inserter(result),
                          [](const auto& node) { return node->GetTitle(); });
  return result;
}

class BookmarkIOSUtilsUnitTest : public BookmarkIOSUnitTestSupport,
                                 public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        syncer::kEnableBookmarksAccountStorage, IsAccountStorageEnabled());
    BookmarkIOSUnitTestSupport::SetUp();
  }

  bool IsAccountStorageEnabled() const { return GetParam(); }

  base::Time timeFromEpoch(int days, int hours) {
    return base::Time::UnixEpoch() + base::Days(days) + base::Hours(hours);
  }

  void TestMovingBookmarks(const BookmarkNode* parent_folder) {
    const BookmarkNode* f1 = AddFolder(parent_folder, u"f1");
    const BookmarkNode* a = AddBookmark(parent_folder, u"a");
    AddBookmark(parent_folder, u"b");
    const BookmarkNode* f2 = AddFolder(parent_folder, u"f2");

    AddBookmark(f1, u"f1a");
    AddBookmark(f1, u"f1b");
    AddBookmark(f1, u"f1c");
    AddBookmark(f2, u"f2a");
    const BookmarkNode* f2b = AddBookmark(f2, u"f2b");

    std::vector<const BookmarkNode*> to_move{a, f2b, f2};
    EXPECT_TRUE(bookmark_utils_ios::MoveBookmarks(
        to_move, local_or_syncable_bookmark_model_, account_bookmark_model_,
        f1));

    // Moving within one model shouldn't change pointers in `to_move`.
    EXPECT_THAT(to_move, ::testing::ElementsAre(a, f2b, f2));

    EXPECT_THAT(GetBookmarkTitles(parent_folder->children()),
                ::testing::UnorderedElementsAre(u"f1", u"b"));
    EXPECT_THAT(GetBookmarkTitles(f1->children()),
                ::testing::UnorderedElementsAre(u"f1a", u"f1b", u"f1c", u"a",
                                                u"f2b", u"f2"));
    EXPECT_THAT(GetBookmarkTitles(f2->children()),
                ::testing::ElementsAre(u"f2a"));
  }
};

TEST_P(BookmarkIOSUtilsUnitTest, CreateOrUpdateNoop) {
  const BookmarkNode* mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  std::u16string title = u"title";
  const BookmarkNode* node = AddBookmark(mobile_node, title);

  GURL url_copy = node->GetTitledUrlNodeUrl();
  // This call is a no-op, , so `CreateOrUpdateBookmark` should return `false`.
  EXPECT_FALSE(bookmark_utils_ios::CreateOrUpdateBookmark(
      node, base::SysUTF16ToNSString(title), url_copy, mobile_node,
      local_or_syncable_bookmark_model_, account_bookmark_model_));
  EXPECT_EQ(node->GetTitle(), title);
}

TEST_P(BookmarkIOSUtilsUnitTest, CreateOrUpdateWithinModel) {
  const BookmarkNode* mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* node = AddBookmark(mobile_node, u"a");
  const BookmarkNode* folder = AddFolder(mobile_node, u"f1");

  NSString* new_title = @"b";
  GURL new_url("http://example.com");
  EXPECT_TRUE(bookmark_utils_ios::CreateOrUpdateBookmark(
      node, new_title, new_url, folder, local_or_syncable_bookmark_model_,
      account_bookmark_model_));

  ASSERT_THAT(mobile_node->children(),
              testing::ElementsAre(testing::Pointer(folder)));
  ASSERT_THAT(folder->children(), testing::ElementsAre(testing::Pointer(node)));
  EXPECT_EQ(node->GetTitle(), base::SysNSStringToUTF16(new_title));
  EXPECT_EQ(node->GetTitledUrlNodeUrl(), new_url);
}

// TODO(crbug.com/1446407): Add tests that call `CreateOrUpdateBookmark` with
//                          the account storage.

TEST_P(BookmarkIOSUtilsUnitTest, CreateOrUpdateBetweenModels) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to move bookmarks between storages";
  }
  const BookmarkNode* local_or_syncable_mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* node = AddBookmark(local_or_syncable_mobile_node, u"a");
  const BookmarkNode* account_mobile_node =
      account_bookmark_model_->mobile_node();

  NSString* new_title = @"b";
  GURL new_url("http://example.com");
  EXPECT_TRUE(bookmark_utils_ios::CreateOrUpdateBookmark(
      node, new_title, new_url, account_mobile_node,
      local_or_syncable_bookmark_model_, account_bookmark_model_));

  EXPECT_THAT(local_or_syncable_mobile_node->children(), testing::IsEmpty());
  ASSERT_THAT(account_mobile_node->children(), testing::SizeIs(1));
  const BookmarkNode* moved_node = account_mobile_node->children()[0].get();
  EXPECT_EQ(moved_node->GetTitle(), base::SysNSStringToUTF16(new_title));
  EXPECT_EQ(moved_node->GetTitledUrlNodeUrl(), new_url);
}

TEST_P(BookmarkIOSUtilsUnitTest, DeleteNodes) {
  const BookmarkNode* mobileNode =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, u"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, u"a");
  const BookmarkNode* b = AddBookmark(mobileNode, u"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, u"f2");

  AddBookmark(f1, u"f1a");
  AddBookmark(f1, u"f1b");
  AddBookmark(f1, u"f1c");
  AddBookmark(f2, u"f2a");
  const BookmarkNode* f2b = AddBookmark(f2, u"f2b");

  std::set<const BookmarkNode*> toDelete;
  toDelete.insert(a);
  toDelete.insert(f2b);
  toDelete.insert(f2);

  bookmark_utils_ios::DeleteBookmarks(toDelete,
                                      local_or_syncable_bookmark_model_);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(3u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_P(BookmarkIOSUtilsUnitTest, MoveNodesInLocalOrSyncableModel) {
  const BookmarkNode* local_or_syncable_mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  ASSERT_NO_FATAL_FAILURE(TestMovingBookmarks(local_or_syncable_mobile_node));
}

TEST_P(BookmarkIOSUtilsUnitTest, MoveNodesInAccountModel) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to move bookmarks in it";
  }
  const BookmarkNode* account_mobile_node =
      account_bookmark_model_->mobile_node();
  ASSERT_NO_FATAL_FAILURE(TestMovingBookmarks(account_mobile_node));
}

TEST_P(BookmarkIOSUtilsUnitTest, MoveNodesBetweenModels) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to move bookmarks between storages";
  }
  const BookmarkNode* local_or_syncable_mobile_node =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(local_or_syncable_mobile_node, u"f1");
  AddBookmark(local_or_syncable_mobile_node, u"a");
  const BookmarkNode* b = AddBookmark(local_or_syncable_mobile_node, u"b");
  const BookmarkNode* f1a = AddBookmark(f1, u"f1a");
  AddBookmark(f1, u"f1b");

  const BookmarkNode* account_mobile_node =
      account_bookmark_model_->mobile_node();
  const BookmarkNode* c = AddBookmark(account_mobile_node, u"c");
  const BookmarkNode* f2 = AddFolder(account_mobile_node, u"f2");
  const BookmarkNode* f2a = AddBookmark(f2, u"f2a");

  std::vector<const BookmarkNode*> to_move;
  to_move.push_back(f1);   // Cross-storage move.
  to_move.push_back(f1a);  // Cross-storage move, the parent is also moved.
  to_move.push_back(b);    // Cross-storage move, the parent is not moved.
  to_move.push_back(c);    // Same-storage move.

  bookmark_utils_ios::MoveBookmarks(to_move, local_or_syncable_bookmark_model_,
                                    account_bookmark_model_, f2);

  EXPECT_THAT(GetBookmarkTitles(local_or_syncable_mobile_node->children()),
              ::testing::ElementsAre(u"a"));
  EXPECT_THAT(GetBookmarkTitles(account_mobile_node->children()),
              ::testing::ElementsAre(u"f2"));

  ASSERT_EQ(to_move.size(), 4u);
  const BookmarkNode* moved_f1 = to_move[0];
  EXPECT_EQ(moved_f1->GetTitle(), u"f1");
  EXPECT_THAT(GetBookmarkTitles(moved_f1->children()),
              ::testing::ElementsAre(u"f1b"));
  const BookmarkNode* moved_f1a = to_move[1];
  EXPECT_EQ(moved_f1a->GetTitle(), u"f1a");
  const BookmarkNode* moved_b = to_move[2];
  EXPECT_EQ(moved_b->GetTitle(), u"b");
  const BookmarkNode* moved_c = to_move[3];
  EXPECT_EQ(moved_c->GetTitle(), u"c");

  EXPECT_THAT(f2->children(),
              ::testing::UnorderedElementsAre(
                  ::testing::Pointer(f2a), ::testing::Pointer(moved_f1),
                  ::testing::Pointer(moved_f1a), ::testing::Pointer(moved_b),
                  ::testing::Pointer(moved_c)));
}

TEST_P(BookmarkIOSUtilsUnitTest, TestCreateBookmarkPath) {
  const BookmarkNode* mobileNode =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, u"f1");
  NSArray<NSNumber*>* path = bookmark_utils_ios::CreateBookmarkPath(
      local_or_syncable_bookmark_model_, f1->id());
  NSMutableArray<NSNumber*>* expectedPath = [NSMutableArray array];
  [expectedPath addObject:@0];
  [expectedPath addObject:[NSNumber numberWithLongLong:mobileNode->id()]];
  [expectedPath addObject:[NSNumber numberWithLongLong:f1->id()]];
  EXPECT_TRUE([expectedPath isEqualToArray:path]);
}

TEST_P(BookmarkIOSUtilsUnitTest, TestCreateNilBookmarkPath) {
  NSArray<NSNumber*>* path = bookmark_utils_ios::CreateBookmarkPath(
      local_or_syncable_bookmark_model_, 999);
  EXPECT_TRUE(path == nil);
}

TEST_P(BookmarkIOSUtilsUnitTest, TestVisibleNonDescendantNodes) {
  const BookmarkNode* mobileNode =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* music = AddFolder(mobileNode, u"music");

  const BookmarkNode* pop = AddFolder(music, u"pop");
  const BookmarkNode* lindsey = AddBookmark(pop, u"lindsey lohan");
  AddBookmark(pop, u"katy perry");
  const BookmarkNode* gaga = AddFolder(pop, u"lady gaga");
  AddBookmark(gaga, u"gaga song 1");
  AddFolder(gaga, u"gaga folder 1");

  const BookmarkNode* metal = AddFolder(music, u"metal");
  AddFolder(metal, u"opeth");
  AddFolder(metal, u"F12");
  AddFolder(metal, u"f31");

  const BookmarkNode* animals = AddFolder(mobileNode, u"animals");
  AddFolder(animals, u"cat");
  const BookmarkNode* camel = AddFolder(animals, u"camel");
  AddFolder(camel, u"al paca");

  AddFolder(local_or_syncable_bookmark_model_->other_node(), u"buildings");

  std::set<const BookmarkNode*> obstructions;
  // Editing a folder and a bookmark.
  obstructions.insert(gaga);
  obstructions.insert(lindsey);

  bookmark_utils_ios::NodeVector result =
      bookmark_utils_ios::VisibleNonDescendantNodes(
          obstructions, local_or_syncable_bookmark_model_);
  ASSERT_EQ(13u, result.size());

  EXPECT_EQ(result[0]->GetTitle(), u"Mobile Bookmarks");
  EXPECT_EQ(result[1]->GetTitle(), u"animals");
  EXPECT_EQ(result[2]->GetTitle(), u"camel");
  EXPECT_EQ(result[3]->GetTitle(), u"al paca");
  EXPECT_EQ(result[4]->GetTitle(), u"cat");
  EXPECT_EQ(result[5]->GetTitle(), u"music");
  EXPECT_EQ(result[6]->GetTitle(), u"metal");
  EXPECT_EQ(result[7]->GetTitle(), u"F12");
  EXPECT_EQ(result[8]->GetTitle(), u"f31");
  EXPECT_EQ(result[9]->GetTitle(), u"opeth");
  EXPECT_EQ(result[10]->GetTitle(), u"pop");
  EXPECT_EQ(result[11]->GetTitle(), u"Other Bookmarks");
  EXPECT_EQ(result[12]->GetTitle(), u"buildings");
}

TEST_P(BookmarkIOSUtilsUnitTest, TestIsSubvectorOfNodes) {
  // Empty vectors: [] - [].
  bookmark_utils_ios::NodeVector vector1;
  bookmark_utils_ios::NodeVector vector2;
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // Empty vs vector with one element: [] - [1].
  const BookmarkNode* mobileNode =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, u"1");
  vector2.push_back(bookmark1);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // The same element in each: [1] - [1].
  vector1.push_back(bookmark1);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // One different element in each: [2] - [1].
  vector1.pop_back();
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, u"2");
  vector1.push_back(bookmark2);
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [2] - [1, 2].
  vector2.push_back(bookmark2);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [3] - [1, 2].
  vector1.pop_back();
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, u"3");
  vector1.push_back(bookmark3);
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [2, 3] - [1, 2, 3].
  vector1.insert(vector1.begin(), bookmark2);
  vector2.push_back(bookmark3);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [2, 3, 1] - [1, 2, 3].
  vector1.push_back(bookmark2);
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [1, 3] - [1, 2, 3].
  vector1.clear();
  vector1.push_back(bookmark1);
  vector1.push_back(bookmark2);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [1, 1] - [1, 2, 3].
  vector1.pop_back();
  vector1.push_back(bookmark1);
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [1, 1] - [1, 1, 2, 3].
  vector2.insert(vector2.begin(), bookmark1);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));
}

TEST_P(BookmarkIOSUtilsUnitTest, TestMissingNodes) {
  // [] - [].
  bookmark_utils_ios::NodeVector vector1;
  bookmark_utils_ios::NodeVector vector2;
  EXPECT_EQ(0u,
            bookmark_utils_ios::MissingNodesIndices(vector1, vector2).size());

  // [] - [1].
  const BookmarkNode* mobileNode =
      local_or_syncable_bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, u"1");
  vector2.push_back(bookmark1);
  std::vector<bookmark_utils_ios::NodeVector::size_type> missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [1] - [1].
  vector1.push_back(bookmark1);
  EXPECT_EQ(0u,
            bookmark_utils_ios::MissingNodesIndices(vector1, vector2).size());

  // [2] - [1, 2].
  vector1.pop_back();
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, u"2");
  vector1.push_back(bookmark2);
  vector2.push_back(bookmark2);
  missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [2, 3] - [1, 2, 3].
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, u"3");
  vector1.push_back(bookmark3);
  vector2.push_back(bookmark3);
  missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [1, 3] - [1, 2, 3].
  vector1.clear();
  vector1.push_back(bookmark1);
  vector1.push_back(bookmark3);
  missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(1u, missingNodesIndices[0]);

  // [1, 1] - [1, 1, 2, 3].
  vector1.pop_back();
  vector1.push_back(bookmark1);
  vector2.insert(vector2.begin(), bookmark1);
  missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(2u, missingNodesIndices.size());
  EXPECT_EQ(2u, missingNodesIndices[0]);
  EXPECT_EQ(3u, missingNodesIndices[1]);
}

// Tests returned values from `IsAccountBookmarkStorageOptedIn()`.
TEST_P(BookmarkIOSUtilsUnitTest, IsAccountBookmarkStorageOptedIn) {
  syncer::TestSyncService sync_service;

  // If the user is signed out, `IsAccountBookmarkStorageOptedIn()` should
  // always return false.
  EXPECT_FALSE(
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(&sync_service));

  // Sign-in.
  CoreAccountInfo account;
  account.gaia = "gaia_id";
  account.email = "email@test.com";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  sync_service.SetAccountInfo(account);

  // If sync-the-feature is on, including bookmarks,
  // `IsAccountBookmarkStorageOptedIn()` should always return false.
  sync_service.SetHasSyncConsent(true);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is on, but bookmarks excluded,
  // `IsAccountBookmarkStorageOptedIn()` should always return false.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is off and the account storage is enabled,
  // `IsAccountBookmarkStorageOptedIn()` should return true, but only if the
  // feature is enabled (IsAccountStorageEnabled()).
  sync_service.SetHasSyncConsent(false);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_EQ(IsAccountStorageEnabled(),
            bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is off and the account storage is not enabled,
  // `IsAccountBookmarkStorageOptedIn()` should return false.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(
      bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(&sync_service));
}

TEST_P(BookmarkIOSUtilsUnitTest, IsBookmarkedNoMatches) {
  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  if (IsAccountStorageEnabled()) {
    AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                GURL("http://example.com/b"));
  }

  EXPECT_FALSE(bookmark_utils_ios::IsBookmarked(
      GURL("http://example.com/c"), local_or_syncable_bookmark_model_,
      account_bookmark_model_));
}

TEST_P(BookmarkIOSUtilsUnitTest, IsBookmarkedLocalMatch) {
  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  if (IsAccountStorageEnabled()) {
    AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                GURL("http://example.com/b"));
  }

  EXPECT_TRUE(bookmark_utils_ios::IsBookmarked(
      GURL("http://example.com/a"), local_or_syncable_bookmark_model_,
      account_bookmark_model_));
}

TEST_P(BookmarkIOSUtilsUnitTest, IsBookmarkedAccountMatch) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to test matches in that storage";
  }

  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  AddBookmark(account_bookmark_model_->mobile_node(), u"b",
              GURL("http://example.com/b"));

  EXPECT_TRUE(bookmark_utils_ios::IsBookmarked(
      GURL("http://example.com/b"), local_or_syncable_bookmark_model_,
      account_bookmark_model_));
}

TEST_P(BookmarkIOSUtilsUnitTest, IsBookmarkedBothStoragesMatch) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to test matches in both storages";
  }

  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  AddBookmark(account_bookmark_model_->mobile_node(), u"b",
              GURL("http://example.com/a"));

  EXPECT_TRUE(bookmark_utils_ios::IsBookmarked(
      GURL("http://example.com/a"), local_or_syncable_bookmark_model_,
      account_bookmark_model_));
}

TEST_P(BookmarkIOSUtilsUnitTest, GetMostRecentlyAddedNoMatchingBookmarks) {
  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  if (IsAccountStorageEnabled()) {
    AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                GURL("http://example.com/b"));
  }

  const BookmarkNode* result =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          GURL("http://example.com/c"), local_or_syncable_bookmark_model_,
          account_bookmark_model_);
  EXPECT_EQ(result, nullptr);
}

TEST_P(BookmarkIOSUtilsUnitTest, GetMostRecentlyAddedMatchingLocalBookmark) {
  const BookmarkNode* local_bookmark =
      AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
                  GURL("http://example.com/a"));
  if (IsAccountStorageEnabled()) {
    AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                GURL("http://example.com/b"));
  }

  const BookmarkNode* result =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          GURL("http://example.com/a"), local_or_syncable_bookmark_model_,
          account_bookmark_model_);
  EXPECT_EQ(result, local_bookmark);
}

TEST_P(BookmarkIOSUtilsUnitTest, GetMostRecentlyAddedMatchingAccountBookmark) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to test matches in that storage";
  }

  AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
              GURL("http://example.com/a"));
  const BookmarkNode* account_bookmark =
      AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                  GURL("http://example.com/b"));

  const BookmarkNode* result =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          GURL("http://example.com/b"), local_or_syncable_bookmark_model_,
          account_bookmark_model_);
  EXPECT_EQ(result, account_bookmark);
}

TEST_P(BookmarkIOSUtilsUnitTest,
       GetMostRecentlyAddedMatchingBothStoragesLocalWins) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to test matches in both storages";
  }

  const BookmarkNode* local_bookmark =
      AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
                  GURL("http://example.com/a"));
  const BookmarkNode* account_bookmark =
      AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                  GURL("http://example.com/a"));

  base::Time added_time_account_bookmark = base::Time::Now();
  account_bookmark_model_->SetDateAdded(account_bookmark,
                                        added_time_account_bookmark);
  // Simulate local bookmark being added after the account one.
  base::Time added_time_local_bookmark =
      added_time_account_bookmark + base::Seconds(1);
  local_or_syncable_bookmark_model_->SetDateAdded(local_bookmark,
                                                  added_time_local_bookmark);

  const BookmarkNode* result =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          GURL("http://example.com/a"), local_or_syncable_bookmark_model_,
          account_bookmark_model_);
  // Local bookmark is more recent, so it should be returned.
  EXPECT_EQ(result, local_bookmark);
}

TEST_P(BookmarkIOSUtilsUnitTest,
       GetMostRecentlyAddedMatchingBothStoragesAccountWins) {
  if (!IsAccountStorageEnabled()) {
    GTEST_SKIP() << "Need account storage to test matches in both storages";
  }

  const BookmarkNode* local_bookmark =
      AddBookmark(local_or_syncable_bookmark_model_->mobile_node(), u"a",
                  GURL("http://example.com/a"));
  const BookmarkNode* account_bookmark =
      AddBookmark(account_bookmark_model_->mobile_node(), u"b",
                  GURL("http://example.com/a"));

  base::Time added_time_local_bookmark = base::Time::Now();
  local_or_syncable_bookmark_model_->SetDateAdded(local_bookmark,
                                                  added_time_local_bookmark);
  // Simulate account bookmark being added after the local one.
  base::Time added_time_account_bookmark =
      added_time_local_bookmark + base::Seconds(1);
  account_bookmark_model_->SetDateAdded(account_bookmark,
                                        added_time_account_bookmark);

  const BookmarkNode* result =
      bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
          GURL("http://example.com/a"), local_or_syncable_bookmark_model_,
          account_bookmark_model_);
  // Account bookmark is more recent, so it should be returned.
  EXPECT_EQ(result, account_bookmark);
}

INSTANTIATE_TEST_SUITE_P(All, BookmarkIOSUtilsUnitTest, ::testing::Bool());

}  // namespace
