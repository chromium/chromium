// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/commands.h>

#include <map>
#include <memory>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"

namespace content {
namespace {

using AXRole = ax::mojom::Role;
using fuchsia::accessibility::semantics::Role;

constexpr int32_t kRootId = 182;
constexpr int32_t kRowNodeId1 = 2;
constexpr int32_t kRowNodeId2 = 3;
constexpr int32_t kCellNodeId = 7;
constexpr int32_t kListElementId1 = 111;
constexpr int32_t kListElementId2 = 222;

ui::AXTreeUpdate CreateTableUpdate() {
  ui::AXTreeUpdate update;
  update.root_id = kRootId;
  update.nodes.resize(8);
  auto& table = update.nodes[0];
  table.id = kRootId;
  table.role = AXRole::kTable;
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount, 2);
  table.AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount, 2);
  table.child_ids = {888, kRowNodeId2};

  auto& row_group = update.nodes[1];
  row_group.id = 888;
  row_group.role = AXRole::kRowGroup;
  row_group.child_ids = {kRowNodeId1};

  auto& row_1 = update.nodes[2];
  row_1.id = kRowNodeId1;
  row_1.role = AXRole::kRow;
  row_1.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, 0);
  row_1.child_ids = {4, 5};

  auto& row_2 = update.nodes[3];
  row_2.id = kRowNodeId2;
  row_2.role = AXRole::kRow;
  row_2.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex, 1);
  row_2.child_ids = {6, kCellNodeId};

  auto& column_header_1 = update.nodes[4];
  column_header_1.id = 4;
  column_header_1.role = AXRole::kColumnHeader;
  column_header_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                  0);
  column_header_1.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 0);

  auto& column_header_2 = update.nodes[5];
  column_header_2.id = 5;
  column_header_2.role = AXRole::kColumnHeader;
  column_header_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                  0);
  column_header_2.AddIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex, 1);

  auto& cell_1 = update.nodes[6];
  cell_1.id = 6;
  cell_1.role = AXRole::kCell;
  cell_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, 1);
  cell_1.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex, 0);

  auto& cell_2 = update.nodes[7];
  cell_2.id = kCellNodeId;
  cell_2.role = AXRole::kCell;
  cell_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex, 1);
  cell_2.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex, 1);

  return update;
}

ui::AXTreeUpdate CreateListUpdate() {
  ui::AXTreeUpdate update;
  update.root_id = kRootId;
  update.nodes.resize(3);

  auto& list = update.nodes[0];
  list.id = kRootId;
  list.role = AXRole::kList;
  list.AddIntAttribute(ax::mojom::IntAttribute::kSetSize, 2);
  list.child_ids = {kListElementId1, kListElementId2};

  auto& list_element_1 = update.nodes[1];
  list_element_1.id = kListElementId1;
  list_element_1.role = AXRole::kListItem;
  list_element_1.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, 1);

  auto& list_element_2 = update.nodes[2];
  list_element_2.id = kListElementId2;
  list_element_2.role = AXRole::kListItem;
  list_element_2.AddIntAttribute(ax::mojom::IntAttribute::kPosInSet, 2);

  return update;
}

}  // namespace

class BrowserAccessibilityFuchsiaTest : public testing::Test {
 public:
  BrowserAccessibilityFuchsiaTest() = default;
  ~BrowserAccessibilityFuchsiaTest() override = default;

  void SetUp() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  // Disallow copy and assign.
  BrowserAccessibilityFuchsiaTest(const BrowserAccessibilityFuchsiaTest&) =
      delete;
  BrowserAccessibilityFuchsiaTest& operator=(
      const BrowserAccessibilityFuchsiaTest&) = delete;
};

void BrowserAccessibilityFuchsiaTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesRoles) {
  std::map<ax::mojom::Role, fuchsia::accessibility::semantics::Role>
      role_mapping = {
          {AXRole::kButton, Role::BUTTON},
          {AXRole::kCheckBox, Role::CHECK_BOX},
          {AXRole::kHeader, Role::HEADER},
          {AXRole::kImage, Role::IMAGE},
          {AXRole::kLink, Role::LINK},
          {AXRole::kList, Role::LIST},
          {AXRole::kListItem, Role::LIST_ELEMENT},
          {AXRole::kListMarker, Role::LIST_ELEMENT_MARKER},
          {AXRole::kRadioButton, Role::RADIO_BUTTON},
          {AXRole::kSlider, Role::SLIDER},
          {AXRole::kTextField, Role::TEXT_FIELD},
          {AXRole::kSearchBox, Role::SEARCH_BOX},
          {AXRole::kTextFieldWithComboBox, Role::TEXT_FIELD_WITH_COMBO_BOX},
          {AXRole::kTable, Role::TABLE},
          {AXRole::kGrid, Role::GRID},
          {AXRole::kRow, Role::TABLE_ROW},
          {AXRole::kCell, Role::CELL},
          {AXRole::kColumnHeader, Role::COLUMN_HEADER},
          {AXRole::kRowGroup, Role::ROW_GROUP},
          {AXRole::kParagraph, Role::PARAGRAPH}};

  for (const auto& role_pair : role_mapping) {
    ui::AXNodeData node;
    node.id = 1;
    node.role = role_pair.first;

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdateForTesting(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(), role_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesNodeActions) {
  std::map<ax::mojom::Action, fuchsia::accessibility::semantics::Action>
      action_mapping = {
          {ax::mojom::Action::kDoDefault,
           fuchsia::accessibility::semantics::Action::DEFAULT},
          {ax::mojom::Action::kFocus,
           fuchsia::accessibility::semantics::Action::SET_FOCUS},
          {ax::mojom::Action::kSetValue,
           fuchsia::accessibility::semantics::Action::SET_VALUE},
          {ax::mojom::Action::kScrollToMakeVisible,
           fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN}};

  for (const auto& action_pair : action_mapping) {
    ui::AXNodeData node;
    node.id = kRootId;
    node.AddAction(action_pair.first);

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdateForTesting(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    const auto& actions = fuchsia_node_data.actions();
    ASSERT_EQ(actions.size(), 1u);
    EXPECT_EQ(actions[0], action_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesLabels) {
  const std::string kLabel = "label";
  const std::string kSecondaryLabel = "secondary label";

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddStringAttribute(ax::mojom::StringAttribute::kName, kLabel);
  node.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                          kSecondaryLabel);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_attributes());
  ASSERT_TRUE(fuchsia_node_data.attributes().has_label());
  EXPECT_EQ(fuchsia_node_data.attributes().label(), kLabel);
  EXPECT_EQ(fuchsia_node_data.attributes().secondary_label(), kSecondaryLabel);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesRangeAttributes) {
  const float kMin = 1.f;
  const float kMax = 2.f;
  const float kStep = 0.1f;
  const float kValue = 1.5f;

  ui::AXNodeData node;
  node.id = kRootId;
  node.role = ax::mojom::Role::kSlider;
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange, kMin);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange, kMax);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange, kStep);
  node.AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange, kValue);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_attributes());
  ASSERT_TRUE(fuchsia_node_data.attributes().has_range());
  const auto& range_attributes = fuchsia_node_data.attributes().range();
  EXPECT_EQ(range_attributes.min_value(), kMin);
  EXPECT_EQ(range_attributes.max_value(), kMax);
  EXPECT_EQ(range_attributes.step_delta(), kStep);
  EXPECT_TRUE(fuchsia_node_data.has_states());
  EXPECT_TRUE(fuchsia_node_data.states().has_range_value());
  EXPECT_EQ(fuchsia_node_data.states().range_value(), kValue);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesTableAttributes) {
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          CreateTableUpdate(), test_browser_accessibility_delegate_.get()));

  // Verify table node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::TABLE);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_attributes());
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_attributes().number_of_rows(), 2u);
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_attributes().number_of_columns(),
        2u);
  }

  // Verify table row node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(kRowNodeId2));

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::TABLE_ROW);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_row_attributes());
    EXPECT_EQ(fuchsia_node_data.attributes().table_row_attributes().row_index(),
              1u);
  }

  // Verify table cell node translation.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(kCellNodeId));

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::CELL);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_table_cell_attributes());
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_cell_attributes().row_index(), 1u);
    EXPECT_EQ(
        fuchsia_node_data.attributes().table_cell_attributes().column_index(),
        1u);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesListAttributes) {
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          CreateListUpdate(), test_browser_accessibility_delegate_.get()));

  // Verify that the list root was translated.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());
    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();
    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::LIST);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_list_attributes());
    ASSERT_FALSE(fuchsia_node_data.attributes().has_list_element_attributes());
    EXPECT_EQ(fuchsia_node_data.attributes().list_attributes().size(), 2u);
  }

  // Verify that the list elements were translated.
  {
    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(kListElementId2));
    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();
    EXPECT_EQ(fuchsia_node_data.role(),
              fuchsia::accessibility::semantics::Role::LIST_ELEMENT);
    ASSERT_TRUE(fuchsia_node_data.has_attributes());
    ASSERT_FALSE(fuchsia_node_data.attributes().has_list_attributes());
    ASSERT_TRUE(fuchsia_node_data.attributes().has_list_element_attributes());
    EXPECT_EQ(fuchsia_node_data.attributes().list_element_attributes().index(),
              2u);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesCheckedState) {
  std::map<ax::mojom::CheckedState,
           fuchsia::accessibility::semantics::CheckedState>
      state_mapping = {
          {ax::mojom::CheckedState::kNone,
           fuchsia::accessibility::semantics::CheckedState::NONE},
          {ax::mojom::CheckedState::kTrue,
           fuchsia::accessibility::semantics::CheckedState::CHECKED},
          {ax::mojom::CheckedState::kFalse,
           fuchsia::accessibility::semantics::CheckedState::UNCHECKED},
          {ax::mojom::CheckedState::kMixed,
           fuchsia::accessibility::semantics::CheckedState::MIXED}};

  for (const auto state_pair : state_mapping) {
    ui::AXNodeData node;
    node.id = kRootId;
    node.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                         static_cast<int32_t>(state_pair.first));

    std::unique_ptr<BrowserAccessibilityManager> manager(
        BrowserAccessibilityManager::Create(
            MakeAXTreeUpdateForTesting(node),
            test_browser_accessibility_delegate_.get()));

    BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
        ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

    ASSERT_TRUE(browser_accessibility_fuchsia);
    auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

    ASSERT_TRUE(fuchsia_node_data.has_states());
    ASSERT_TRUE(fuchsia_node_data.states().has_checked_state());
    EXPECT_EQ(fuchsia_node_data.states().checked_state(), state_pair.second);
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesSelectedState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  EXPECT_TRUE(fuchsia_node_data.states().has_selected());
  EXPECT_TRUE(fuchsia_node_data.states().selected());
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesInvisibleState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddState(ax::mojom::State::kInvisible);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_hidden());
  EXPECT_TRUE(fuchsia_node_data.states().hidden());
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesIgnoredState) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.AddState(ax::mojom::State::kIgnored);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_hidden());
  EXPECT_TRUE(fuchsia_node_data.states().hidden());
}

