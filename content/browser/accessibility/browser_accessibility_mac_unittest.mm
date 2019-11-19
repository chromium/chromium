// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/accessibility/ax_tree_update.h"
#import "ui/base/test/cocoa_helper.h"

namespace content {

namespace {

void MakeTable(ui::AXNodeData* table, int id, int row_count, int col_count) {
  table->id = id;
  table->role = ax::mojom::Role::kTable;
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, row_count);
  table->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount, col_count);
}

void MakeRow(ui::AXNodeData* row, int id) {
  row->id = id;
  row->role = ax::mojom::Role::kRow;
}

void MakeCell(ui::AXNodeData* cell,
              int id,
              int row_index,
              int col_index,
              int row_span = 1,
              int col_span = 1) {
  cell->id = id;
  cell->role = ax::mojom::Role::kCell;
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, row_index);
  cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                        col_index);
  if (row_span > 1)
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, row_span);
  if (col_span > 1)
    cell->AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                          col_span);
}

void MakeColumnHeader(ui::AXNodeData* cell,
                      int id,
                      int row_index,
                      int col_index,
                      int row_span = 1,
                      int col_span = 1) {
  MakeCell(cell, id, row_index, col_index, row_span, col_span);
  cell->role = ax::mojom::Role::kColumnHeader;
}

}  // namespace

class BrowserAccessibilityMacTest : public ui::CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();
    RebuildAccessibilityTree();
  }

 protected:
  void RebuildAccessibilityTree() {
    // Clean out the existing root data in case this method is called multiple
    // times in a test.
    root_ = ui::AXNodeData();
    root_.id = 1000;
    root_.relative_bounds.bounds.set_width(500);
    root_.relative_bounds.bounds.set_height(100);
    root_.role = ax::mojom::Role::kRootWebArea;
    root_.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                             "HelpText");
    root_.child_ids.push_back(1001);
    root_.child_ids.push_back(1002);

    ui::AXNodeData child1;
    child1.id = 1001;
    child1.SetName("Child1");
    child1.relative_bounds.bounds.set_width(250);
    child1.relative_bounds.bounds.set_height(100);
    child1.role = ax::mojom::Role::kButton;

    ui::AXNodeData child2;
    child2.id = 1002;
    child2.relative_bounds.bounds.set_x(250);
    child2.relative_bounds.bounds.set_width(250);
    child2.relative_bounds.bounds.set_height(100);
    child2.role = ax::mojom::Role::kHeading;

    manager_.reset(new BrowserAccessibilityManagerMac(
        MakeAXTreeUpdate(root_, child1, child2), nullptr));
    accessibility_.reset(
        [ToBrowserAccessibilityCocoa(manager_->GetRoot()) retain]);
  }

  void SetRootValue(std::string value) {
    if (!manager_)
      return;
    root_.SetValue(value);
    AXEventNotificationDetails event_bundle;
    event_bundle.updates.resize(1);
    event_bundle.updates[0].nodes.push_back(root_);
    ASSERT_TRUE(manager_->OnAccessibilityEvents(event_bundle));
  }

  ui::AXNodeData root_;
  base::scoped_nsobject<BrowserAccessibilityCocoa> accessibility_;
  std::unique_ptr<BrowserAccessibilityManager> manager_;
};

// Standard hit test.
TEST_F(BrowserAccessibilityMacTest, HitTestTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1", firstChild.descriptionForAccessibility);
}

// Test doing a hit test on the edge of a child.
TEST_F(BrowserAccessibilityMacTest, EdgeHitTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSZeroPoint];
  EXPECT_NSEQ(@"Child1", firstChild.descriptionForAccessibility);
}

// This will test a hit test with invalid coordinates.  It is assumed that
// the hit test has been narrowed down to this object or one of its children
// so it should return itself since it has no better hit result.
TEST_F(BrowserAccessibilityMacTest, InvalidHitTestCoordsTest) {
  BrowserAccessibilityCocoa* hitTestResult =
      [accessibility_ accessibilityHitTest:NSMakePoint(-50, 50)];
  EXPECT_NSEQ(accessibility_, hitTestResult);
}

// Test to ensure querying standard attributes works.
TEST_F(BrowserAccessibilityMacTest, BasicAttributeTest) {
  EXPECT_NSEQ(@"HelpText", [accessibility_ accessibilityHelp]);
}

