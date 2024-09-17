// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"

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
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"

namespace bookmark_utils_ios {
namespace {

using bookmarks::BookmarkNode;

std::vector<std::u16string> GetBookmarkTitles(
    const std::vector<std::unique_ptr<BookmarkNode>>& nodes) {
  std::vector<std::u16string> result;
  base::ranges::transform(nodes, std::back_inserter(result),
                          [](const auto& node) { return node->GetTitle(); });
  return result;
}

class BookmarkIOSUtilsUnitTest : public BookmarkIOSUnitTestSupport {
 protected:
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
    EXPECT_TRUE(MoveBookmarks(to_move, bookmark_model_, f1));

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

TEST_F(BookmarkIOSUtilsUnitTest, CreateOrUpdateNoop) {
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  std::u16string title = u"title";
  const BookmarkNode* node = AddBookmark(mobile_node, title);

  GURL url_copy = node->GetTitledUrlNodeUrl();
  // This call is a no-op, , so `UpdateBookmark` should return `false`.
  EXPECT_FALSE(UpdateBookmark(node, base::SysUTF16ToNSString(title), url_copy,
                              mobile_node, bookmark_model_));
  EXPECT_EQ(node->GetTitle(), title);
}

TEST_F(BookmarkIOSUtilsUnitTest, CreateOrUpdateWithinStorage) {
  const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* node = AddBookmark(mobile_node, u"a");
  const BookmarkNode* folder = AddFolder(mobile_node, u"f1");

  NSString* new_title = @"b";
  GURL new_url("http://example.com");
  EXPECT_TRUE(
      UpdateBookmark(node, new_title, new_url, folder, bookmark_model_));

  ASSERT_THAT(mobile_node->children(),
              testing::ElementsAre(testing::Pointer(folder)));
  ASSERT_THAT(folder->children(), testing::ElementsAre(testing::Pointer(node)));
  EXPECT_EQ(node->GetTitle(), base::SysNSStringToUTF16(new_title));
  EXPECT_EQ(node->GetTitledUrlNodeUrl(), new_url);
}

// TODO(crbug.com/40268591): Add tests that call `UpdateBookmark` with the
// account storage.

TEST_F(BookmarkIOSUtilsUnitTest, CreateOrUpdateBetweenStorages) {
  const BookmarkNode* local_mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* node = AddBookmark(local_mobile_node, u"a");
  const BookmarkNode* account_mobile_node =
      bookmark_model_->account_mobile_node();

  NSString* new_title = @"b";
  GURL new_url("http://example.com");
  EXPECT_TRUE(UpdateBookmark(node, new_title, new_url, account_mobile_node,
                             bookmark_model_));

  EXPECT_THAT(local_mobile_node->children(), testing::IsEmpty());
  ASSERT_THAT(account_mobile_node->children(), testing::SizeIs(1));
  const BookmarkNode* moved_node = account_mobile_node->children()[0].get();
  EXPECT_EQ(moved_node->GetTitle(), base::SysNSStringToUTF16(new_title));
  EXPECT_EQ(moved_node->GetTitledUrlNodeUrl(), new_url);
}

TEST_F(BookmarkIOSUtilsUnitTest, DeleteNodes) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
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

  DeleteBookmarks(toDelete, bookmark_model_, FROM_HERE);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(3u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_F(BookmarkIOSUtilsUnitTest, MoveNodesInLocalOrSyncableModel) {
  const BookmarkNode* local_mobile_node = bookmark_model_->mobile_node();
  ASSERT_NO_FATAL_FAILURE(TestMovingBookmarks(local_mobile_node));
}

TEST_F(BookmarkIOSUtilsUnitTest, MoveAccountNodes) {
  const BookmarkNode* account_mobile_node =
      bookmark_model_->account_mobile_node();
  ASSERT_NO_FATAL_FAILURE(TestMovingBookmarks(account_mobile_node));
}

TEST_F(BookmarkIOSUtilsUnitTest, MoveNodesBetweenStorages) {
  const BookmarkNode* local_mobile_node = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(local_mobile_node, u"f1");
  AddBookmark(local_mobile_node, u"a");
  const BookmarkNode* b = AddBookmark(local_mobile_node, u"b");
  const BookmarkNode* f1a = AddBookmark(f1, u"f1a");
  AddBookmark(f1, u"f1b");

  const BookmarkNode* account_mobile_node =
      bookmark_model_->account_mobile_node();
  const BookmarkNode* c = AddBookmark(account_mobile_node, u"c");
  const BookmarkNode* f2 = AddFolder(account_mobile_node, u"f2");
  const BookmarkNode* f2a = AddBookmark(f2, u"f2a");

  std::vector<const BookmarkNode*> to_move;
  to_move.push_back(f1);   // Cross-storage move.
  to_move.push_back(f1a);  // Cross-storage move, the parent is also moved.
  to_move.push_back(b);    // Cross-storage move, the parent is not moved.
  to_move.push_back(c);    // Same-storage move.

  MoveBookmarks(to_move, bookmark_model_, f2);

  EXPECT_THAT(GetBookmarkTitles(local_mobile_node->children()),
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

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateBookmarkPath) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, u"f1");
  NSArray<NSNumber*>* path = CreateBookmarkPath(bookmark_model_, f1->id());
  NSMutableArray<NSNumber*>* expectedPath = [NSMutableArray array];
  [expectedPath addObject:[NSNumber numberWithLongLong:mobileNode->id()]];
  [expectedPath addObject:[NSNumber numberWithLongLong:f1->id()]];
  EXPECT_NSEQ(expectedPath, path);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateNilBookmarkPath) {
  NSArray<NSNumber*>* path = CreateBookmarkPath(bookmark_model_, 999);
  EXPECT_TRUE(path == nil);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestVisibleNonDescendantNodes) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
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

