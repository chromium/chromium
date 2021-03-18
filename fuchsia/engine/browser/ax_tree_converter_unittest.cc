// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/ax_tree_converter.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace {

using fuchsia::accessibility::semantics::Action;
using fuchsia::accessibility::semantics::Attributes;
using fuchsia::accessibility::semantics::CheckedState;
using fuchsia::accessibility::semantics::Node;
using fuchsia::accessibility::semantics::Role;
using fuchsia::accessibility::semantics::States;

const char kLabel1[] = "label nodes, not people";
const char kLabel2[] = "fancy stickers";
const char kDescription1[] = "this node does some stuff";
const char kValue1[] = "user entered value";
const int32_t kRootId = 182;
const int32_t kChildId1 = 23901;
const int32_t kChildId2 = 484345;
const int32_t kChildId3 = 4156877;
const int32_t kRectX = 1;
const int32_t kRectY = 2;
const int32_t kRectWidth = 7;
const int32_t kRectHeight = 8;
const std::array<float, 16> k4DIdentityMatrixWithDefaultOffset = {
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 0, 1};

class MockNodeIDMapper : public NodeIDMapper {
 public:
  MockNodeIDMapper() = default;
  ~MockNodeIDMapper() override = default;

  uint32_t ToFuchsiaNodeID(const ui::AXTreeID& ax_tree_id,
                           int32_t ax_node_id,
                           bool is_tree_root) override {
    return base::checked_cast<uint32_t>(ax_node_id);
  }
};

ui::AXNodeData CreateAXNodeData(ax::mojom::Role role,
                                ax::mojom::Action action,
                                std::vector<int32_t> child_ids,
                                ui::AXRelativeBounds relative_bounds,
                                base::StringPiece name,
                                base::StringPiece description,
                                ax::mojom::CheckedState checked_state) {
  ui::AXNodeData node;
  node.id = 2;
  node.role = role;
  if (action != ax::mojom::Action::kNone) {
    node.AddAction(action);
  }

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
  node.set_container_id(kRootId);
  return node;
}

// Create an AXNodeData and a Fuchsia node that represent the same information.
std::pair<ui::AXNodeData, Node> CreateSemanticNodeAllFieldsSet() {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  relative_bounds.offset_container_id = -1;
  auto ax_node_data = CreateAXNodeData(
      ax::mojom::Role::kButton, ax::mojom::Action::kFocus,
      std::vector<int32_t>{kChildId1, kChildId2, kChildId3}, relative_bounds,
      kLabel1, kDescription1, ax::mojom::CheckedState::kMixed);
  ax_node_data.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  ax_node_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 10);
  ax_node_data.RemoveState(ax::mojom::State::kIgnored);
  ax_node_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 20);
  ax_node_data.id = kChildId1;

  Attributes attributes;
  attributes.set_label(kLabel1);
  attributes.set_secondary_label(kDescription1);
  fuchsia::ui::gfx::BoundingBox box;
  box.min = scenic::NewVector3({kRectX, kRectY, 0.0f});
  box.max =
      scenic::NewVector3({kRectX + kRectWidth, kRectY + kRectHeight, 0.0f});
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrixWithDefaultOffset);
  States states;
  states.set_checked_state(CheckedState::MIXED);
  states.set_hidden(false);
  states.set_selected(false);
  states.set_viewport_offset({10, 20});
  MockNodeIDMapper mapper;
  auto fuchsia_node = CreateSemanticNode(
      mapper.ToFuchsiaNodeID(ui::AXTreeID::CreateNewAXTreeID(), ax_node_data.id,
                             false),
      Role::BUTTON, std::move(attributes), std::move(states),
      std::vector<Action>{Action::SET_FOCUS},
      std::vector<uint32_t>{kChildId1, kChildId2, kChildId3}, box, mat.value);

  return std::make_pair(std::move(ax_node_data), std::move(fuchsia_node));
}