TEST_F(BrowserAccessibilityMacTest, RetainedDetachedObjectsReturnNil) {
  // Get the first child.
  BrowserAccessibilityCocoa* retainedFirstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1", retainedFirstChild.descriptionForAccessibility);

  // Retain it. This simulates what the system might do with an
  // accessibility object.
  [retainedFirstChild retain];

  // Rebuild the accessibility tree, which should detach |retainedFirstChild|.
  RebuildAccessibilityTree();

  // Now any attributes we query should return nil.
  EXPECT_NSEQ(nil, retainedFirstChild.descriptionForAccessibility);

  // Don't leak memory in the test.
  [retainedFirstChild release];
}

TEST_F(BrowserAccessibilityMacTest, TestComputeTextEdit) {
  BrowserAccessibility* owner = [accessibility_ owner];
  ASSERT_NE(nullptr, owner);

  // Insertion but no deletion.

  SetRootValue("text");
  AXTextEdit text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("text"), text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("new text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("new "), text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16(" hello"), text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  SetRootValue("newer text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("er"), text_edit.inserted_text);
  EXPECT_TRUE(text_edit.deleted_text.empty());

  // Deletion but no insertion.

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("er"), text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("new text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16(" hello"), text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("text");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("new "), text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  SetRootValue("");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("text"), text_edit.deleted_text);
  EXPECT_TRUE(text_edit.inserted_text.empty());

  // Both insertion and deletion.

  SetRootValue("new text hello");
  text_edit = [accessibility_ computeTextEdit];
  SetRootValue("new word hello");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("text"), text_edit.deleted_text);
  EXPECT_EQ(base::UTF8ToUTF16("word"), text_edit.inserted_text);

  SetRootValue("new word there");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("hello"), text_edit.deleted_text);
  EXPECT_EQ(base::UTF8ToUTF16("there"), text_edit.inserted_text);

  SetRootValue("old word there");
  text_edit = [accessibility_ computeTextEdit];
  EXPECT_EQ(base::UTF8ToUTF16("new"), text_edit.deleted_text);
  EXPECT_EQ(base::UTF8ToUTF16("old"), text_edit.inserted_text);
}

// Test Mac-specific table APIs.
TEST_F(BrowserAccessibilityMacTest, TableAPIs) {
  ui::AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(7);
  MakeTable(&initial_state.nodes[0], 1, 0, 0);
  initial_state.nodes[0].child_ids = {2, 3};
  MakeRow(&initial_state.nodes[1], 2);
  initial_state.nodes[1].child_ids = {4, 5};
  MakeRow(&initial_state.nodes[2], 3);
  initial_state.nodes[2].child_ids = {6, 7};
  MakeColumnHeader(&initial_state.nodes[3], 4, 0, 0);
  MakeColumnHeader(&initial_state.nodes[4], 5, 0, 1);
  MakeCell(&initial_state.nodes[5], 6, 1, 0);
  MakeCell(&initial_state.nodes[6], 7, 1, 1);

  manager_.reset(new BrowserAccessibilityManagerMac(initial_state, nullptr));
  base::scoped_nsobject<BrowserAccessibilityCocoa> ax_table_(
      [ToBrowserAccessibilityCocoa(manager_->GetRoot()) retain]);
  id children = [ax_table_ children];
  EXPECT_EQ(5U, [children count]);

  EXPECT_NSEQ(@"AXRow", [children[0] role]);
  EXPECT_EQ(2U, [[children[0] children] count]);

  EXPECT_NSEQ(@"AXRow", [children[1] role]);
  EXPECT_EQ(2U, [[children[1] children] count]);

  EXPECT_NSEQ(@"AXColumn", [children[2] role]);
  EXPECT_EQ(2U, [[children[2] children] count]);
  id col_children = [children[2] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);

  EXPECT_NSEQ(@"AXColumn", [children[3] role]);
  EXPECT_EQ(2U, [[children[3] children] count]);
  col_children = [children[3] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);

  EXPECT_NSEQ(@"AXGroup", [children[4] role]);
  EXPECT_EQ(2U, [[children[4] children] count]);
  col_children = [children[4] children];
  EXPECT_NSEQ(@"AXCell", [col_children[0] role]);
  EXPECT_NSEQ(@"AXCell", [col_children[1] role]);
}

}  // namespace content
