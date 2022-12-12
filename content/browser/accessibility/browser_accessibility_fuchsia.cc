// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

#include <lib/ui/scenic/cpp/commands.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace content {

using AXRole = ax::mojom::Role;
using FuchsiaRole = fuchsia::accessibility::semantics::Role;

BrowserAccessibilityFuchsia::BrowserAccessibilityFuchsia(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node)
    : BrowserAccessibility(manager, node) {
  platform_node_ =
      static_cast<ui::AXPlatformNodeFuchsia*>(ui::AXPlatformNode::Create(this));
}

ui::AccessibilityBridgeFuchsia*
BrowserAccessibilityFuchsia::GetAccessibilityBridge() const {
  BrowserAccessibilityManagerFuchsia* manager_fuchsia =
      static_cast<BrowserAccessibilityManagerFuchsia*>(manager());
  DCHECK(manager_fuchsia);

  return manager_fuchsia->GetAccessibilityBridge();
}

// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    ui::AXNode* node) {
  return std::make_unique<BrowserAccessibilityFuchsia>(manager, node);
}

BrowserAccessibilityFuchsia::~BrowserAccessibilityFuchsia() {
  DeleteNode();
  platform_node_->Destroy();
}

uint32_t BrowserAccessibilityFuchsia::GetFuchsiaNodeID() const {
  return static_cast<uint32_t>(GetUniqueId());
}

fuchsia::accessibility::semantics::Node
BrowserAccessibilityFuchsia::ToFuchsiaNodeData() const {
  fuchsia::accessibility::semantics::Node fuchsia_node_data;

  fuchsia_node_data.set_node_id(GetFuchsiaNodeID());
  fuchsia_node_data.set_role(GetFuchsiaRole());
  fuchsia_node_data.set_states(GetFuchsiaStates());
  fuchsia_node_data.set_attributes(GetFuchsiaAttributes());
  fuchsia_node_data.set_actions(GetFuchsiaActions());
  fuchsia_node_data.set_location(GetFuchsiaLocation());
  fuchsia_node_data.set_node_to_container_transform(GetFuchsiaTransform());
  fuchsia_node_data.set_container_id(GetOffsetContainerOrRootNodeID());
  fuchsia_node_data.set_child_ids(GetFuchsiaChildIDs());

  return fuchsia_node_data;
}

void BrowserAccessibilityFuchsia::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  // Declare this node as the fuchsia tree root if it's the root of the main
  // frame's tree.
  if (manager()->IsRootFrameManager() &&
      manager()->GetBrowserAccessibilityRoot() == this) {
    ui::AccessibilityBridgeFuchsia* accessibility_bridge =
        GetAccessibilityBridge();
    if (accessibility_bridge)
      accessibility_bridge->SetRootID(GetUniqueId());
  }

  UpdateNode();
}

void BrowserAccessibilityFuchsia::OnLocationChanged() {
  UpdateNode();
}

BrowserAccessibilityFuchsia* ToBrowserAccessibilityFuchsia(
    BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityFuchsia*>(obj);
}

std::vector<uint32_t> BrowserAccessibilityFuchsia::GetFuchsiaChildIDs() const {
  std::vector<uint32_t> child_ids;

  // TODO(abrusher): Switch back to using platform children.
  for (const auto* child : AllChildren()) {
    const BrowserAccessibilityFuchsia* fuchsia_child =
        static_cast<const BrowserAccessibilityFuchsia*>(child);
    DCHECK(fuchsia_child);

    child_ids.push_back(fuchsia_child->GetFuchsiaNodeID());
  }

  return child_ids;
}

std::vector<fuchsia::accessibility::semantics::Action>
BrowserAccessibilityFuchsia::GetFuchsiaActions() const {
  std::vector<fuchsia::accessibility::semantics::Action> actions;

  if (HasAction(ax::mojom::Action::kDoDefault) ||
      GetData().GetDefaultActionVerb() != ax::mojom::DefaultActionVerb::kNone) {
    actions.push_back(fuchsia::accessibility::semantics::Action::DEFAULT);
  }

  if (HasAction(ax::mojom::Action::kFocus))
    actions.push_back(fuchsia::accessibility::semantics::Action::SET_FOCUS);

  if (HasAction(ax::mojom::Action::kSetValue))
    actions.push_back(fuchsia::accessibility::semantics::Action::SET_VALUE);

  if (HasAction(ax::mojom::Action::kScrollToMakeVisible)) {
    actions.push_back(
        fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN);
  }

  return actions;
}