TEST_F(BrowserAccessibilityFuchsiaTest, ToFuchsiaNodeDataTranslatesValue) {
  const auto full_value =
      std::string(fuchsia::accessibility::semantics::MAX_LABEL_SIZE + 1, 'a');
  const auto truncated_value =
      std::string(fuchsia::accessibility::semantics::MAX_LABEL_SIZE, 'a');

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddStringAttribute(ax::mojom::StringAttribute::kValue, full_value);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_value());
  EXPECT_EQ(fuchsia_node_data.states().value(), truncated_value);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesScrollOffset) {
  const int32_t kScrollX = 1;
  const int32_t kScrollY = 2;

  ui::AXNodeData node;
  node.id = kRootId;
  node.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, kScrollX);
  node.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, kScrollY);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_states());
  ASSERT_TRUE(fuchsia_node_data.states().has_viewport_offset());
  const auto& viewport_offset = fuchsia_node_data.states().viewport_offset();
  EXPECT_EQ(viewport_offset.x, kScrollX);
  EXPECT_EQ(viewport_offset.y, kScrollY);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesSpatialInfo) {
  const float x_scale = 0.7f;
  const float y_scale = 0.8f;
  const float z_scale = 0.9f;
  const float x_translation = 10.f;
  const float y_translation = 11.f;
  const float z_translation = 12.f;
  const float x_min = 1.f;
  const float y_min = 2.f;
  const float x_max = 3.f;
  const float y_max = 4.f;

  ui::AXNodeData node;
  node.id = kRootId;
  node.relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::RowMajor(
          x_scale, 0, 0, x_translation, 0, y_scale, 0, y_translation, 0, 0,
          z_scale, z_translation, 0, 0, 0, 1));
  node.relative_bounds.bounds = gfx::RectF(
      x_min, y_min, /* width = */ x_max - x_min, /* height = */ y_max - y_min);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  ASSERT_TRUE(fuchsia_node_data.has_node_to_container_transform());
  const auto& transform =
      fuchsia_node_data.node_to_container_transform().matrix;
  EXPECT_EQ(transform[0], x_scale);
  EXPECT_EQ(transform[5], y_scale);
  EXPECT_EQ(transform[10], z_scale);
  EXPECT_EQ(transform[12], x_translation);
  EXPECT_EQ(transform[13], y_translation);
  EXPECT_EQ(transform[14], z_translation);
  ASSERT_TRUE(fuchsia_node_data.has_location());
  const auto& location = fuchsia_node_data.location();
  EXPECT_EQ(location.min.x, x_min);
  EXPECT_EQ(location.min.y, y_min);
  EXPECT_EQ(location.max.x, x_max);
  EXPECT_EQ(location.max.y, y_max);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesOffsetContainerID) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.child_ids = {2};
  ui::AXNodeData node_2;
  node_2.id = 2;
  node_2.child_ids = {3};
  node_2.relative_bounds.offset_container_id = -1;
  ui::AXNodeData node_3;
  node_3.id = 3;
  node_3.relative_bounds.offset_container_id = 2;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node, node_2, node_3),
          test_browser_accessibility_delegate_.get()));

  // Verify that node 2's offset container was translated correctly.
  BrowserAccessibilityFuchsia* root =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(kRootId));
  BrowserAccessibilityFuchsia* child =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
  ASSERT_TRUE(child);
  auto child_node_data = child->ToFuchsiaNodeData();
  ASSERT_TRUE(child_node_data.has_container_id());
  EXPECT_EQ(child_node_data.container_id(), root->GetFuchsiaNodeID());

  // Verify that node 3's offset container was translated correctly.
  BrowserAccessibilityFuchsia* grandchild =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(3));
  ASSERT_TRUE(grandchild);
  auto grandchild_node_data = grandchild->ToFuchsiaNodeData();
  ASSERT_TRUE(grandchild_node_data.has_container_id());
  EXPECT_EQ(grandchild_node_data.container_id(), child->GetFuchsiaNodeID());
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToleratesNonexistentOffsetContainerNodeID) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.child_ids = {2};
  ui::AXNodeData node_2;
  node_2.id = 2;
  node_2.relative_bounds.offset_container_id = 100;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node, node_2),
          test_browser_accessibility_delegate_.get()));

  // Verify that node 2's offset container was translated correctly.
  BrowserAccessibilityFuchsia* child =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
  ASSERT_TRUE(child);
  auto child_node_data = child->ToFuchsiaNodeData();
  ASSERT_TRUE(child_node_data.has_container_id());
  // Offset container ID should default to 0 if the specified node doesn't
  // exist.
  EXPECT_EQ(child_node_data.container_id(), 0u);
}

