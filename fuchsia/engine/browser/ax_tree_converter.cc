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

#include "base/numerics/safe_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

using AXRole = ax::mojom::Role;
using fuchsia::accessibility::semantics::MAX_LABEL_SIZE;
using fuchsia::accessibility::semantics::Role;

// Fuchsia's default root node ID.
constexpr uint32_t kFuchsiaRootNodeId = 0;

fuchsia::accessibility::semantics::Attributes ConvertAttributes(
    const ui::AXNode& node) {
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

  if (node.data().IsRangeValueSupported()) {
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

  if (node.IsTable()) {
    fuchsia::accessibility::semantics::TableAttributes table_attributes;
    auto col_count = node.GetTableColCount();
    if (col_count)
      table_attributes.set_number_of_columns(*col_count);

    auto row_count = node.GetTableRowCount();
    if (row_count)
      table_attributes.set_number_of_rows(*row_count);

    if (!table_attributes.IsEmpty())
      attributes.set_table_attributes(std::move(table_attributes));
  }

  if (node.IsTableRow()) {
    fuchsia::accessibility::semantics::TableRowAttributes table_row_attributes;
    auto row_index = node.GetTableRowRowIndex();
    if (row_index) {
      table_row_attributes.set_row_index(*row_index);
      attributes.set_table_row_attributes(std::move(table_row_attributes));
    }
  }

  if (node.IsTableCellOrHeader()) {
    fuchsia::accessibility::semantics::TableCellAttributes
        table_cell_attributes;

    auto col_index = node.GetTableCellColIndex();
    if (col_index)
      table_cell_attributes.set_column_index(*col_index);

    auto row_index = node.GetTableCellRowIndex();
    if (row_index)
      table_cell_attributes.set_row_index(*row_index);

    auto col_span = node.GetTableCellColSpan();
    if (col_span)
      table_cell_attributes.set_column_span(*col_span);

    auto row_span = node.GetTableCellRowSpan();
    if (row_span)
      table_cell_attributes.set_row_span(*row_span);

    if (!table_cell_attributes.IsEmpty())
      attributes.set_table_cell_attributes(std::move(table_cell_attributes));
  }

  return attributes;
}

// Converts an AXRole to a Fuchsia Role.
Role AxRoleToFuchsiaSemanticRole(AXRole role) {
  switch (role) {
    case AXRole::kButton:
      return Role::BUTTON;
    case AXRole::kCheckBox:
      return Role::CHECK_BOX;
    case AXRole::kHeader:
      return Role::HEADER;
    case AXRole::kImage:
      return Role::IMAGE;
    case AXRole::kLink:
      return Role::LINK;
    case AXRole::kRadioButton:
      return Role::RADIO_BUTTON;
    case AXRole::kSlider:
      return Role::SLIDER;
    case AXRole::kTextField:
      return Role::TEXT_FIELD;
    case AXRole::kStaticText:
      return Role::STATIC_TEXT;
    case AXRole::kSearchBox:
      return Role::SEARCH_BOX;
    case AXRole::kTextFieldWithComboBox:
      return Role::TEXT_FIELD_WITH_COMBO_BOX;
    case AXRole::kTable:
      return Role::TABLE;
    case AXRole::kGrid:
      return Role::GRID;
    case AXRole::kRow:
      return Role::TABLE_ROW;
    case AXRole::kCell:
      return Role::CELL;
    case AXRole::kColumnHeader:
      return Role::COLUMN_HEADER;
    case AXRole::kRowGroup:
      return Role::ROW_GROUP;
    case AXRole::kParagraph:
      return Role::PARAGRAPH;
    default:
      return Role::UNKNOWN;
  }
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
  states.set_hidden(node.IsIgnored() || node.IsInvisible());

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

  // The scroll offsets, if the element is a scrollable container.
  const float x_scroll_offset =
      node.GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  const float y_scroll_offset =
      node.GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  if (x_scroll_offset || y_scroll_offset)
    states.set_viewport_offset({x_scroll_offset, y_scroll_offset});

  if (node.HasState(ax::mojom::State::kFocusable))
    states.set_focusable(true);

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

std::vector<uint32_t> ConvertChildIds(std::vector<int32_t> ids,
                                      const ui::AXTreeID& tree_id,
                                      NodeIDMapper* id_mapper) {
  std::vector<uint32_t> child_ids;
  child_ids.reserve(ids.size());
  for (const auto& i : ids) {
    child_ids.push_back(id_mapper->ToFuchsiaNodeID(tree_id, i, false));
  }
  return child_ids;
}

fuchsia::ui::gfx::BoundingBox ConvertBoundingBox(gfx::RectF bounds) {
  fuchsia::ui::gfx::BoundingBox box;
  // Since the origin is at the top left, min should represent the top left and
  // max should be the bottom right.
  box.min = scenic::NewVector3({bounds.x(), bounds.y(), 0.0f});
  box.max = scenic::NewVector3({bounds.right(), bounds.bottom(), 0.0f});
  return box;
}

// The Semantics Manager applies this matrix to position the node and its
// subtree as an optimization to handle resizing or repositioning. This requires
// only one node to be updated on such an event.
fuchsia::ui::gfx::mat4 ConvertTransform(gfx::Transform* transform) {
  std::array<float, 16> mat = {};
  transform->matrix().getColMajor(mat.data());
  fuchsia::ui::gfx::Matrix4Value fuchsia_transform =
      scenic::NewMatrix4Value(mat);
  return fuchsia_transform.value;
}

}  // namespace

fuchsia::accessibility::semantics::Node AXNodeDataToSemanticNode(
    const ui::AXNode& ax_node,
    const ui::AXNode& container_node,
    const ui::AXTreeID& tree_id,
    bool is_root,
    float device_scale_factor,
    NodeIDMapper* id_mapper) {
  fuchsia::accessibility::semantics::Node fuchsia_node;
  fuchsia_node.set_node_id(
      id_mapper->ToFuchsiaNodeID(tree_id, ax_node.id(), is_root));
  const ui::AXNodeData& node = ax_node.data();
  fuchsia_node.set_role(AxRoleToFuchsiaSemanticRole(node.role));
  fuchsia_node.set_states(ConvertStates(node));
  fuchsia_node.set_attributes(ConvertAttributes(ax_node));
  fuchsia_node.set_actions(ConvertActions(node));
  fuchsia_node.set_child_ids(
      ConvertChildIds(node.child_ids, tree_id, id_mapper));
  fuchsia_node.set_location(ConvertBoundingBox(node.relative_bounds.bounds));
  fuchsia_node.set_container_id(
      id_mapper->ToFuchsiaNodeID(tree_id, container_node.data().id, false));

  // The transform field must be handled carefully to account for
  // the offsetting implied by the offset container's relative bounds.
  gfx::Transform transform;
  if (node.relative_bounds.transform) {
    transform = *node.relative_bounds.transform;
  }
  transform.PostTranslate(container_node.data().relative_bounds.bounds.x(),
                          container_node.data().relative_bounds.bounds.y());
  if (device_scale_factor > 0) {
    transform.PostScale(1 / device_scale_factor, 1 / device_scale_factor);
  }

  fuchsia_node.set_transform(ConvertTransform(&transform));

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

NodeIDMapper::NodeIDMapper()
    : root_(std::make_pair(ui::AXTreeIDUnknown(), 0)) {}

NodeIDMapper::~NodeIDMapper() = default;

uint32_t NodeIDMapper::ToFuchsiaNodeID(const ui::AXTreeID& ax_tree_id,
                                       int32_t ax_node_id,
                                       bool is_tree_root) {
  const bool should_change_root =
      is_tree_root && (root_.first != ax_tree_id || root_.second != ax_node_id);

  CHECK_LE(next_fuchsia_id_, UINT32_MAX);
  uint32_t fuchsia_node_id;
  if (should_change_root) {
    // The node that points to the root is changing. Update the old root to
    // receive an unique ID, and make the new root receive the default value.
    if (root_.first != ui::AXTreeIDUnknown())
      id_map_[root_.first][root_.second] = next_fuchsia_id_++;
    root_ = std::make_pair(ax_tree_id, ax_node_id);
    fuchsia_node_id = kFuchsiaRootNodeId;
  } else {
    auto it = id_map_.find(ax_tree_id);
    if (it != id_map_.end()) {
      auto node_id_it = it->second.find(ax_node_id);
      if (node_id_it != it->second.end())
        return node_id_it->second;
    }

    // The ID is not in the map yet, so give it a new value.
    fuchsia_node_id = next_fuchsia_id_++;
  }

  id_map_[ax_tree_id][ax_node_id] = fuchsia_node_id;
  return fuchsia_node_id;
}

absl::optional<std::pair<ui::AXTreeID, int32_t>> NodeIDMapper::ToAXNodeID(
    uint32_t fuchsia_node_id) {
  for (const auto& tree_id_to_node_ids : id_map_) {
    for (const auto& ax_id_to_fuchsia_id : tree_id_to_node_ids.second) {
      if (ax_id_to_fuchsia_id.second == fuchsia_node_id)
        return std::make_pair(tree_id_to_node_ids.first,
                              ax_id_to_fuchsia_id.first);
    }
  }

  return absl::nullopt;
}

bool NodeIDMapper::UpdateAXTreeIDForCachedNodeIDs(
    const ui::AXTreeID& old_ax_tree_id,
    const ui::AXTreeID& new_ax_tree_id) {
  if (old_ax_tree_id == new_ax_tree_id)
    return false;

  auto it = id_map_.find(old_ax_tree_id);
  if (it == id_map_.end())
    return false;

  // The iterator is not stable, so the order of operations here is important.
  auto data = std::move(it->second);
  id_map_.erase(it);
  id_map_[new_ax_tree_id] = std::move(data);
  return true;
}