fuchsia::accessibility::semantics::Role
BrowserAccessibilityFuchsia::GetFuchsiaRole() const {
  auto role = GetRole();

  switch (role) {
    case AXRole::kButton:
      return FuchsiaRole::BUTTON;
    case AXRole::kCell:
      return FuchsiaRole::CELL;
    case AXRole::kCheckBox:
      return FuchsiaRole::CHECK_BOX;
    case AXRole::kColumnHeader:
      return FuchsiaRole::COLUMN_HEADER;
    case AXRole::kGrid:
      return FuchsiaRole::GRID;
    case AXRole::kHeader:
      return FuchsiaRole::HEADER;
    case AXRole::kImage:
      return FuchsiaRole::IMAGE;
    case AXRole::kLink:
      return FuchsiaRole::LINK;
    case AXRole::kList:
      return FuchsiaRole::LIST;
    case AXRole::kListItem:
      return FuchsiaRole::LIST_ELEMENT;
    case AXRole::kListMarker:
      return FuchsiaRole::LIST_ELEMENT_MARKER;
    case AXRole::kParagraph:
      return FuchsiaRole::PARAGRAPH;
    case AXRole::kRadioButton:
      return FuchsiaRole::RADIO_BUTTON;
    case AXRole::kRowGroup:
      return FuchsiaRole::ROW_GROUP;
    case AXRole::kSearchBox:
      return FuchsiaRole::SEARCH_BOX;
    case AXRole::kSlider:
      return FuchsiaRole::SLIDER;
    case AXRole::kStaticText:
      return FuchsiaRole::STATIC_TEXT;
    case AXRole::kTable:
      return FuchsiaRole::TABLE;
    case AXRole::kRow:
      return FuchsiaRole::TABLE_ROW;
    case AXRole::kTextField:
      return FuchsiaRole::TEXT_FIELD;
    case AXRole::kTextFieldWithComboBox:
      return FuchsiaRole::TEXT_FIELD_WITH_COMBO_BOX;
    default:
      return FuchsiaRole::UNKNOWN;
  }
}

fuchsia::accessibility::semantics::States
BrowserAccessibilityFuchsia::GetFuchsiaStates() const {
  fuchsia::accessibility::semantics::States states;

  // Convert checked state.
  if (HasIntAttribute(ax::mojom::IntAttribute::kCheckedState)) {
    ax::mojom::CheckedState ax_state = GetData().GetCheckedState();
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

  // Convert selected state.
  // Indicates whether a node has been selected.
  if (GetData().IsSelectable() &&
      HasBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    states.set_selected(GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  // Indicates if the node is hidden.
  states.set_hidden(IsInvisibleOrIgnored());

  // The user entered value of the node, if applicable.
  if (HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    const std::string& value =
        GetStringAttribute(ax::mojom::StringAttribute::kValue);
    states.set_value(
        value.substr(0, fuchsia::accessibility::semantics::MAX_LABEL_SIZE));
  }

  // The value a range element currently has.
  if (HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    states.set_range_value(
        GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange));
  }

  // The scroll offsets, if the element is a scrollable container.
  const float x_scroll_offset =
      GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  const float y_scroll_offset =
      GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  if (x_scroll_offset || y_scroll_offset)
    states.set_viewport_offset({x_scroll_offset, y_scroll_offset});

  if (IsFocusable())
    states.set_focusable(true);

  states.set_has_input_focus(IsFocused());

  return states;
}

fuchsia::accessibility::semantics::Attributes
BrowserAccessibilityFuchsia::GetFuchsiaAttributes() const {
  fuchsia::accessibility::semantics::Attributes attributes;
  if (HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    const std::string& name =
        GetStringAttribute(ax::mojom::StringAttribute::kName);
    attributes.set_label(
        name.substr(0, fuchsia::accessibility::semantics::MAX_LABEL_SIZE));
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    const std::string& description =
        GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    attributes.set_secondary_label(description.substr(
        0, fuchsia::accessibility::semantics::MAX_LABEL_SIZE));
  }

  if (GetData().IsRangeValueSupported()) {
    fuchsia::accessibility::semantics::RangeAttributes range_attributes;
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange)) {
      range_attributes.set_min_value(
          GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange));
    }
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange)) {
      range_attributes.set_max_value(
          GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange));
    }
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange)) {
      range_attributes.set_step_delta(
          GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange));
    }
    attributes.set_range(std::move(range_attributes));
  }

  if (IsTable()) {
    fuchsia::accessibility::semantics::TableAttributes table_attributes;
    auto col_count = GetTableColCount();
    if (col_count)
      table_attributes.set_number_of_columns(*col_count);

    auto row_count = GetTableRowCount();
    if (row_count)
      table_attributes.set_number_of_rows(*row_count);

    if (!table_attributes.IsEmpty())
      attributes.set_table_attributes(std::move(table_attributes));
  }

  if (IsTableRow()) {
    fuchsia::accessibility::semantics::TableRowAttributes table_row_attributes;
    auto row_index = GetTableRowRowIndex();
    if (row_index) {
      table_row_attributes.set_row_index(*row_index);
      attributes.set_table_row_attributes(std::move(table_row_attributes));
    }
  }

  if (IsTableCellOrHeader()) {
    fuchsia::accessibility::semantics::TableCellAttributes
        table_cell_attributes;

    auto col_index = GetTableCellColIndex();
    if (col_index)
      table_cell_attributes.set_column_index(*col_index);

    auto row_index = GetTableCellRowIndex();
    if (row_index)
      table_cell_attributes.set_row_index(*row_index);

    auto col_span = GetTableCellColSpan();
    if (col_span)
      table_cell_attributes.set_column_span(*col_span);

    auto row_span = GetTableCellRowSpan();
    if (row_span)
      table_cell_attributes.set_row_span(*row_span);

    if (!table_cell_attributes.IsEmpty())
      attributes.set_table_cell_attributes(std::move(table_cell_attributes));
  }

  if (IsList()) {
    absl::optional<int> size = GetSetSize();
    if (size) {
      fuchsia::accessibility::semantics::SetAttributes list_attributes;
      list_attributes.set_size(*size);
      attributes.set_list_attributes(std::move(list_attributes));
    }
  }

  if (IsListElement()) {
    absl::optional<int> index = GetPosInSet();
    if (index) {
      fuchsia::accessibility::semantics::SetAttributes list_element_attributes;
      list_element_attributes.set_index(*index);
      attributes.set_list_element_attributes(
          std::move(list_element_attributes));
    }
  }

  return attributes;
}