TEST_F(BrowserAccessibilityFuchsiaTest,
       ToFuchsiaNodeDataTranslatesNodeIDAndChildIDs) {
  ui::AXNodeData node;
  node.id = kRootId;
  node.child_ids = {2, 3};
  ui::AXNodeData node_2;
  node_2.id = 2;
  ui::AXNodeData node_3;
  node_3.id = 3;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node, node_2, node_3),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  BrowserAccessibilityFuchsia* root =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(kRootId));
  BrowserAccessibilityFuchsia* child_1 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
  BrowserAccessibilityFuchsia* child_2 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(3));

  ASSERT_TRUE(child_1);
  ASSERT_TRUE(child_2);
  EXPECT_EQ(fuchsia_node_data.node_id(), root->GetFuchsiaNodeID());
  ASSERT_EQ(fuchsia_node_data.child_ids().size(), 2u);
  EXPECT_EQ(fuchsia_node_data.child_ids()[0], child_1->GetFuchsiaNodeID());
  EXPECT_EQ(fuchsia_node_data.child_ids()[1], child_2->GetFuchsiaNodeID());
}

TEST_F(BrowserAccessibilityFuchsiaTest, ChildTree) {
  // Create a child tree with multiple nodes.
  ui::AXNodeData node;
  node.id = 1;
  node.child_ids = {2, 3};
  ui::AXNodeData node_2;
  node_2.id = 2;
  ui::AXNodeData node_3;
  node_3.id = 3;
  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node, node_2, node_3), nullptr));

  // Create a parent tree that points to the child tree.
  ui::AXNodeData node_4;
  node_4.id = 4;
  node_4.child_ids = {5};
  ui::AXNodeData node_5;
  node_5.id = 5;
  node_5.AddChildTreeId(child_manager->ax_tree_id());
  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node_4, node_5), nullptr));

  // Update the child tree's parent tree ID.
  ui::AXTreeData updated_data = child_manager->GetTreeData();
  updated_data.parent_tree_id = parent_manager->ax_tree_id();
  child_manager->ax_tree()->UpdateDataForTesting(updated_data);

  // Get the parent node that points to the child tree.
  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(parent_manager->GetFromID(5));

  {
    ASSERT_TRUE(browser_accessibility_fuchsia);
    fuchsia::accessibility::semantics::Node fuchsia_node_data =
        browser_accessibility_fuchsia->ToFuchsiaNodeData();

    // Get the root of the child tree to verify that it's present in the parent
    // node's children.
    BrowserAccessibilityFuchsia* child_root = ToBrowserAccessibilityFuchsia(
        child_manager->GetBrowserAccessibilityRoot());

    ASSERT_EQ(fuchsia_node_data.child_ids().size(), 1u);
    EXPECT_EQ(fuchsia_node_data.child_ids()[0], child_root->GetFuchsiaNodeID());
  }

  // Destroy the child tree, and ensure that the parent fuchsia node's child IDs
  // no longer reference it.
  child_manager.reset();

  {
    ASSERT_TRUE(browser_accessibility_fuchsia);
    fuchsia::accessibility::semantics::Node fuchsia_node_data =
        browser_accessibility_fuchsia->ToFuchsiaNodeData();

    EXPECT_TRUE(fuchsia_node_data.child_ids().empty());
  }
}

