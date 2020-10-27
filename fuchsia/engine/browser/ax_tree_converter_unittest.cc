// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/ax_tree_converter.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

using fuchsia::accessibility::semantics::Action;
using fuchsia::accessibility::semantics::Attributes;
using fuchsia::accessibility::semantics::CheckedState;
using fuchsia::accessibility::semantics::Node;
using fuchsia::accessibility::semantics::Role;
using fuchsia::accessibility::semantics::States;

namespace {

const char kLabel1[] = "label nodes, not people";
const char kLabel2[] = "fancy stickers";
const char kDescription1[] = "this node does some stuff";
const char kValue1[] = "user entered value";
const int32_t kChildId1 = 23901;
const int32_t kChildId2 = 484345;
const int32_t kChildId3 = 4156877;
const int32_t kRootId = 5;
const int32_t kRectX = 1;
const int32_t kRectY = 2;
const int32_t kRectWidth = 7;
const int32_t kRectHeight = 8;
const uint32_t kInvalidIdRemappedForFuchsia = 1u + INT32_MAX;
const std::array<float, 16> k4DIdentityMatrix = {1, 0, 0, 0, 0, 1, 0, 0,
                                                 0, 0, 1, 0, 0, 0, 0, 1};

ui::AXNodeData CreateAXNodeData(ax::mojom::Role role,
                                ax::mojom::Action action,
                                std::vector<int32_t> child_ids,
                                ui::AXRelativeBounds relative_bounds,
                                base::StringPiece name,
                                base::StringPiece description,
                                ax::mojom::CheckedState checked_state) {
  ui::AXNodeData node;
  // Important! ID must be set to zero here because its default value (-1), will
  // fail when getting converted to an unsigned int (Fuchsia's ID format).
  node.id = 0;
  node.role = role;
  node.AddAction(action);
  node.AddIntAttribute(ax::mojom::IntAttribute::kCheckedState,
                       static_cast<int32_t>(checked_state));
  node.child_ids = child_ids;
  node.relative_bounds = relative_bounds;
  if (!name.empty())
    node.AddStringAttribute(ax::mojom::StringAttribute::kName, name.data());
  if (!description.empty()) {
    node.AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                            description.data());
  }
  return node;
}

Node CreateSemanticNode(uint32_t id,
                        Role role,
                        Attributes attributes,
                        States states,
                        std::vector<Action> actions,
                        std::vector<uint32_t> child_ids,
                        fuchsia::ui::gfx::BoundingBox location,
                        fuchsia::ui::gfx::mat4 transform) {
  Node node;
  node.set_node_id(id);
  node.set_role(role);
  node.set_attributes(std::move(attributes));
  node.set_states(std::move(states));
  node.set_actions(actions);
  node.set_child_ids(child_ids);
  node.set_location(location);
  node.set_transform(transform);
  return node;
}

// Create an AXNodeData and a Fuchsia node that represent the same information.
std::pair<ui::AXNodeData, Node> CreateSemanticNodeAllFieldsSet() {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  auto ax_node_data = CreateAXNodeData(
      ax::mojom::Role::kButton, ax::mojom::Action::kFocus,
      std::vector<int32_t>{kChildId1, kChildId2, kChildId3}, relative_bounds,
      kLabel1, kDescription1, ax::mojom::CheckedState::kMixed);
  ax_node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  ax_node_data.RemoveState(ax::mojom::State::kIgnored);
  ax_node_data.id = kChildId1;

  Attributes attributes;
  attributes.set_label(kLabel1);
  attributes.set_secondary_label(kDescription1);
  fuchsia::ui::gfx::BoundingBox box;
  box.min = scenic::NewVector3({kRectX, kRectY + kRectHeight, 0.0f});
  box.max = scenic::NewVector3({kRectHeight, kRectY, 0.0f});
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrix);
  States states;
  states.set_checked_state(CheckedState::MIXED);
  states.set_hidden(false);
  states.set_selected(false);
  auto fuchsia_node = CreateSemanticNode(
      ConvertToFuchsiaNodeId(ax_node_data.id, kRootId), Role::BUTTON,
      std::move(attributes), std::move(states),
      std::vector<Action>{Action::SET_FOCUS},
      std::vector<uint32_t>{kChildId1, kChildId2, kChildId3}, box, mat.value);

  return std::make_pair(std::move(ax_node_data), std::move(fuchsia_node));
}

class AXTreeConverterTest : public testing::Test {
 public:
  AXTreeConverterTest() = default;
  ~AXTreeConverterTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(AXTreeConverterTest);
};

