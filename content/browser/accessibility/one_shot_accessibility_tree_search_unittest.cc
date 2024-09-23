// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"

#include <memory>

#include "build/build_config.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#if BUILDFLAG(IS_ANDROID)
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#elif OS_FUCHSIA
#include "ui/accessibility/platform/fuchsia/browser_accessibility_manager_fuchsia.h"
#endif
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"

namespace content {

namespace {

#if BUILDFLAG(IS_ANDROID)
class TestBrowserAccessibilityManager
    : public BrowserAccessibilityManagerAndroid {
 public:
  explicit TestBrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      ui::AXNodeIdDelegate& node_id_delegate)
      : BrowserAccessibilityManagerAndroid(initial_tree,
                                           nullptr,
                                           node_id_delegate,
                                           nullptr) {}
};
#elif OS_FUCHSIA
class TestBrowserAccessibilityManager
    : public ui::BrowserAccessibilityManagerFuchsia {
 public:
  explicit TestBrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      ui::AXNodeIdDelegate& node_id_delegate)
      : BrowserAccessibilityManagerFuchsia(initial_tree,
                                           node_id_delegate,
                                           nullptr) {}
};
#else
class TestBrowserAccessibilityManager : public ui::BrowserAccessibilityManager {
 public:
  explicit TestBrowserAccessibilityManager(
      const ui::AXTreeUpdate& initial_tree,
      ui::AXNodeIdDelegate& node_id_delegate)
      : BrowserAccessibilityManager(node_id_delegate, nullptr) {
    Initialize(initial_tree);
  }
};
#endif

}  // namespace

class OneShotAccessibilityTreeSearchTest : public testing::Test {
 public:
  OneShotAccessibilityTreeSearchTest() = default;

  OneShotAccessibilityTreeSearchTest(
      const OneShotAccessibilityTreeSearchTest&) = delete;
  OneShotAccessibilityTreeSearchTest& operator=(
      const OneShotAccessibilityTreeSearchTest&) = delete;

  ~OneShotAccessibilityTreeSearchTest() override = default;

 protected:
  void SetUp() override;

  BrowserTaskEnvironment task_environment_;

  ui::TestAXNodeIdDelegate node_id_delegate_;
  std::unique_ptr<ui::BrowserAccessibilityManager> tree_;
};

void OneShotAccessibilityTreeSearchTest::SetUp() {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.SetName("Document");
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);

  ui::AXNodeData heading;
  heading.id = 2;
  heading.role = ax::mojom::Role::kHeading;
  heading.SetName("Heading");
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
  list_item_1.role = ax::mojom::Role::kListItem;
  list_item_1.SetName("Autobots");
  list_item_1.relative_bounds.bounds = gfx::RectF(10, 10, 200, 30);

  ui::AXNodeData list_item_2;
  list_item_2.id = 9;
  list_item_2.role = ax::mojom::Role::kListItem;
  list_item_2.SetName("Decepticons");
  list_item_2.relative_bounds.bounds = gfx::RectF(10, 40, 200, 60);

  ui::AXNodeData footer;
  footer.id = 10;
  footer.role = ax::mojom::Role::kFooter;
  footer.SetName("Footer");
  footer.relative_bounds.bounds = gfx::RectF(0, 650, 100, 800);

  table_row.child_ids = {table_column_header_1.id, table_column_header_2.id};
  table.child_ids = {table_row.id};
  list.child_ids = {list_item_1.id, list_item_2.id};
  root.child_ids = {heading.id, table.id, list.id, footer.id};

  tree_ = std::make_unique<TestBrowserAccessibilityManager>(
      MakeAXTreeUpdateForTesting(root, heading, table, table_row,
                                 table_column_header_1, table_column_header_2,
                                 list, list_item_1, list_item_2, footer),
      node_id_delegate_);
}

TEST_F(OneShotAccessibilityTreeSearchTest, GetAll) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
#if BUILDFLAG(IS_MAC)
  ASSERT_EQ(13U, search.CountMatches());
#else
  ASSERT_EQ(10U, search.CountMatches());
#endif
}

TEST_F(OneShotAccessibilityTreeSearchTest, BackwardsWrapFromRoot) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetDirection(ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetResultLimit(100);
  search.SetCanWrapToLastElement(true);
#if BUILDFLAG(IS_MAC)
  ASSERT_EQ(13U, search.CountMatches());
#else
  ASSERT_EQ(10U, search.CountMatches());
#endif
  EXPECT_EQ(1, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(2)->GetId());
  EXPECT_EQ(8, search.GetMatchAtIndex(3)->GetId());
  EXPECT_EQ(7, search.GetMatchAtIndex(4)->GetId());
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(-3, search.GetMatchAtIndex(5)->GetId());
  EXPECT_EQ(-2, search.GetMatchAtIndex(6)->GetId());
  EXPECT_EQ(-1, search.GetMatchAtIndex(7)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(8)->GetId());
  EXPECT_EQ(5, search.GetMatchAtIndex(9)->GetId());
  EXPECT_EQ(4, search.GetMatchAtIndex(10)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(11)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(12)->GetId());
#else
  EXPECT_EQ(6, search.GetMatchAtIndex(5)->GetId());
  EXPECT_EQ(5, search.GetMatchAtIndex(6)->GetId());
  EXPECT_EQ(4, search.GetMatchAtIndex(7)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(8)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(9)->GetId());
#endif
}