class AXTreeConverterTest : public testing::Test {
 public:
  AXTreeConverterTest() {
    ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
    relative_bounds.bounds =
        gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
    root_node_data_ =
        CreateAXNodeData(ax::mojom::Role::kNone, ax::mojom::Action::kNone,
                         std::vector<int32_t>{}, relative_bounds, "", "",
                         ax::mojom::CheckedState::kNone);
    root_node_data_.id = kRootId;
  }
  ~AXTreeConverterTest() override = default;

  ui::AXNodeData& root_node() { return root_node_data_; }

  DISALLOW_COPY_AND_ASSIGN(AXTreeConverterTest);

 private:
  ui::AXNodeData root_node_data_;
};

TEST_F(AXTreeConverterTest, AllFieldsSetAndEqual) {
  auto nodes = CreateSemanticNodeAllFieldsSet();
  auto& source_node_data = nodes.first;
  auto& expected_node = nodes.second;

  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      source_node_data, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false,
      &mapper);
  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, TransformAccountsForContainerOffset) {
  ui::AXNodeData container_node_data;
  container_node_data.id = 0;
  container_node_data.relative_bounds = ui::AXRelativeBounds();
  container_node_data.relative_bounds.bounds =
      gfx::RectF(100 /* x */, 200 /* y */, 10 /* width */, 20 /* height */);

  ui::AXNodeData child_node_data;
  child_node_data.id = 1;
  child_node_data.relative_bounds.transform = std::make_unique<gfx::Transform>(
      2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      child_node_data, container_node_data, ui::AXTreeID::CreateNewAXTreeID(),
      false, &mapper);

  Node expected_node;
  expected_node.set_node_id(1);
  fuchsia::ui::gfx::BoundingBox box;
  expected_node.set_location(std::move(box));
  // The fuchsia node transform should include a post-translation for the
  // container node's relative bounds.
  auto expected_transform = scenic::NewMatrix4Value(
      {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 100, 200, 0, 1});
  expected_node.set_transform(expected_transform.value);
  EXPECT_EQ(converted_node.transform().matrix,
            expected_node.transform().matrix);
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

  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      source_node_data, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false,
      &mapper);

  Node expected_node;
  expected_node.set_node_id(0);
  expected_node.set_actions(
      std::vector<Action>{Action::SET_FOCUS, Action::SET_VALUE});
  expected_node.set_child_ids(std::vector<uint32_t>{kChildId1});
  expected_node.set_role(Role::IMAGE);
  States states;
  states.set_hidden(false);
  states.set_value(kValue1);
  expected_node.set_states(std::move(states));
  Attributes attributes;
  expected_node.set_attributes(std::move(attributes));
  fuchsia::ui::gfx::BoundingBox box;
  expected_node.set_location(std::move(box));
  expected_node.set_container_id(kRootId);
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrixWithDefaultOffset);
  expected_node.set_transform(mat.value);
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

  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      source_node_data, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false,
      &mapper);

  Attributes attributes;
  attributes.set_label(kLabel1);
  attributes.set_secondary_label(kDescription1);
  States states;
  states.set_hidden(false);
  states.set_checked_state(CheckedState::UNCHECKED);
  fuchsia::ui::gfx::BoundingBox box;
  box.min = scenic::NewVector3({kRectX, kRectY, 0.0f});
  box.max =
      scenic::NewVector3({kRectX + kRectWidth, kRectY + kRectHeight, 0.0f});
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrixWithDefaultOffset);
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

  converted_node = AXNodeDataToSemanticNode(modified_node_data, root_node(),
                                            ui::AXTreeID::CreateNewAXTreeID(),
                                            false, &mapper);
  EXPECT_FALSE(fidl::Equals(converted_node, expected_node));

  // The same as above, this time changing |child_ids|.
  modified_node_data = source_node_data;
  modified_node_data.child_ids = std::vector<int32_t>{};
  converted_node = AXNodeDataToSemanticNode(modified_node_data, root_node(),
                                            ui::AXTreeID::CreateNewAXTreeID(),
                                            false, &mapper);
  EXPECT_FALSE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, LocationFieldRespectsTypeInvariants) {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  auto source_node_data = CreateAXNodeData(
      ax::mojom::Role::kHeader, ax::mojom::Action::kSetValue,
      std::vector<int32_t>{kChildId1, kChildId2, kChildId3}, relative_bounds,
      kLabel1, kDescription1, ax::mojom::CheckedState::kFalse);

  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      source_node_data, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false,
      &mapper);

  // The type definition of the location field requires that in order to be
  // interpreted as having non-zero length in a dimension, the min must be less
  // than the max in that dimension.
  EXPECT_LE(converted_node.location().min.x, converted_node.location().max.x);
  EXPECT_LE(converted_node.location().min.y, converted_node.location().max.y);
  EXPECT_LE(converted_node.location().min.z, converted_node.location().max.z);
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

  MockNodeIDMapper mapper;
  auto converted_node = AXNodeDataToSemanticNode(
      source_node_data, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false,
      &mapper);

  EXPECT_TRUE(fidl::Equals(converted_node, expected_node));
}