fuchsia::ui::gfx::BoundingBox BrowserAccessibilityFuchsia::GetFuchsiaLocation()
    const {
  const gfx::RectF& bounds = GetLocation();

  fuchsia::ui::gfx::BoundingBox box;
  // Since the origin is at the top left, min should represent the top left and
  // max should be the bottom right.
  box.min = scenic::NewVector3({bounds.x(), bounds.y(), 0.0f});
  box.max = scenic::NewVector3({bounds.right(), bounds.bottom(), 0.0f});
  return box;
}

fuchsia::ui::gfx::mat4 BrowserAccessibilityFuchsia::GetFuchsiaTransform()
    const {
  // Get AXNode's explicit transform.
  gfx::Transform transform;
  if (GetData().relative_bounds.transform)
    transform = *GetData().relative_bounds.transform;

  // Convert to fuchsia's transform type.
  std::array<float, 16> mat = {};
  transform.GetColMajorF(mat.data());
  fuchsia::ui::gfx::Matrix4Value fuchsia_transform =
      scenic::NewMatrix4Value(mat);
  return fuchsia_transform.value;
}

uint32_t BrowserAccessibilityFuchsia::GetOffsetContainerOrRootNodeID() const {
  int offset_container_id = GetData().relative_bounds.offset_container_id;

  BrowserAccessibility* offset_container =
      offset_container_id == -1 ? manager()->GetBrowserAccessibilityRoot()
                                : manager()->GetFromID(offset_container_id);

  BrowserAccessibilityFuchsia* fuchsia_container =
      ToBrowserAccessibilityFuchsia(offset_container);

  // TODO(https://crbug.com/1321935): Remove this check once we understand why
  // we're getting non-existent offset container IDs from blink.
  if (!fuchsia_container) {
    ZX_LOG(ERROR, ZX_OK) << "Node " << GetId()
                         << " references non-existent offset container ID "
                         << offset_container_id;
    return 0;
  }

  return fuchsia_container->GetFuchsiaNodeID();
}

void BrowserAccessibilityFuchsia::UpdateNode() {
  if (!GetAccessibilityBridge())
    return;

  GetAccessibilityBridge()->UpdateNode(ToFuchsiaNodeData());
}

void BrowserAccessibilityFuchsia::DeleteNode() {
  if (!GetAccessibilityBridge())
    return;

  GetAccessibilityBridge()->DeleteNode(GetFuchsiaNodeID());
}

bool BrowserAccessibilityFuchsia::IsList() const {
  return GetRole() == AXRole::kList;
}

bool BrowserAccessibilityFuchsia::IsListElement() const {
  return GetRole() == AXRole::kListItem;
}

bool BrowserAccessibilityFuchsia::AccessibilityPerformAction(
    const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kHitTest) {
    BrowserAccessibilityManager* root_manager =
        manager()->GetManagerForRootFrame();
    DCHECK(root_manager);

    ui::AccessibilityBridgeFuchsia* accessibility_bridge =
        GetAccessibilityBridge();
    if (!accessibility_bridge)
      return false;

    root_manager->HitTest(action_data.target_point, action_data.request_id);
    return true;
  }

  return BrowserAccessibility::AccessibilityPerformAction(action_data);
}

}  // namespace content
