// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

#include <memory>
#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

namespace {

using bookmark_utils_ios::NodesSection;

class BookmarkIOSUtilsUnitTest : public BookmarkIOSUnitTest {
 protected:
  base::Time timeFromEpoch(int days, int hours) {
    return base::Time::UnixEpoch() + base::TimeDelta::FromDays(days) +
           base::TimeDelta::FromHours(hours);
  }
};

TEST_F(BookmarkIOSUtilsUnitTest, segregateNodesByCreationDate) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, @"a");
  _bookmarkModel->SetDateAdded(a, timeFromEpoch(169, 5));
  const BookmarkNode* b = AddBookmark(mobileNode, @"b");
  _bookmarkModel->SetDateAdded(b, timeFromEpoch(170, 6));
  const BookmarkNode* f2 = AddFolder(mobileNode, @"f2");

  const BookmarkNode* f1a = AddBookmark(f1, @"f1a");
  _bookmarkModel->SetDateAdded(f1a, timeFromEpoch(129, 5));
  const BookmarkNode* f1b = AddBookmark(f1, @"f1b");
  _bookmarkModel->SetDateAdded(f1b, timeFromEpoch(130, 6));
  const BookmarkNode* f2a = AddBookmark(f2, @"f2a");
  _bookmarkModel->SetDateAdded(f2a, timeFromEpoch(201, 5));
  const BookmarkNode* f2b = AddBookmark(f2, @"f2b");
  _bookmarkModel->SetDateAdded(f2b, timeFromEpoch(10, 5));

  std::vector<const BookmarkNode*> toSort;
  toSort.push_back(a);
  toSort.push_back(b);
  toSort.push_back(f1a);
  toSort.push_back(f1b);
  toSort.push_back(f2a);
  toSort.push_back(f2b);

  std::vector<std::unique_ptr<NodesSection>> nodesSectionVector;
  bookmark_utils_ios::segregateNodes(toSort, nodesSectionVector);

  // Expect the nodes to be sorted in reverse chronological order, grouped by
  // month.
  ASSERT_EQ(nodesSectionVector.size(), 4u);
  NodesSection* section = nodesSectionVector[0].get();
  ASSERT_EQ(section->vector.size(), 1u);
  EXPECT_EQ(section->vector[0], f2a);

  section = nodesSectionVector[1].get();
  ASSERT_EQ(section->vector.size(), 2u);
  EXPECT_EQ(section->vector[0], b);
  EXPECT_EQ(section->vector[1], a);

  section = nodesSectionVector[2].get();
  ASSERT_EQ(section->vector.size(), 2u);
  EXPECT_EQ(section->vector[0], f1b);
  EXPECT_EQ(section->vector[1], f1a);

  section = nodesSectionVector[3].get();
  ASSERT_EQ(section->vector.size(), 1u);
  EXPECT_EQ(section->vector[0], f2b);
}

TEST_F(BookmarkIOSUtilsUnitTest, DeleteNodes) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
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

  bookmark_utils_ios::DeleteBookmarks(toDelete, _bookmarkModel);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(3u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_F(BookmarkIOSUtilsUnitTest, MoveNodes) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
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

  bookmark_utils_ios::MoveBookmarks(toMove, _bookmarkModel, f1);

  EXPECT_EQ(2u, mobileNode->children().size());
  const BookmarkNode* child0 = mobileNode->children()[0].get();
  EXPECT_EQ(child0, f1);
  EXPECT_EQ(6u, child0->children().size());
  const BookmarkNode* child1 = mobileNode->children()[1].get();
  EXPECT_EQ(child1, b);
  EXPECT_EQ(0u, child1->children().size());
}

TEST_F(BookmarkIOSUtilsUnitTest, TestDefaultMoveFolder) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  const BookmarkNode* a = AddBookmark(mobileNode, @"a");
  AddBookmark(mobileNode, @"b");
  const BookmarkNode* f2 = AddFolder(mobileNode, @"f2");

  AddBookmark(f1, @"f1a");
  AddBookmark(f1, @"f1b");
  AddBookmark(f1, @"f1c");
  const BookmarkNode* f2a = AddBookmark(f2, @"f2a");
  const BookmarkNode* f2b = AddBookmark(f2, @"f2b");

  std::set<const BookmarkNode*> toMove;
  toMove.insert(a);
  toMove.insert(f2b);
  toMove.insert(f2);

  const BookmarkNode* folder =
      bookmark_utils_ios::defaultMoveFolder(toMove, _bookmarkModel);
  EXPECT_EQ(folder, mobileNode);

  toMove.clear();
  toMove.insert(f2a);
  toMove.insert(f2b);
  folder = bookmark_utils_ios::defaultMoveFolder(toMove, _bookmarkModel);
  EXPECT_EQ(folder, f2);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateBookmarkPath) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
  const BookmarkNode* f1 = AddFolder(mobileNode, @"f1");
  NSArray* path =
      bookmark_utils_ios::CreateBookmarkPath(_bookmarkModel, f1->id());
  NSMutableArray* expectedPath = [NSMutableArray array];
  [expectedPath addObject:@0];
  [expectedPath addObject:[NSNumber numberWithLongLong:mobileNode->id()]];
  [expectedPath addObject:[NSNumber numberWithLongLong:f1->id()]];
  EXPECT_TRUE([expectedPath isEqualToArray:path]);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestCreateNilBookmarkPath) {
  NSArray* path = bookmark_utils_ios::CreateBookmarkPath(_bookmarkModel, 999);
  EXPECT_TRUE(path == nil);
}

TEST_F(BookmarkIOSUtilsUnitTest, TestVisibleNonDescendantNodes) {
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
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

  AddFolder(_bookmarkModel->other_node(), @"buildings");

  std::set<const BookmarkNode*> obstructions;
  // Editing a folder and a bookmark.
  obstructions.insert(gaga);
  obstructions.insert(lindsey);

  bookmark_utils_ios::NodeVector result =
      bookmark_utils_ios::VisibleNonDescendantNodes(obstructions,
                                                    _bookmarkModel);
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
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
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
  const BookmarkNode* mobileNode = _bookmarkModel->mobile_node();
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