  AddFolder(bookmark_model_->other_node(), u"buildings");

  std::set<const BookmarkNode*> obstructions;
  // Editing a folder and a bookmark.
  obstructions.insert(gaga);
  obstructions.insert(lindsey);

  NodeVector result = VisibleNonDescendantNodes(
      obstructions, bookmark_model_, BookmarkStorageType::kLocalOrSyncable);
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

TEST_F(BookmarkIOSUtilsUnitTest, TestIsSubvectorOfNodes) {
  // Empty vectors: [] - [].
  NodeVector vector1;
  NodeVector vector2;
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(IsSubvectorOfNodes(vector2, vector1));

  // Empty vs vector with one element: [] - [1].
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, u"1");
  vector2.push_back(bookmark1);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // The same element in each: [1] - [1].
  vector1.push_back(bookmark1);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(IsSubvectorOfNodes(vector2, vector1));

  // One different element in each: [2] - [1].
  vector1.pop_back();
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, u"2");
  vector1.push_back(bookmark2);
  EXPECT_FALSE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [2] - [1, 2].
  vector2.push_back(bookmark2);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [3] - [1, 2].
  vector1.pop_back();
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, u"3");
  vector1.push_back(bookmark3);
  EXPECT_FALSE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [2, 3] - [1, 2, 3].
  vector1.insert(vector1.begin(), bookmark2);
  vector2.push_back(bookmark3);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [2, 3, 1] - [1, 2, 3].
  vector1.push_back(bookmark2);
  EXPECT_FALSE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [1, 3] - [1, 2, 3].
  vector1.clear();
  vector1.push_back(bookmark1);
  vector1.push_back(bookmark2);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [1, 1] - [1, 2, 3].
  vector1.pop_back();
  vector1.push_back(bookmark1);
  EXPECT_FALSE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));

  // [1, 1] - [1, 1, 2, 3].
  vector2.insert(vector2.begin(), bookmark1);
  EXPECT_TRUE(IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(IsSubvectorOfNodes(vector2, vector1));
}

TEST_F(BookmarkIOSUtilsUnitTest, TestMissingNodes) {
  // [] - [].
  NodeVector vector1;
  NodeVector vector2;
  EXPECT_EQ(0u, MissingNodesIndices(vector1, vector2).size());

  // [] - [1].
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, u"1");
  vector2.push_back(bookmark1);
  std::vector<NodeVector::size_type> missingNodesIndices =
      MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [1] - [1].
  vector1.push_back(bookmark1);
  EXPECT_EQ(0u, MissingNodesIndices(vector1, vector2).size());

  // [2] - [1, 2].
  vector1.pop_back();
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, u"2");
  vector1.push_back(bookmark2);
  vector2.push_back(bookmark2);
  missingNodesIndices = MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [2, 3] - [1, 2, 3].
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, u"3");
  vector1.push_back(bookmark3);
  vector2.push_back(bookmark3);
  missingNodesIndices = MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [1, 3] - [1, 2, 3].
  vector1.clear();
  vector1.push_back(bookmark1);
  vector1.push_back(bookmark3);
  missingNodesIndices = MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(1u, missingNodesIndices[0]);

  // [1, 1] - [1, 1, 2, 3].
  vector1.pop_back();
  vector1.push_back(bookmark1);
  vector2.insert(vector2.begin(), bookmark1);
  missingNodesIndices = MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(2u, missingNodesIndices.size());
  EXPECT_EQ(2u, missingNodesIndices[0]);
  EXPECT_EQ(3u, missingNodesIndices[1]);
}

// Tests returned values from `IsAccountBookmarkStorageOptedIn()`.
TEST_F(BookmarkIOSUtilsUnitTest, IsAccountBookmarkStorageOptedIn) {
  syncer::TestSyncService sync_service;

  // If the user is signed out, `IsAccountBookmarkStorageOptedIn()` should
  // always return false.
  EXPECT_FALSE(IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is on, including bookmarks,
  // `IsAccountBookmarkStorageOptedIn()` should always return false.
  sync_service.SetSignedIn(signin::ConsentLevel::kSync);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is on, but bookmarks excluded,
  // `IsAccountBookmarkStorageOptedIn()` should always return false.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is off and the account storage is enabled,
  // `IsAccountBookmarkStorageOptedIn()` should return true.
  sync_service.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_TRUE(IsAccountBookmarkStorageOptedIn(&sync_service));

  // If sync-the-feature is off and the account storage is not enabled,
  // `IsAccountBookmarkStorageOptedIn()` should return false.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IsAccountBookmarkStorageOptedIn(&sync_service));
}

}  // namespace
}  // namespace bookmark_utils_ios
