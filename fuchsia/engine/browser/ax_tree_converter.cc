// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/ax_tree_converter.h"

#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <vector>

#include "base/bit_cast.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

using fuchsia::accessibility::semantics::MAX_LABEL_SIZE;

namespace {

fuchsia::accessibility::semantics::Role ConvertRole(ax::mojom::Role role) {
  if (role == ax::mojom::Role::kButton)
    return fuchsia::accessibility::semantics::Role::BUTTON;
  if (role == ax::mojom::Role::kHeader)
    return fuchsia::accessibility::semantics::Role::HEADER;
  if (role == ax::mojom::Role::kImage)
    return fuchsia::accessibility::semantics::Role::IMAGE;
  if (role == ax::mojom::Role::kTextField)
    return fuchsia::accessibility::semantics::Role::TEXT_FIELD;

  return fuchsia::accessibility::semantics::Role::UNKNOWN;
}

fuchsia::accessibility::semantics::Attributes ConvertAttributes(
    const ui::AXNodeData& node) {
  fuchsia::accessibility::semantics::Attributes attributes;
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    attributes.set_label(name.substr(0, MAX_LABEL_SIZE));
  }

  if (node.HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    const std::string& description =
        node.GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    attributes.set_secondary_label(description.substr(0, MAX_LABEL_SIZE));
  }

  return attributes;
}

// This function handles conversions for all data that is part of a Semantic
// Node's state. The corresponding data in an AXNodeData is stored in various
// attributes.
fuchsia::accessibility::semantics::States ConvertStates(
    const ui::AXNodeData& node) {
  fuchsia::accessibility::semantics::States states;

  // Converts checked state of a node.
  if (node.HasIntAttribute(ax::mojom::IntAttribute::kCheckedState)) {
    ax::mojom::CheckedState ax_state = node.GetCheckedState();
    switch (ax_state) {
      case ax::mojom::CheckedState::kNone:
        states.set_checked_state(
            fuchsia::accessibility::semantics::CheckedState::NONE);
        break;
      case ax::mojom::CheckedState::kTrue:
        states.set_checked_state(
            fuchsia::accessibility::semantics::CheckedState::CHECKED);
        break;
      case ax::mojom::CheckedState::kFalse:
        states.set_checked_state(
            fuchsia::accessibility::semantics::CheckedState::UNCHECKED);
        break;
      case ax::mojom::CheckedState::kMixed:
        states.set_checked_state(
            fuchsia::accessibility::semantics::CheckedState::MIXED);
        break;
    }
  }

  // Indicates whether a node has been selected.
  if (node.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    states.set_selected(
        node.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  // Indicates if the node is hidden.
  states.set_hidden(node.HasState(ax::mojom::State::kInvisible));

  // The user entered value of the node, if applicable.
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    const std::string& value =
        node.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    states.set_value(value.substr(0, MAX_LABEL_SIZE));
  }

  return states;
}

std::vector<fuchsia::accessibility::semantics::Action> ConvertActions(
    const ui::AXNodeData& node) {
  std::vector<fuchsia::accessibility::semantics::Action> fuchsia_actions;

  if (node.HasAction(ax::mojom::Action::kDoDefault)) {
    fuchsia_actions.push_back(
        fuchsia::accessibility::semantics::Action::DEFAULT);
  }
  if (node.HasAction(ax::mojom::Action::kFocus)) {
    fuchsia_actions.push_back(
        fuchsia::accessibility::semantics::Action::SET_FOCUS);
  }
  if (node.HasAction(ax::mojom::Action::kSetValue)) {
    fuchsia_actions.push_back(
        fuchsia::accessibility::semantics::Action::SET_VALUE);
  }
  if (node.HasAction(ax::mojom::Action::kScrollToMakeVisible)) {
    fuchsia_actions.push_back(
        fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN);
  }

  return fuchsia_actions;
}

std::vector<uint32_t> ConvertChildIds(std::vector<int32_t> ids) {
  std::vector<uint32_t> child_ids;
  child_ids.reserve(ids.size());
  for (auto i : ids) {
    child_ids.push_back(bit_cast<uint32_t>(i));
  }
  return child_ids;
}

fuchsia::ui::gfx::BoundingBox ConvertBoundingBox(gfx::RectF bounds) {
  fuchsia::ui::gfx::BoundingBox box;
  box.min = scenic::NewVector3({bounds.bottom_left().x(),
                                bounds.bottom_left().y(), 0.0f});
  box.max = scenic::NewVector3({bounds.top_right().x(), bounds.top_right().y(),
                                0.0f});
  return box;
}

// The Semantics Manager applies this matrix to position the node and its
// subtree as an optimization to handle resizing or repositioning. This requires
// only one node to be updated on such an event.
fuchsia::ui::gfx::mat4 ConvertTransform(gfx::Transform* transform) {
  std::array<float, 16> mat = {};
  transform->matrix().asColMajorf(mat.data());
  fuchsia::ui::gfx::Matrix4Value fuchsia_transform =
      scenic::NewMatrix4Value(mat);
  return fuchsia_transform.value;
}

}  // namespace

fuchsia::accessibility::semantics::Node AXNodeDataToSemanticNode(
    const ui::AXNodeData& node) {
  fuchsia::accessibility::semantics::Node fuchsia_node;
  fuchsia_node.set_node_id(bit_cast<uint32_t>(node.id));
  fuchsia_node.set_role(ConvertRole(node.role));
  fuchsia_node.set_states(ConvertStates(node));
  fuchsia_node.set_attributes(ConvertAttributes(node));
  fuchsia_node.set_actions(ConvertActions(node));
  fuchsia_node.set_child_ids(ConvertChildIds(node.child_ids));
  fuchsia_node.set_location(ConvertBoundingBox(node.relative_bounds.bounds));
  if (node.relative_bounds.transform) {
    fuchsia_node.set_transform(
        ConvertTransform(node.relative_bounds.transform.get()));
  }

  return fuchsia_node;
}

bool ConvertAction(fuchsia::accessibility::semantics::Action fuchsia_action,
                   ax::mojom::Action* mojom_action) {
  switch (fuchsia_action) {
    case fuchsia::accessibility::semantics::Action::DEFAULT:
      *mojom_action = ax::mojom::Action::kDoDefault;
      return true;
    case fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN:
      *mojom_action = ax::mojom::Action::kScrollToMakeVisible;
      return true;
    case fuchsia::accessibility::semantics::Action::SECONDARY:
    case fuchsia::accessibility::semantics::Action::SET_FOCUS:
    case fuchsia::accessibility::semantics::Action::SET_VALUE:
      return false;
    default:
      LOG(WARNING)
          << "Unknown fuchsia::accessibility::semantics::Action with value "
          << static_cast<int>(fuchsia_action);
      return false;
  }
}