TEST_F(AXTreeConverterTest, MapsNodeIDs) {
  NodeIDMapper mapper;
  const ui::AXTreeID tree_id_1 = ui::AXTreeID::CreateNewAXTreeID();
  const ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  const ui::AXTreeID tree_id_3 = ui::AXTreeID::CreateNewAXTreeID();

  auto id = mapper.ToFuchsiaNodeID(tree_id_1, 1, false);
  EXPECT_EQ(id, 1u);

  id = mapper.ToFuchsiaNodeID(tree_id_2, 1, false);
  EXPECT_EQ(id, 2u);

  const auto result_1 = mapper.ToAXNodeID(1u);
  EXPECT_TRUE(result_1);
  EXPECT_EQ(result_1->first, tree_id_1);
  EXPECT_EQ(result_1->second, 1);

  const auto result_2 = mapper.ToAXNodeID(2u);
  EXPECT_TRUE(result_2);
  EXPECT_EQ(result_2->first, tree_id_2);
  EXPECT_EQ(result_2->second, 1);

  // Set the root.
  id = mapper.ToFuchsiaNodeID(tree_id_1, 2, true);
  EXPECT_EQ(id, 0u);

  // Update the root. The old root should receive a new value.
  id = mapper.ToFuchsiaNodeID(tree_id_1, 1, true);
  EXPECT_EQ(id, 0u);
  const auto result_3 = mapper.ToAXNodeID(3u);
  EXPECT_TRUE(result_3);
  EXPECT_EQ(result_3->first, tree_id_1);
  EXPECT_EQ(result_3->second, 2);  // First root's ID.

  mapper.UpdateAXTreeIDForCachedNodeIDs(tree_id_1, tree_id_3);
  const auto result_4 = mapper.ToAXNodeID(3u);
  EXPECT_TRUE(result_4);
  EXPECT_EQ(result_4->first, tree_id_3);
  EXPECT_EQ(result_4->second, 2);
}

TEST_F(AXTreeConverterTest, ConvertRoles) {
  MockNodeIDMapper mapper;
  ui::AXNodeData node;
  node.id = 0;
  node.role = ax::mojom::Role::kButton;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::BUTTON,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kCheckBox;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::CHECK_BOX,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kHeader;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::HEADER,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kImage;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::IMAGE,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kLink;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::LINK,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kRadioButton;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::RADIO_BUTTON,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kSlider;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::SLIDER,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kTextField;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::TEXT_FIELD,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());

  node.role = ax::mojom::Role::kStaticText;
  EXPECT_EQ(
      fuchsia::accessibility::semantics::Role::STATIC_TEXT,
      AXNodeDataToSemanticNode(
          node, root_node(), ui::AXTreeID::CreateNewAXTreeID(), false, &mapper)
          .role());
}

}  // namespace