TEST_F(OneShotAccessibilityTreeSearchTest, NoCycle) {
  // If you set a result limit of 1, you won't get the root node back as
  // the first match.
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetResultLimit(1);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_NE(1, search.GetMatchAtIndex(0)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, ForwardsWithStartNode) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetStartNode(tree_->GetFromID(7));
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(2)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, BackwardsWithStartNode) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetStartNode(tree_->GetFromID(4));
  search.SetDirection(ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(3, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(1, search.GetMatchAtIndex(2)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, BackwardsWithStartNodeForAndroid) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetStartNode(tree_->GetFromID(4));
  search.SetDirection(ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetResultLimit(3);
  search.SetCanWrapToLastElement(true);
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(3, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(2, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(1, search.GetMatchAtIndex(2)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, ForwardsWithStartNodeAndScope) {
  ui::OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(8));
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(1)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, ResultLimitZero) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetResultLimit(0);
  ASSERT_EQ(0U, search.CountMatches());
}

TEST_F(OneShotAccessibilityTreeSearchTest, ResultLimitFive) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetResultLimit(5);
  ASSERT_EQ(5U, search.CountMatches());
}

TEST_F(OneShotAccessibilityTreeSearchTest, DescendantsOnlyOfRoot) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetStartNode(tree_->GetFromID(1));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(4U, search.CountMatches());
  EXPECT_EQ(2, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(3, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(7, search.GetMatchAtIndex(2)->GetId());
  EXPECT_EQ(10, search.GetMatchAtIndex(3)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, DescendantsOnlyOfNode) {
  ui::OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, DescendantsOnlyOfNodeWithStartNode) {
  ui::OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(8));
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest,
       DescendantsOnlyOfNodeWithStartNodeBackwardsTableCell) {
  ui::OneShotAccessibilityTreeSearch search(tree_->GetFromID(3));
  search.SetStartNode(tree_->GetFromID(5));
  search.SetDirection(ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetImmediateDescendantsOnly(true);
  ASSERT_EQ(0U, search.CountMatches());
}

TEST_F(OneShotAccessibilityTreeSearchTest,
       DescendantsOnlyOfNodeWithStartNodeBackwardsListItem) {
  ui::OneShotAccessibilityTreeSearch search(tree_->GetFromID(7));
  search.SetStartNode(tree_->GetFromID(9));
  search.SetImmediateDescendantsOnly(true);
  search.SetDirection(ui::OneShotAccessibilityTreeSearch::BACKWARDS);
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, OnscreenOnly) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
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

TEST_F(OneShotAccessibilityTreeSearchTest, CaseInsensitiveStringMatch) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.SetSearchText("eCEptiCOn");
  ASSERT_EQ(1U, search.CountMatches());
  EXPECT_EQ(9, search.GetMatchAtIndex(0)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, OnePredicateTableCell) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return current->GetRole() == ax::mojom::Role::kColumnHeader;
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(5, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(1)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, OnePredicateListItem) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return current->GetRole() == ax::mojom::Role::kListItem;
      });
  ASSERT_EQ(2U, search.CountMatches());
  EXPECT_EQ(8, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(1)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, TwoPredicatesTableRowAndCell) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kRow);
      });
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kColumnHeader);
      });
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(4, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(5, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(6, search.GetMatchAtIndex(2)->GetId());
}

TEST_F(OneShotAccessibilityTreeSearchTest, TwoPredicatesListItem) {
  ui::OneShotAccessibilityTreeSearch search(
      tree_->GetBrowserAccessibilityRoot());
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kList);
      });
  search.AddPredicate(
      [](ui::BrowserAccessibility* start, ui::BrowserAccessibility* current) {
        return (current->GetRole() == ax::mojom::Role::kListItem);
      });
  ASSERT_EQ(3U, search.CountMatches());
  EXPECT_EQ(7, search.GetMatchAtIndex(0)->GetId());
  EXPECT_EQ(8, search.GetMatchAtIndex(1)->GetId());
  EXPECT_EQ(9, search.GetMatchAtIndex(2)->GetId());
}

}  // namespace content
