// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

namespace content {
namespace {

using FuchsiaAction = fuchsia::accessibility::semantics::Action;
using FuchsiaCheckedState = fuchsia::accessibility::semantics::CheckedState;
using FuchsiaRole = fuchsia::accessibility::semantics::Role;
using FuchsiaToggledState = fuchsia::accessibility::semantics::ToggledState;

constexpr const char* const kBoolAttributes[] = {
    "hidden", "focusable", "has_input_focus", "is_keyboard_key", "selected",
};

constexpr const char* const kStringAttributes[] = {
    "label",           "actions",       "secondary_label",
    "value",           "checked_state", "toggled_state",
    "viewport_offset", "location",      "transform",
};

constexpr const char* const kIntAttributes[] = {
    "number_of_rows",   "number_of_columns", "row_index",
    "cell_row_index",   "cell_column_index", "cell_row_span",
    "cell_column_span", "list_size",         "list_element_index"};

constexpr const char* const kDoubleAttributes[] = {
    "min_value",
    "max_value",
    "step_delta",
};

std::string FuchsiaRoleToString(const FuchsiaRole role) {
  switch (role) {
    case FuchsiaRole::BUTTON:
      return "BUTTON";
    case FuchsiaRole::CELL:
      return "CELL";
    case FuchsiaRole::CHECK_BOX:
      return "CHECK_BOX";
    case FuchsiaRole::COLUMN_HEADER:
      return "COLUMN_HEADER";
    case FuchsiaRole::GRID:
      return "GRID";
    case FuchsiaRole::HEADER:
      return "HEADER";
    case FuchsiaRole::IMAGE:
      return "IMAGE";
    case FuchsiaRole::LINK:
      return "LINK";
    case FuchsiaRole::LIST:
      return "LIST";
    case FuchsiaRole::LIST_ELEMENT:
      return "LIST_ELEMENT";
    case FuchsiaRole::LIST_ELEMENT_MARKER:
      return "LIST_ELEMENT_MARKER";
    case FuchsiaRole::PARAGRAPH:
      return "PARAGRAPH";
    case FuchsiaRole::RADIO_BUTTON:
      return "RADIO_BUTTON";
    case FuchsiaRole::ROW_GROUP:
      return "ROW_GROUP";
    case FuchsiaRole::ROW_HEADER:
      return "ROW_HEADER";
    case FuchsiaRole::SEARCH_BOX:
      return "SEARCH_BOX";
    case FuchsiaRole::SLIDER:
      return "SLIDER";
    case FuchsiaRole::STATIC_TEXT:
      return "STATIC_TEXT";
    case FuchsiaRole::TABLE:
      return "TABLE";
    case FuchsiaRole::TABLE_ROW:
      return "TABLE_ROW";
    case FuchsiaRole::TEXT_FIELD:
      return "TEXT_FIELD";
    case FuchsiaRole::TEXT_FIELD_WITH_COMBO_BOX:
      return "TEXT_FIELD_WITH_COMBO_BOX";
    case FuchsiaRole::TOGGLE_SWITCH:
      return "TOGGLE_SWITCH";
    case FuchsiaRole::UNKNOWN:
      return "UNKNOWN";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string FuchsiaActionToString(FuchsiaAction action) {
  switch (action) {
    case FuchsiaAction::DEFAULT:
      return "DEFAULT";
    case FuchsiaAction::DECREMENT:
      return "DECREMENT";
    case FuchsiaAction::INCREMENT:
      return "INCREMENT";
    case FuchsiaAction::SECONDARY:
      return "SECONDARY";
    case FuchsiaAction::SET_FOCUS:
      return "SET_FOCUS";
    case FuchsiaAction::SET_VALUE:
      return "SET_VALUE";
    case FuchsiaAction::SHOW_ON_SCREEN:
      return "SHOW_ON_SCREEN";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string FuchsiaActionsToString(const std::vector<FuchsiaAction>& actions) {
  std::vector<std::string> fuchsia_actions;
  for (const auto& action : actions) {
    fuchsia_actions.push_back(FuchsiaActionToString(action));
  }

  if (fuchsia_actions.empty())
    return std::string();

  return "{" + base::JoinString(fuchsia_actions, ", ") + "}";
}

std::string CheckedStateToString(const FuchsiaCheckedState checked_state) {
  switch (checked_state) {
    case FuchsiaCheckedState::NONE:
      return "NONE";
    case FuchsiaCheckedState::CHECKED:
      return "CHECKED";
    case FuchsiaCheckedState::UNCHECKED:
      return "UNCHECKED";
    case FuchsiaCheckedState::MIXED:
      return "MIXED";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string ToggledStateToString(const FuchsiaToggledState toggled_state) {
  switch (toggled_state) {
    case FuchsiaToggledState::ON:
      return "ON";
    case FuchsiaToggledState::OFF:
      return "OFF";
    case FuchsiaToggledState::INDETERMINATE:
      return "INDETERMINATE";
    default:
      NOTREACHED();
      return std::string();
  }
}

std::string ViewportOffsetToString(
    const fuchsia::ui::gfx::vec2& viewport_offset) {
  return base::StringPrintf("(%.1f, %.1f)", viewport_offset.x,
                            viewport_offset.y);
}

std::string Vec3ToString(const fuchsia::ui::gfx::vec3& vec) {
  return base::StringPrintf("(%.1f, %.1f, %.1f)", vec.x, vec.y, vec.z);
}

std::string Mat4ToString(const fuchsia::ui::gfx::mat4& mat) {
  std::string retval = "{ ";
  for (int i = 0; i < 4; i++) {
    retval.append(base::StringPrintf(
        "col%d: (%.1f,%.1f,%.1f,%.1f), ", i, mat.matrix[i * 4],
        mat.matrix[i * 4 + 1], mat.matrix[i * 4 + 2], mat.matrix[i * 4 + 3]));
  }
  return retval.append(" }");
}

std::string LocationToString(const fuchsia::ui::gfx::BoundingBox& location) {
  return base::StringPrintf("{ min: %s, max: %s }",
                            Vec3ToString(location.min).c_str(),
                            Vec3ToString(location.max).c_str());
}

}  // namespace

AccessibilityTreeFormatterFuchsia::AccessibilityTreeFormatterFuchsia() =
    default;
AccessibilityTreeFormatterFuchsia::~AccessibilityTreeFormatterFuchsia() =
    default;

void AccessibilityTreeFormatterFuchsia::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  // Exclude spatial semantics by default to avoid flakiness.
  AddPropertyFilter(property_filters, "location", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "transform", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "viewport_offset",
                    AXPropertyFilter::DENY);
}

base::Value::Dict AccessibilityTreeFormatterFuchsia::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  if (!root) {
    return base::Value::Dict();
  }

  base::Value::Dict dict;
  RecursiveBuildTree(*root, &dict);
  return dict;
}

void AccessibilityTreeFormatterFuchsia::RecursiveBuildTree(
    const ui::AXPlatformNodeDelegate& node,
    base::Value::Dict* dict) const {
  if (!ShouldDumpNode(node))
    return;

  AddProperties(node, dict);
  if (!ShouldDumpChildren(node))
    return;

  base::Value::List children;

  fuchsia::accessibility::semantics::Node fuchsia_node =
      static_cast<const BrowserAccessibilityFuchsia&>(node).ToFuchsiaNodeData();

  for (uint32_t child_id : fuchsia_node.child_ids()) {
    ui::AXPlatformNodeFuchsia* child_node =
        static_cast<ui::AXPlatformNodeFuchsia*>(
            ui::AXPlatformNodeBase::GetFromUniqueId(child_id));
    DCHECK(child_node);

    ui::AXPlatformNodeDelegate* child_delegate = child_node->GetDelegate();

    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_delegate, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

base::Value::Dict AccessibilityTreeFormatterFuchsia::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::Value::Dict dict;
  AddProperties(*node, &dict);
  return dict;
}

void AccessibilityTreeFormatterFuchsia::AddProperties(
    const ui::AXPlatformNodeDelegate& node,
    base::Value::Dict* dict) const {
  dict->Set("id", node.GetId());

  const BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      static_cast<const BrowserAccessibilityFuchsia*>(&node);

  CHECK(browser_accessibility_fuchsia);

  const fuchsia::accessibility::semantics::Node& fuchsia_node =
      browser_accessibility_fuchsia->ToFuchsiaNodeData();

  // Add fuchsia node attributes.
  dict->Set("role", FuchsiaRoleToString(fuchsia_node.role()));

  dict->Set("actions", FuchsiaActionsToString(fuchsia_node.actions()));

  if (fuchsia_node.has_attributes()) {
    const fuchsia::accessibility::semantics::Attributes& attributes =
        fuchsia_node.attributes();

    if (attributes.has_label() && !attributes.label().empty())
      dict->Set("label", attributes.label());

    if (attributes.has_secondary_label() &&
        !attributes.secondary_label().empty()) {
      dict->Set("secondary_label", attributes.secondary_label());
    }

    if (attributes.has_range()) {
      const auto& range_attributes = attributes.range();

      if (range_attributes.has_min_value())
        dict->Set("min_value", range_attributes.min_value());

      if (range_attributes.has_max_value())
        dict->Set("max_value", range_attributes.max_value());

      if (range_attributes.has_step_delta())
        dict->Set("step_delta", range_attributes.step_delta());
    }

    if (attributes.has_table_attributes()) {
      const auto& table_attributes = attributes.table_attributes();

      if (table_attributes.has_number_of_rows())
        dict->Set("number_of_rows",
                  static_cast<int>(table_attributes.number_of_rows()));

      if (table_attributes.has_number_of_columns()) {
        dict->Set("number_of_columns",
                  static_cast<int>(table_attributes.number_of_columns()));
      }
    }

    if (attributes.has_table_row_attributes()) {
      const auto& table_row_attributes = attributes.table_row_attributes();

      if (table_row_attributes.has_row_index())
        dict->Set("row_index",
                  static_cast<int>(table_row_attributes.row_index()));
    }

    if (attributes.has_table_cell_attributes()) {
      const auto& table_cell_attributes = attributes.table_cell_attributes();

      if (table_cell_attributes.has_row_index())
        dict->Set("cell_row_index",
                  static_cast<int>(table_cell_attributes.row_index()));

      if (table_cell_attributes.has_column_index()) {
        dict->Set("cell_column_index",
                  static_cast<int>(table_cell_attributes.column_index()));
      }

      if (table_cell_attributes.has_row_span())
        dict->Set("cell_row_span",
                  static_cast<int>(table_cell_attributes.row_span()));

      if (table_cell_attributes.has_column_span()) {
        dict->Set("cell_column_span",
                  static_cast<int>(table_cell_attributes.column_span()));
      }
    }

    if (attributes.has_list_attributes()) {
      dict->Set("list_size",
                static_cast<int>(attributes.list_attributes().size()));
    }

    if (attributes.has_list_element_attributes()) {
      dict->Set("list_element_index",
                static_cast<int>(attributes.list_element_attributes().index()));
    }

    if (attributes.has_is_keyboard_key())
      dict->Set("is_keyboard_key", attributes.is_keyboard_key());
  }

  if (fuchsia_node.has_states()) {
    const fuchsia::accessibility::semantics::States& states =
        fuchsia_node.states();

    if (states.has_selected())
      dict->Set("selected", states.selected());

    if (states.has_checked_state()) {
      dict->Set("checked_state", CheckedStateToString(states.checked_state()));
    }

    if (states.has_hidden())
      dict->Set("hidden", states.hidden());

    if (states.has_value() && !states.value().empty())
      dict->Set("value", states.value());

    if (states.has_viewport_offset()) {
      dict->Set("viewport_offset",
                ViewportOffsetToString(states.viewport_offset()));
    }

    if (states.has_toggled_state()) {
      dict->Set("toggled_state", ToggledStateToString(states.toggled_state()));
    }

    if (states.has_focusable())
      dict->Set("focusable", states.focusable());

    if (states.has_has_input_focus())
      dict->Set("has_input_focus", states.has_input_focus());
  }

  if (fuchsia_node.has_location())
    dict->Set("location", LocationToString(fuchsia_node.location()));

  if (fuchsia_node.has_transform()) {
    dict->Set("transform",
              Mat4ToString(fuchsia_node.node_to_container_transform()));
  }
}

std::string AccessibilityTreeFormatterFuchsia::ProcessTreeForOutput(
    const base::Value::Dict& node) const {
  if (const std::string* error_value = node.FindString("error")) {
    return *error_value;
  }

  std::string line;

  if (show_ids()) {
    int id_value = node.FindInt("id").value_or(0);
    WriteAttribute(true, base::NumberToString(id_value), &line);
  }

  if (const std::string* role_value = node.FindString("role")) {
    WriteAttribute(true, *role_value, &line);
  }

  for (const char* bool_attribute : kBoolAttributes) {
    if (node.FindBool(bool_attribute).value_or(false))
      WriteAttribute(/*include_by_default=*/true, bool_attribute, &line);
  }

  for (const char* string_attribute : kStringAttributes) {
    const std::string* value = node.FindString(string_attribute);
    if (!value || value->empty())
      continue;

    WriteAttribute(
        /*include_by_default=*/true,
        base::StringPrintf("%s='%s'", string_attribute, value->c_str()), &line);
  }

  for (const char* attribute_name : kIntAttributes) {
    int value = node.FindInt(attribute_name).value_or(0);
    if (value == 0)
      continue;
    WriteAttribute(true, base::StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  for (const char* attribute_name : kDoubleAttributes) {
    int value = node.FindInt(attribute_name).value_or(0);
    if (value == 0)
      continue;
    WriteAttribute(true, base::StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  return line;
}

base::Value::Dict AccessibilityTreeFormatterFuchsia::BuildTreeForSelector(
    const AXTreeSelector&) const {
  NOTIMPLEMENTED();
  return base::Value::Dict();
}

}  // namespace content