TEST_F(BrowserAccessibilityFuchsiaTest, ChildTreeMissing) {
  // Create a parent tree that points to a non-existent child tree.
  ui::AXNodeData node_4;
  node_4.id = 4;
  node_4.child_ids = {5};
  ui::AXNodeData node_5;
  node_5.id = 5;
  node_5.AddChildTreeId(ui::AXTreeID::CreateNewAXTreeID());
  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node_4, node_5),
          test_browser_accessibility_delegate_.get()));

  // Get the parent node that points to the child tree.
  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(parent_manager->GetFromID(5));

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  EXPECT_TRUE(fuchsia_node_data.child_ids().empty());
}

TEST_F(BrowserAccessibilityFuchsiaTest, GetFuchsiaNodeIDNonRootTree) {
  // We want to verify that the root of a non-root tree will NOT be assigned ID
  // = 0, so Specify that this tree is not the root.
  test_browser_accessibility_delegate_->is_root_frame_ = false;

  ui::AXNodeData node;
  node.id = kRootId;

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdateForTesting(node),
          test_browser_accessibility_delegate_.get()));

  BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      ToBrowserAccessibilityFuchsia(manager->GetBrowserAccessibilityRoot());

  ASSERT_TRUE(browser_accessibility_fuchsia);
  auto fuchsia_node_data = browser_accessibility_fuchsia->ToFuchsiaNodeData();

  EXPECT_GT(fuchsia_node_data.node_id(), 0u);
}

}  // namespace content