TEST_F(AXTreeConverterTest, AllFieldsSetAndEqual) {
  auto nodes = CreateSemanticNodeAllFieldsSet();
  auto& source_node_data = nodes.first;
  auto& expected_node = nodes.second;

  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(ConvertToFuchsiaNodeId(source_node_data.id, kRootId),
            converted_node.node_id());

  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, SomeFieldsSetAndEqual) {
  ui::AXNodeData source_node_data;
  source_node_data.id = 0;
  source_node_data.AddAction(ax::mojom::Action::kFocus);
  source_node_data.AddAction(ax::mojom::Action::kSetValue);
  source_node_data.child_ids = std::vector<int32_t>{kChildId1};
  source_node_data.role = ax::mojom::Role::kImage;
  source_node_data.AddStringAttribute(ax::mojom::StringAttribute::kValue,
                                      kValue1);
  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(static_cast<uint32_t>(source_node_data.id),
            converted_node.node_id());

  Node expected_node;
  expected_node.set_node_id(static_cast<uint32_t>(source_node_data.id));
  expected_node.set_actions(
      std::vector<Action>{Action::SET_FOCUS, Action::SET_VALUE});
  expected_node.set_child_ids(
      std::vector<uint32_t>{static_cast<uint32_t>(kChildId1)});
  expected_node.set_role(Role::IMAGE);
  States states;
  states.set_hidden(false);
  states.set_value(kValue1);
  expected_node.set_states(std::move(states));
  Attributes attributes;
  expected_node.set_attributes(std::move(attributes));
  fuchsia::ui::gfx::BoundingBox box;
  expected_node.set_location(std::move(box));

  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, FieldMismatch) {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  auto source_node_data = CreateAXNodeData(
      ax::mojom::Role::kHeader, ax::mojom::Action::kSetValue,
      std::vector<int32_t>{kChildId1, kChildId2, kChildId3}, relative_bounds,
      kLabel1, kDescription1, ax::mojom::CheckedState::kFalse);
  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(static_cast<uint32_t>(source_node_data.id),
            converted_node.node_id());

  Attributes attributes;
  attributes.set_label(kLabel1);
  attributes.set_secondary_label(kDescription1);
  States states;
  states.set_hidden(false);
  states.set_checked_state(CheckedState::UNCHECKED);
  fuchsia::ui::gfx::BoundingBox box;
  box.min = scenic::NewVector3({kRectX, kRectY + kRectHeight, 0.0f});
  box.max = scenic::NewVector3({kRectHeight, kRectY, 0.0f});
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrix);
  auto expected_node = CreateSemanticNode(
      source_node_data.id, Role::HEADER, std::move(attributes),
      std::move(states), std::vector<Action>{Action::SET_VALUE},
      std::vector<uint32_t>{kChildId1, kChildId2, kChildId3}, box, mat.value);

  // Start with nodes being equal.
  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));

  // Make a copy of |source_node_data| and change the name attribute. Check that
  // the resulting |converted_node| is different from |expected_node|.
  auto modified_node_data = source_node_data;
  modified_node_data.AddStringAttribute(ax::mojom::StringAttribute::kName,
                                        kLabel2);
  converted_node = AXNodeDataToSemanticNode(modified_node_data);
  EXPECT_FALSE(fidl::Equals(converted_node, expected_node));

  // The same as above, this time changing |child_ids|.
  modified_node_data = source_node_data;
  modified_node_data.child_ids = std::vector<int32_t>{};
  converted_node = AXNodeDataToSemanticNode(modified_node_data);
  EXPECT_FALSE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, DefaultAction) {
  auto nodes = CreateSemanticNodeAllFieldsSet();
  auto& source_node_data = nodes.first;
  auto& expected_node = nodes.second;

  // Default action verb on an AXNodeData is equivalent to Action::DEFAULT on a
  // Fuchsia semantic node.
  source_node_data.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  expected_node.mutable_actions()->insert(
      expected_node.mutable_actions()->begin(),
      fuchsia::accessibility::semantics::Action::DEFAULT);

  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(ConvertToFuchsiaNodeId(source_node_data.id, kRootId),
            converted_node.node_id());

  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, ConvertToFuchsiaNodeId) {
  // Root AxNode is 0, Fuchsia is also 0.
  EXPECT_EQ(0u, ConvertToFuchsiaNodeId(0, 0));

  // Root AxNode is not 0, Fuchsia is still 0.
  EXPECT_EQ(0u, ConvertToFuchsiaNodeId(2, 2));

  // Regular AxNode is 0, Fuchsia can't be 0.
  EXPECT_EQ(kInvalidIdRemappedForFuchsia, ConvertToFuchsiaNodeId(0, 2));

  // Regular AxNode is not 0, Fuchsia is same value.
  EXPECT_EQ(10u, ConvertToFuchsiaNodeId(10, 0));
}

TEST_F(AXTreeConverterTest, ConvertToAxNodeId) {
  // Root Fuchsia id is 0, AxNode is also 0.
  EXPECT_EQ(0, ConvertToAxNodeId(0u, 0));

  // Root Fuchsia id is 0, AxNode is not 0.
  EXPECT_EQ(2, ConvertToAxNodeId(0u, 2));

  // Fuchsia node is the max value, AxNode should be 0.
  EXPECT_EQ(0, ConvertToAxNodeId(kInvalidIdRemappedForFuchsia, 2));

  // Regular Fuchsia node is not root, AxNode is same value.
  EXPECT_EQ(10, ConvertToAxNodeId(10u, 0));
}

TEST_F(AXTreeConverterTest, ConvertRoles) {
  ui::AXNodeData node;
  node.id = 0;
  node.role = ax::mojom::Role::kButton;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::BUTTON,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kCheckBox;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::CHECK_BOX,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kHeader;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::HEADER,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kImage;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::IMAGE,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kLink;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::LINK,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kRadioButton;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::RADIO_BUTTON,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kSlider;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::SLIDER,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kTextField;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::TEXT_FIELD,
            AXNodeDataToSemanticNode(node).role());

  node.role = ax::mojom::Role::kStaticText;
  EXPECT_EQ(fuchsia::accessibility::semantics::Role::STATIC_TEXT,
            AXNodeDataToSemanticNode(node).role());
}

}  // namespace
