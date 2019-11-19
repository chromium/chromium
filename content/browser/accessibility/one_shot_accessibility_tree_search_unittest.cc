// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"

#include <memory>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#ifdef OS_ANDROID
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

#ifdef OS_ANDROID
class TestBrowserAccessibilityManager
    : public BrowserAccessibilityManagerAndroid {
 public:
  TestBrowserAccessibilityManager(const ui::AXTreeUpdate& initial_tree)
      : BrowserAccessibilityManagerAndroid(initial_tree, nullptr, nullptr) {}
};
#else
class TestBrowserAccessibilityManager : public BrowserAccessibilityManager {
 public:
  TestBrowserAccessibilityManager(const ui::AXTreeUpdate& initial_tree)
      : BrowserAccessibilityManager(initial_tree,
                                    nullptr,
                                    new BrowserAccessibilityFactory()) {}
};
#endif

}  // namespace

// These tests prevent other tests from being run. crbug.com/514632
#if defined(ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_OneShotAccessibilityTreeSearchTest \
  DISABLED_OneShotAccessibilityTreeSearchTets
#else
#define MAYBE_OneShotAccessibilityTreeSearchTest \
  OneShotAccessibilityTreeSearchTest
#endif
class MAYBE_OneShotAccessibilityTreeSearchTest
    : public testing::TestWithParam<bool> {
 public:
  MAYBE_OneShotAccessibilityTreeSearchTest() {}
  ~MAYBE_OneShotAccessibilityTreeSearchTest() override {}

 protected:
  void SetUp() override;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<BrowserAccessibilityManager> tree_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_OneShotAccessibilityTreeSearchTest);
};

void MAYBE_OneShotAccessibilityTreeSearchTest::SetUp() {
  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);

  ui::AXNodeData heading;
  heading.id = 2;
  heading.SetName("Heading");
  heading.role = ax::mojom::Role::kHeading;
  heading.relative_bounds.bounds = gfx::RectF(0, 0, 800, 50);

  ui::AXNodeData table;
  table.id = 3;
  table.role = ax::mojom::Role::kTable;
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, 1);
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount, 2);

  ui::AXNodeData table_row;
  table_row.id = 4;
  table_row.role = ax::mojom::Role::kRow;
  table_row.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, 0);

  ui::AXNodeData table_column_header_1;
  table_column_header_1.id = 5;
  table_column_header_1.role = ax::mojom::Role::kColumnHeader;
  table_column_header_1.SetName("Cell1");
  table_column_header_1.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellRowIndex, 0);
  table_column_header_1.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 0);

  ui::AXNodeData table_column_header_2;
  table_column_header_2.id = 6;
  table_column_header_2.role = ax::mojom::Role::kColumnHeader;
  table_column_header_2.SetName("Cell2");
  table_column_header_2.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellRowIndex, 0);
  table_column_header_2.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 1);

  ui::AXNodeData list;
  list.id = 7;
  list.role = ax::mojom::Role::kList;
  list.relative_bounds.bounds = gfx::RectF(0, 50, 500, 500);

  ui::AXNodeData list_item_1;
  list_item_1.id = 8;
  list_item_1.SetName("Autobots");
  list_item_1.role = ax::mojom::Role::kListItem;
  list_item_1.relative_bounds.bounds = gfx::RectF(10, 10, 200, 30);

  ui::AXNodeData list_item_2;
  list_item_2.id = 9;
  list_item_2.SetName("Decepticons");
  list_item_2.role = ax::mojom::Role::kListItem;
  list_item_2.relative_bounds.bounds = gfx::RectF(10, 40, 200, 60);

  ui::AXNodeData footer;
  footer.id = 10;
  footer.SetName("Footer");
  footer.role = ax::mojom::Role::kFooter;
  footer.relative_bounds.bounds = gfx::RectF(0, 650, 100, 800);

  table_row.child_ids = {table_column_header_1.id, table_column_header_2.id};
  table.child_ids = {table_row.id};
  list.child_ids = {list_item_1.id, list_item_2.id};
  root.child_ids = {heading.id, table.id, list.id, footer.id};

  tree_.reset(new TestBrowserAccessibilityManager(MakeAXTreeUpdate(
      root, heading, table, table_row, table_column_header_1,
      table_column_header_2, list, list_item_1, list_item_2, footer)));
}

TEST_F(MAYBE_OneShotAccessibilityTreeSearchTest, GetAll) {
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  ASSERT_EQ(10U, search.CountMatches());
}

