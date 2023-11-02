// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#import <memory>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {

class BookmarkIOSUtilsUnitTest : public BookmarkIOSUnitTest {
 protected:
  base::Time timeFromEpoch(int days, int hours) {
    return base::Time::UnixEpoch() + base::Days(days) + base::Hours(hours);
  }
};

TEST_F(BookmarkIOSUtilsUnitTest, DeleteNodes) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, @"a");
  const BookmarkNode* b = AddBookmark(mobileNode, @"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, @"f2");

  AddBookmark(f1, @"f1a");
  AddBookmark(f1, @"f1b");
  AddBookmark(f1, @"f1c");
  AddBookmark(f2, @"f2a");
  const BookmarkNode* f2b = AddBookmark(f2, @"f2b");

  std::set<const BookmarkNode*> toDelete;
  toDelete.insert(a);
  toDelete.insert(f2b);
  toDelete.insert(f2);

  bookmark_utils_ios::DeleteBookmarks(toDelete, bookmark_model_);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(3u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_F(BookmarkIOSUtilsUnitTest, MoveNodes) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, @"a");
  const BookmarkNode* b = AddBookmark(mobileNode, @"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, @"f2");

  AddBookmark(f1, @"f1a");
  AddBookmark(f1, @"f1b");
  AddBookmark(f1, @"f1c");
  AddBookmark(f2, @"f2a");
  const BookmarkNode* f2b = AddBookmark(f2, @"f2b");

  std::set<const BookmarkNode*> toMove;
  toMove.insert(a);
  toMove.insert(f2b);
  toMove.insert(f2);

  bookmark_utils_ios::MoveBookmarks(toMove, bookmark_model_, f1);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(6u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateBookmarkPath) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  NSArray* path =
      bookmark_utils_ios::CreateBookmarkPath(bookmark_model_, f1->id());
  NSMutableArray* expectedPath = [NSMutableArray array];
  [expectedPath addObject:@0];
  [expectedPath addObject:[NSNumber numberWithLongLong:mobileNode->id()]];
  [expectedPath addObject:[NSNumber numberWithLongLong:f1->id()]];
  EXPECT_TRUE([expectedPath isEqualToArray:path]);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateNilBookmarkPath) {
  NSArray* path = bookmark_utils_ios::CreateBookmarkPath(bookmark_model_, 999);
  EXPECT_TRUE(path == nil);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestVisibleNonDescendantNodes) {
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* music = AddFolder(mobileNode, @"music");

  const BookmarkNode* pop = AddFolder(music, @"pop");
  const BookmarkNode* lindsey = AddBookmark(pop, @"lindsey lohan");
  AddBookmark(pop, @"katy perry");
  const BookmarkNode* gaga = AddFolder(pop, @"lady gaga");
  AddBookmark(gaga, @"gaga song 1");
  AddFolder(gaga, @"gaga folder 1");

  const BookmarkNode* metal = AddFolder(music, @"metal");
  AddFolder(metal, @"opeth");
  AddFolder(metal, @"F12");
  AddFolder(metal, @"f31");

  const BookmarkNode* animals = AddFolder(mobileNode, @"animals");
  AddFolder(animals, @"cat");
  const BookmarkNode* camel = AddFolder(animals, @"camel");
  AddFolder(camel, @"al paca");

  AddFolder(bookmark_model_->other_node(), @"buildings");

  std::set<const BookmarkNode*> obstructions;
  // Editing a folder and a bookmark.
  obstructions.insert(gaga);
  obstructions.insert(lindsey);

  bookmark_utils_ios::NodeVector result =
      bookmark_utils_ios::VisibleNonDescendantNodes(obstructions,
                                                    bookmark_model_);
  ASSERT_EQ(13u, result.size());

  EXPECT_NSEQ(base::SysUTF16ToNSString(result[0]->GetTitle()),
              @"Mobile Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[1]->GetTitle()), @"animals");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[2]->GetTitle()), @"camel");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[3]->GetTitle()), @"al paca");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[4]->GetTitle()), @"cat");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[5]->GetTitle()), @"music");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[6]->GetTitle()), @"metal");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[7]->GetTitle()), @"F12");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[8]->GetTitle()), @"f31");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[9]->GetTitle()), @"opeth");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[10]->GetTitle()), @"pop");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[11]->GetTitle()),
              @"Other Bookmarks");
  EXPECT_NSEQ(base::SysUTF16ToNSString(result[12]->GetTitle()), @"buildings");
}

TEST_F(BookmarkIOSUtilsUnitTest, TestIsSubvectorOfNodes) {
  // Empty vectors: [] - [].
  bookmark_utils_ios::NodeVector vector1;
  bookmark_utils_ios::NodeVector vector2;
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // Empty vs vector with one element: [] - [1].
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, @"1");
  vector2.push_back(bookmark1);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // The same element in each: [1] - [1].
  vector1.push_back(bookmark1);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // One different element in each: [2] - [1].
  vector1.pop_back();
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, @"2");
  vector1.push_back(bookmark2);
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [2] - [1, 2].
  vector2.push_back(bookmark2);
  EXPECT_TRUE(bookmark_utils_ios::IsSubvectorOfNodes(vector1, vector2));
  EXPECT_FALSE(bookmark_utils_ios::IsSubvectorOfNodes(vector2, vector1));

  // [3] - [1, 2].
  vector1.pop_back();
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, @"3");
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

TEST_F(BookmarkIOSUtilsUnitTest, TestMissingNodes) {
  // [] - [].
  bookmark_utils_ios::NodeVector vector1;
  bookmark_utils_ios::NodeVector vector2;
  EXPECT_EQ(0u,
            bookmark_utils_ios::MissingNodesIndices(vector1, vector2).size());

  // [] - [1].
  const BookmarkNode* mobileNode = bookmark_model_->mobile_node();
  const BookmarkNode* bookmark1 = AddBookmark(mobileNode, @"1");
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
  const BookmarkNode* bookmark2 = AddBookmark(mobileNode, @"2");
  vector1.push_back(bookmark2);
  vector2.push_back(bookmark2);
  missingNodesIndices =
      bookmark_utils_ios::MissingNodesIndices(vector1, vector2);
  EXPECT_EQ(1u, missingNodesIndices.size());
  EXPECT_EQ(0u, missingNodesIndices[0]);

  // [2, 3] - [1, 2, 3].
  const BookmarkNode* bookmark3 = AddBookmark(mobileNode, @"3");
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

}  // namespace
