// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#import <memory>
#import <string>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {

class BookmarkIOSUtilsUnitTest : public BookmarkIOSUnitTestSupport,
                                 public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        bookmarks::kEnableBookmarksAccountStorage, IsAccountStorageEnabled());
    BookmarkIOSUnitTestSupport::SetUp();
  }

  bool IsAccountStorageEnabled() const { return GetParam(); }

  base::Time timeFromEpoch(int days, int hours) {
    return base::Time::UnixEpoch() + base::Days(days) + base::Hours(hours);
  }
};

TEST_P(BookmarkIOSUtilsUnitTest, DeleteNodes) {
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
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

  bookmark_utils_ios::DeleteBookmarks(toDelete, profile_bookmark_model_);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(3u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_P(BookmarkIOSUtilsUnitTest, MoveNodes) {
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, u"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, u"a");
  const BookmarkNode* b = AddBookmark(mobileNode, u"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, u"f2");

  AddBookmark(f1, u"f1a");
  AddBookmark(f1, u"f1b");
  AddBookmark(f1, u"f1c");
  AddBookmark(f2, u"f2a");
  const BookmarkNode* f2b = AddBookmark(f2, u"f2b");

  std::set<const BookmarkNode*> toMove;
  toMove.insert(a);
  toMove.insert(f2b);
  toMove.insert(f2);

  bookmark_utils_ios::MoveBookmarks(toMove, profile_bookmark_model_, f1);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(6u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_P(BookmarkIOSUtilsUnitTest, TestCreateBookmarkPath) {
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, u"f1");
  NSArray<NSNumber*>* path =
      bookmark_utils_ios::CreateBookmarkPath(profile_bookmark_model_, f1->id());
  NSMutableArray<NSNumber*>* expectedPath = [NSMutableArray array];
  [expectedPath addObject:@0];
  [expectedPath addObject:[NSNumber numberWithLongLong:mobileNode->id()]];
  [expectedPath addObject:[NSNumber numberWithLongLong:f1->id()]];
  EXPECT_TRUE([expectedPath isEqualToArray:path]);
}

TEST_P(BookmarkIOSUtilsUnitTest, TestCreateNilBookmarkPath) {
  NSArray<NSNumber*>* path =
      bookmark_utils_ios::CreateBookmarkPath(profile_bookmark_model_, 999);
  EXPECT_TRUE(path == nil);
}

TEST_P(BookmarkIOSUtilsUnitTest, TestVisibleNonDescendantNodes) {
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
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

  AddFolder(profile_bookmark_model_->other_node(), u"buildings");

  std::set<const BookmarkNode*> obstructions;
  // Editing a folder and a bookmark.
  obstructions.insert(gaga);
  obstructions.insert(lindsey);

  bookmark_utils_ios::NodeVector result =
      bookmark_utils_ios::VisibleNonDescendantNodes(obstructions,
                                                    profile_bookmark_model_);
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
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
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
  const BookmarkNode* mobileNode = profile_bookmark_model_->mobile_node();
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

INSTANTIATE_TEST_SUITE_P(All, BookmarkIOSUtilsUnitTest, ::testing::Bool());

}  // namespace