TEST_F(MAYBE_OneShotAccessibilityTreeSearchTest, BackwardsWrapFromRoot) {
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetDirection(OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetResultLimit(100);
  search.SetCanWrapToLastElement(true);
  ASSERT_EQ(10U, search.CountMatches());
  EXPECT_EQ(1, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(2)->GetId());
  EXPECT_EQ(8, search.GetMatchAtIndex(3)->GetId());
  EXPECT_EQ(7, search.GetMatchAtIndex(4)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(5)->GetId());
  EXPECT_EQ(5, search.GetMatchAtIndex(6)->GetId());
  EXPECT_EQ(4, search.GetMatchAtIndex(7)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(8)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(9)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, NoCycle) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  // If you set a result limit of 1, you won't get the root node back as
  // the first match.
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetResultLimit(1);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_NE(1, search.GetMatchAtIndex(0)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, ForwardsWithStartNode) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetStartNode(tree_->GetFromID(7));
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(2)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, BackwardsWithStartNode) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetStartNode(tree_->GetFromID(4));
  search.SetDirection(OneShotAccessibilityTreeSearch::BACKWARDS);
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(3, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(1, search.GetMatchAtIndex(2)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest,
       BackwardsWithStartNodeForAndroid) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetStartNode(tree_->GetFromID(4));
  search.SetDirection(OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetResultLimit(3);
  search.SetCanWrapToLastElement(true);
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(3, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(1, search.GetMatchAtIndex(2)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest,
       ForwardsWithStartNodeAndScope) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(8));
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(1)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, ResultLimitZero) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetResultLimit(0);
  ASSERT_EQ(0U, search.CountMatches());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, ResultLimitFive) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetResultLimit(5);
  ASSERT_EQ(5U, search.CountMatches());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, DescendantsOnlyOfRoot) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetStartNode(tree_->GetFromID(1));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(4U, search.CountMatches());
  EXPECT_EQ(2, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(7, search.GetMatchAtIndex(2)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(3)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, DescendantsOnlyOfNode) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest,
       DescendantsOnlyOfNodeWithStartNode) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(8));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest,
       DescendantsOnlyOfNodeWithStartNodeBackwardsTableCell) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetFromID(3));
  search.SetStartNode(tree_->GetFromID(5));
  search.SetDirection(OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(0U, search.CountMatches());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest,
       DescendantsOnlyOfNodeWithStartNodeBackwardsListItem) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(9));
  search.SetImmediateDescendantsOnly(true);
  search.SetDirection(OneShotAccessibilityTreeSearch::BACKWARDS);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, OnscreenOnly) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetOnscreenOnly(true);
  ASSERT_EQ(7U, search.CountMatches());
  EXPECT_EQ(1, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(2)->GetId());
  EXPECT_EQ(4, search.GetMatchAtIndex(3)->GetId());
  EXPECT_EQ(7, search.GetMatchAtIndex(4)->GetId());
  EXPECT_EQ(8, search.GetMatchAtIndex(5)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(6)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, CaseInsensitiveStringMatch) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.SetSearchText("eCEptiCOn");
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, OnePredicateTableCell) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return current->GetRole() == ax::mojom::Role::kColumnHeader;
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(5, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(1)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, OnePredicateListItem) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return current->GetRole() == ax::mojom::Role::kListItem;
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, TwoPredicatesTableRowAndCell) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kRow ||
                current->GetRole() == ax::mojom::Role::kColumnHeader);
      });
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return (current->GetId() % 2 == 0);
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(4, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(1)->GetId());
}

TEST_P(MAYBE_OneShotAccessibilityTreeSearchTest, TwoPredicatesListItem) {
  tree_->ax_tree()->SetEnableExtraMacNodes(GetParam());
  OneShotAccessibilityTreeSearch search(tree_->GetRoot());
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kList ||
                current->GetRole() == ax::mojom::Role::kListItem);
      });
  search.AddPredicate(
      [](BrowserAccessibility* start, BrowserAccessibility* current) {
        return (current->GetId() % 2 == 1);
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(7, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
}

INSTANTIATE_TEST_SUITE_P(EnableExtraMacNodes,
                         MAYBE_OneShotAccessibilityTreeSearchTest,
                         testing::Values(true));

INSTANTIATE_TEST_SUITE_P(DisableExtraMacNodes,
                         MAYBE_OneShotAccessibilityTreeSearchTest,
                         testing::Values(false));

}  // namespace content
