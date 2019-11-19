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
using fuchsia::accessibility::semantics::Node;
using fuchsia::accessibility::semantics::Role;

namespace {

const char kLabel1[] = "label nodes, not people";
const char kLabel2[] = "fancy stickers";
const int32_t kChildId1 = 23901;
const int32_t kChildId2 = 484345;
const int32_t kChildId3 = 4156877;
const int32_t kRectX = 1;
const int32_t kRectY = 2;
const int32_t kRectWidth = 7;
const int32_t kRectHeight = 8;
const float k4DIdentityMatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                     0, 0, 1, 0, 0, 0, 0, 1};

bool SemanticNodesAreEqual(const Node& first, const Node& second) {
  if (first.node_id() != second.node_id())
    return false;
  if (first.has_role() && second.has_role() && first.role() != second.role()) {
    return false;
  }
  if (first.has_attributes() && second.has_attributes() &&
      first.attributes().label() != second.attributes().label()) {
    return false;
  }
  if (first.has_actions() && second.has_actions() &&
      first.actions() != second.actions()) {
    return false;
  }
  if (first.has_child_ids() && second.has_child_ids() &&
      first.child_ids() != second.child_ids()) {
    return false;
  }
  if (first.has_location() && second.has_location()) {
    if (first.location().min.x != second.location().min.x)
      return false;
    if (first.location().min.y != second.location().min.y)
      return false;
    if (first.location().min.z != second.location().min.z)
      return false;
    if (first.location().max.x != second.location().max.x)
      return false;
    if (first.location().max.y != second.location().max.y)
      return false;
    if (first.location().max.z != second.location().max.z)
      return false;
  }
  if (first.has_transform() && second.has_transform() &&
      first.transform().matrix != second.transform().matrix) {
    return false;
  }

  return true;
}

ui::AXNodeData CreateAXNodeData(ax::mojom::Role role,
                                uint32_t actions,
                                std::vector<int32_t> child_ids,
                                ui::AXRelativeBounds relative_bounds,
                                base::StringPiece name) {
  ui::AXNodeData node;
  node.role = role;
  node.actions = actions;
  node.child_ids = child_ids;
  node.relative_bounds = relative_bounds;
  if (!name.empty())
    node.AddStringAttribute(ax::mojom::StringAttribute::kName, name.data());
  return node;
}

Node CreateSemanticNode(uint32_t id,
                        Role role,
                        Attributes attributes,
                        std::vector<Action> actions,
                        std::vector<uint32_t> child_ids,
                        fuchsia::ui::gfx::BoundingBox location,
                        fuchsia::ui::gfx::mat4 transform) {
  Node node;
  node.set_node_id(id);
  node.set_role(role);
  node.set_attributes(std::move(attributes));
  node.set_actions(actions);
  node.set_child_ids(child_ids);
  node.set_location(location);
  node.set_transform(transform);
  return node;
}

class AXTreeConverterTest : public testing::Test {
 public:
  AXTreeConverterTest() = default;
  ~AXTreeConverterTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(AXTreeConverterTest);
};

TEST_F(AXTreeConverterTest, AllFieldsSetAndEqual) {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  auto source_node_data =
      CreateAXNodeData(ax::mojom::Role::kButton,
                       static_cast<uint32_t>(ax::mojom::Action::kDoDefault),
                       std::vector<int32_t>{kChildId1, kChildId2, kChildId3},
                       relative_bounds, kLabel1);
  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(static_cast<uint32_t>(source_node_data.id),
            converted_node.node_id());

  Attributes attributes;
  attributes.set_label(kLabel1);
  fuchsia::ui::gfx::BoundingBox box;
  float min[3] = {kRectX, kRectY + kRectHeight, 0.0f};
  float max[3] = {kRectHeight, kRectY, 0.0f};
  box.min = scenic::NewVector3(min);
  box.max = scenic::NewVector3(max);
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrix);
  auto expected_node = CreateSemanticNode(
      source_node_data.id, Role::UNKNOWN, std::move(attributes),
      std::vector<Action>{Action::DEFAULT},
      std::vector<uint32_t>{kChildId1, kChildId2, kChildId3}, box, mat.value);

  EXPECT_TRUE(SemanticNodesAreEqual(std::move(converted_node),
                                    std::move(expected_node)));
}

TEST_F(AXTreeConverterTest, SomeFieldsSetAndEqual) {
  ui::AXNodeData source_node_data;
  source_node_data.actions =
      static_cast<uint32_t>(ax::mojom::Action::kDoDefault);
  source_node_data.child_ids = std::vector<int32_t>{kChildId1};
  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(static_cast<uint32_t>(source_node_data.id),
            converted_node.node_id());

  Node expected_node;
  expected_node.set_node_id(source_node_data.id);
  expected_node.set_actions(std::vector<Action>{Action::DEFAULT});
  expected_node.set_child_ids(std::vector<uint32_t>{kChildId1});

  EXPECT_TRUE(SemanticNodesAreEqual(std::move(converted_node),
                                    std::move(expected_node)));
}

TEST_F(AXTreeConverterTest, FieldMismatch) {
  ui::AXRelativeBounds relative_bounds = ui::AXRelativeBounds();
  relative_bounds.bounds = gfx::RectF(kRectX, kRectY, kRectWidth, kRectHeight);
  relative_bounds.transform =
      std::make_unique<gfx::Transform>(gfx::Transform::kSkipInitialization);
  relative_bounds.transform->MakeIdentity();
  auto source_node_data =
      CreateAXNodeData(ax::mojom::Role::kButton,
                       static_cast<uint32_t>(ax::mojom::Action::kDoDefault),
                       std::vector<int32_t>{kChildId1, kChildId2, kChildId3},
                       relative_bounds, kLabel1);
  auto converted_node = AXNodeDataToSemanticNode(source_node_data);
  EXPECT_EQ(static_cast<uint32_t>(source_node_data.id),
            converted_node.node_id());

  Attributes attributes;
  attributes.set_label(kLabel1);
  fuchsia::ui::gfx::BoundingBox box;
  float min[3] = {kRectX, kRectY + kRectHeight, 0.0f};
  float max[3] = {kRectHeight, kRectY, 0.0f};
  box.min = scenic::NewVector3(min);
  box.max = scenic::NewVector3(max);
  fuchsia::ui::gfx::Matrix4Value mat =
      scenic::NewMatrix4Value(k4DIdentityMatrix);
  auto expected_node = CreateSemanticNode(
      source_node_data.id, Role::UNKNOWN, std::move(attributes),
      std::vector<Action>{Action::DEFAULT},
      std::vector<uint32_t>{kChildId1, kChildId2, kChildId3}, box, mat.value);

  // Start with nodes being equal.
  EXPECT_TRUE(SemanticNodesAreEqual(converted_node, expected_node));

  // Make a copy of |source_node_data| and change the name attribute. Check that
  // the resulting |converted_node| is different from |expected_node|.
  auto modified_node_data = source_node_data;
  modified_node_data.AddStringAttribute(ax::mojom::StringAttribute::kName,
                                        kLabel2);
  converted_node = AXNodeDataToSemanticNode(modified_node_data);
  EXPECT_FALSE(SemanticNodesAreEqual(converted_node, expected_node));

  // The same as above, this time changing |child_ids|.
  modified_node_data = source_node_data;
  modified_node_data.child_ids = std::vector<int32_t>{};
  converted_node = AXNodeDataToSemanticNode(modified_node_data);
  EXPECT_FALSE(SemanticNodesAreEqual(converted_node, expected_node));
}

}  // namespace
