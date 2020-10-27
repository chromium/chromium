// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/ax_tree_converter.h"

#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/ui/scenic/cpp/commands.h>
#include <stdint.h>
#include <vector>

#include "base/bit_cast.h"
#include "base/numerics/safe_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

using fuchsia::accessibility::semantics::MAX_LABEL_SIZE;

namespace {

// Fuchsia's default root node ID.
constexpr uint32_t kFuchsiaRootNodeId = 0;

// Remapped value for AXNode::kInvalidAXID.
// Value is chosen to be outside the range of a 32-bit signed int, so as to not
// conflict with other AXIDs.
constexpr uint32_t kInvalidIdRemappedForFuchsia = 1u + INT32_MAX;

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

  if (node.IsRangeValueSupported()) {
    fuchsia::accessibility::semantics::RangeAttributes range_attributes;
    if (node.HasFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange)) {
      range_attributes.set_min_value(
          node.GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange));
    }
    if (node.HasFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange)) {
      range_attributes.set_max_value(
          node.GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange));
    }
    if (node.HasFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange)) {
      range_attributes.set_step_delta(node.GetFloatAttribute(
          ax::mojom::FloatAttribute::kStepValueForRange));
    }
    attributes.set_range(std::move(range_attributes));
  }

  return attributes;
}

// Converts an ax::mojom::Role to a fuchsia::accessibility::semantics::Role.
fuchsia::accessibility::semantics::Role AxRoleToFuchsiaSemanticRole(
    ax::mojom::Role role) {
  switch (role) {
    case ax::mojom::Role::kButton:
      return fuchsia::accessibility::semantics::Role::BUTTON;
    case ax::mojom::Role::kCheckBox:
      return fuchsia::accessibility::semantics::Role::CHECK_BOX;
    case ax::mojom::Role::kHeader:
      return fuchsia::accessibility::semantics::Role::HEADER;
    case ax::mojom::Role::kImage:
      return fuchsia::accessibility::semantics::Role::IMAGE;
    case ax::mojom::Role::kLink:
      return fuchsia::accessibility::semantics::Role::LINK;
    case ax::mojom::Role::kRadioButton:
      return fuchsia::accessibility::semantics::Role::RADIO_BUTTON;
    case ax::mojom::Role::kSlider:
      return fuchsia::accessibility::semantics::Role::SLIDER;
    case ax::mojom::Role::kTextField:
      return fuchsia::accessibility::semantics::Role::TEXT_FIELD;
    case ax::mojom::Role::kStaticText:
      return fuchsia::accessibility::semantics::Role::STATIC_TEXT;
    default:
      return fuchsia::accessibility::semantics::Role::UNKNOWN;
  }

  return fuchsia::accessibility::semantics::Role::UNKNOWN;
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
  states.set_hidden(node.IsIgnored());

  // The user entered value of the node, if applicable.
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    const std::string& value =
        node.GetStringAttribute(ax::mojom::StringAttribute::kValue);
    states.set_value(value.substr(0, MAX_LABEL_SIZE));
  }

  // The value a range element currently has.
  if (node.HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    states.set_range_value(
        node.GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange));
  }

  return states;
}

std::vector<fuchsia::accessibility::semantics::Action> ConvertActions(
    const ui::AXNodeData& node) {
  std::vector<fuchsia::accessibility::semantics::Action> fuchsia_actions;

  const bool has_default =
      node.HasAction(ax::mojom::Action::kDoDefault) ||
      node.GetDefaultActionVerb() != ax::mojom::DefaultActionVerb::kNone;
  if (has_default) {
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
    child_ids.push_back(base::checked_cast<uint32_t>(i));
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
  fuchsia_node.set_node_id(base::checked_cast<uint32_t>(node.id));
  fuchsia_node.set_role(AxRoleToFuchsiaSemanticRole(node.role));
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
    case fuchsia::accessibility::semantics::Action::DECREMENT:
      *mojom_action = ax::mojom::Action::kDecrement;
      return true;
    case fuchsia::accessibility::semantics::Action::INCREMENT:
      *mojom_action = ax::mojom::Action::kIncrement;
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

uint32_t ConvertToFuchsiaNodeId(int32_t ax_node_id, int32_t ax_root_node_id) {
  if (ax_node_id == ax_root_node_id)
    return kFuchsiaRootNodeId;

  // kInvalidAXID has the same value as the Fuchsia root ID. It is remapped to
  // avoid a conflict.
  if (ax_node_id == ui::AXNode::kInvalidAXID)
    return kInvalidIdRemappedForFuchsia;

  return base::checked_cast<uint32_t>(ax_node_id);
}

int32_t ConvertToAxNodeId(uint32_t fuchsia_node_id, int32_t ax_root_node_id) {
  if (fuchsia_node_id == kFuchsiaRootNodeId)
    return ax_root_node_id;

  if (fuchsia_node_id == kInvalidIdRemappedForFuchsia)
    return ui::AXNode::kInvalidAXID;

  return base::checked_cast<int32_t>(fuchsia_node_id);
}
