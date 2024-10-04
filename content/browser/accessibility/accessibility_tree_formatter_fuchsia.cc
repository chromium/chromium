// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/accessibility/platform/fuchsia/browser_accessibility_fuchsia.h"

namespace content {
namespace {

using FuchsiaAction = fuchsia_accessibility_semantics::Action;
using FuchsiaCheckedState = fuchsia_accessibility_semantics::CheckedState;
using FuchsiaRole = fuchsia_accessibility_semantics::Role;
using FuchsiaToggledState = fuchsia_accessibility_semantics::ToggledState;

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
    case FuchsiaRole::kButton:
      return "BUTTON";
    case FuchsiaRole::kCell:
      return "CELL";
    case FuchsiaRole::kCheckBox:
      return "CHECK_BOX";
    case FuchsiaRole::kColumnHeader:
      return "COLUMN_HEADER";
    case FuchsiaRole::kGrid:
      return "GRID";
    case FuchsiaRole::kHeader:
      return "HEADER";
    case FuchsiaRole::kImage:
      return "IMAGE";
    case FuchsiaRole::kLink:
      return "LINK";
    case FuchsiaRole::kList:
      return "LIST";
    case FuchsiaRole::kListElement:
      return "LIST_ELEMENT";
    case FuchsiaRole::kListElementMarker:
      return "LIST_ELEMENT_MARKER";
    case FuchsiaRole::kParagraph:
      return "PARAGRAPH";
    case FuchsiaRole::kRadioButton:
      return "RADIO_BUTTON";
    case FuchsiaRole::kRowGroup:
      return "ROW_GROUP";
    case FuchsiaRole::kRowHeader:
      return "ROW_HEADER";
    case FuchsiaRole::kSearchBox:
      return "SEARCH_BOX";
    case FuchsiaRole::kSlider:
      return "SLIDER";
    case FuchsiaRole::kStaticText:
      return "STATIC_TEXT";
    case FuchsiaRole::kTable:
      return "TABLE";
    case FuchsiaRole::kTableRow:
      return "TABLE_ROW";
    case FuchsiaRole::kTextField:
      return "TEXT_FIELD";
    case FuchsiaRole::kTextFieldWithComboBox:
      return "TEXT_FIELD_WITH_COMBO_BOX";
    case FuchsiaRole::kToggleSwitch:
      return "TOGGLE_SWITCH";
    case FuchsiaRole::kUnknown:
      return "UNKNOWN";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

std::string FuchsiaActionToString(FuchsiaAction action) {
  switch (action) {
    case FuchsiaAction::kDefault:
      return "DEFAULT";
    case FuchsiaAction::kDecrement:
      return "DECREMENT";
    case FuchsiaAction::kIncrement:
      return "INCREMENT";
    case FuchsiaAction::kSecondary:
      return "SECONDARY";
    case FuchsiaAction::kSetFocus:
      return "SET_FOCUS";
    case FuchsiaAction::kSetValue:
      return "SET_VALUE";
    case FuchsiaAction::kShowOnScreen:
      return "SHOW_ON_SCREEN";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

std::string FuchsiaActionsToString(const std::vector<FuchsiaAction>& actions) {
  std::vector<std::string> fuchsia_actions;
  for (const auto& action : actions) {
    fuchsia_actions.push_back(FuchsiaActionToString(action));
  }

  if (fuchsia_actions.empty()) {
    return std::string();
  }

  return "{" + base::JoinString(fuchsia_actions, ", ") + "}";
}

std::string CheckedStateToString(const FuchsiaCheckedState checked_state) {
  switch (checked_state) {
    case FuchsiaCheckedState::kNone:
      return "NONE";
    case FuchsiaCheckedState::kChecked:
      return "CHECKED";
    case FuchsiaCheckedState::kUnchecked:
      return "UNCHECKED";
    case FuchsiaCheckedState::kMixed:
      return "MIXED";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

std::string ToggledStateToString(const FuchsiaToggledState toggled_state) {
  switch (toggled_state) {
    case FuchsiaToggledState::kOn:
      return "ON";
    case FuchsiaToggledState::kOff:
      return "OFF";
    case FuchsiaToggledState::kIndeterminate:
      return "INDETERMINATE";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

std::string ViewportOffsetToString(
    const fuchsia_ui_gfx::Vec2& viewport_offset) {
  return base::StringPrintf("(%.1f, %.1f)", viewport_offset.x(),
                            viewport_offset.y());
}

std::string Vec3ToString(const fuchsia_ui_gfx::Vec3& vec) {
  return base::StringPrintf("(%.1f, %.1f, %.1f)", vec.x(), vec.y(), vec.z());
}

std::string Mat4ToString(const fuchsia_ui_gfx::Mat4& mat) {
  std::string retval = "{ ";
  for (int i = 0; i < 4; i++) {
    retval.append(
        base::StringPrintf("col%d: (%.1f,%.1f,%.1f,%.1f), ", i,
                           mat.matrix()[i * 4], mat.matrix()[i * 4 + 1],
                           mat.matrix()[i * 4 + 2], mat.matrix()[i * 4 + 3]));
  }
  return retval.append(" }");
}

std::string LocationToString(const fuchsia_ui_gfx::BoundingBox& location) {
  return base::StringPrintf("{ min: %s, max: %s }",
                            Vec3ToString(location.min()).c_str(),
                            Vec3ToString(location.max()).c_str());
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
  if (!ShouldDumpNode(node)) {
    return;
  }

  AddProperties(node, dict);
  if (!ShouldDumpChildren(node)) {
    return;
  }

  base::Value::List children;

  fuchsia_accessibility_semantics::Node fuchsia_node =
      static_cast<const ui::BrowserAccessibilityFuchsia&>(node)
          .ToFuchsiaNodeData();

  for (uint32_t child_id : fuchsia_node.child_ids().value()) {
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

  const ui::BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      static_cast<const ui::BrowserAccessibilityFuchsia*>(&node);

  CHECK(browser_accessibility_fuchsia);

  const fuchsia_accessibility_semantics::Node& fuchsia_node =
      browser_accessibility_fuchsia->ToFuchsiaNodeData();

  // Add fuchsia node attributes.
  dict->Set("role", FuchsiaRoleToString(fuchsia_node.role().value()));

  dict->Set("actions", FuchsiaActionsToString(fuchsia_node.actions().value()));

  if (fuchsia_node.attributes()) {
    const fuchsia_accessibility_semantics::Attributes& attributes =
        fuchsia_node.attributes().value();

    if (attributes.label() && !attributes.label()->empty()) {
      dict->Set("label", attributes.label().value());
    }

    if (attributes.secondary_label() &&
        !attributes.secondary_label()->empty()) {
      dict->Set("secondary_label", attributes.secondary_label().value());
    }

    if (attributes.range()) {
      const auto& range_attributes = attributes.range().value();

      if (range_attributes.min_value()) {
        dict->Set("min_value", range_attributes.min_value().value());
      }

      if (range_attributes.max_value()) {
        dict->Set("max_value", range_attributes.max_value().value());
      }

      if (range_attributes.step_delta()) {
        dict->Set("step_delta", range_attributes.step_delta().value());
      }
    }

    if (attributes.table_attributes()) {
      const auto& table_attributes = attributes.table_attributes().value();

      if (table_attributes.number_of_rows()) {
        dict->Set("number_of_rows",
                  static_cast<int>(table_attributes.number_of_rows().value()));
      }

      if (table_attributes.number_of_columns()) {
        dict->Set(
            "number_of_columns",
            static_cast<int>(table_attributes.number_of_columns().value()));
      }
    }

    if (attributes.table_row_attributes()) {
      const auto& table_row_attributes =
          attributes.table_row_attributes().value();

      if (table_row_attributes.row_index()) {
        dict->Set("row_index",
                  static_cast<int>(table_row_attributes.row_index().value()));
      }
    }

    if (attributes.table_cell_attributes()) {
      const auto& table_cell_attributes =
          attributes.table_cell_attributes().value();

      if (table_cell_attributes.row_index()) {
        dict->Set("cell_row_index",
                  static_cast<int>(table_cell_attributes.row_index().value()));
      }

      if (table_cell_attributes.column_index()) {
        dict->Set(
            "cell_column_index",
            static_cast<int>(table_cell_attributes.column_index().value()));
      }

      if (table_cell_attributes.row_span()) {
        dict->Set("cell_row_span",
                  static_cast<int>(table_cell_attributes.row_span().value()));
      }

      if (table_cell_attributes.column_span()) {
        dict->Set(
            "cell_column_span",
            static_cast<int>(table_cell_attributes.column_span().value()));
      }
    }

    if (attributes.list_attributes() && attributes.list_attributes()->size()) {
      dict->Set("list_size",
                static_cast<int>(attributes.list_attributes()->size().value()));
    }

    if (attributes.list_element_attributes() &&
        attributes.list_element_attributes()->index()) {
      dict->Set("list_element_index",
                static_cast<int>(
                    attributes.list_element_attributes()->index().value()));
    }

    if (attributes.is_keyboard_key()) {
      dict->Set("is_keyboard_key", attributes.is_keyboard_key().value());
    }
  }

  if (fuchsia_node.states()) {
    const fuchsia_accessibility_semantics::States& states =
        fuchsia_node.states().value();

    if (states.selected()) {
      dict->Set("selected", states.selected().value());
    }

    if (states.checked_state()) {
      dict->Set("checked_state",
                CheckedStateToString(states.checked_state().value()));
    }

    if (states.hidden()) {
      dict->Set("hidden", states.hidden().value());
    }

    if (states.value() && !states.value()->empty()) {
      dict->Set("value", states.value().value());
    }

    if (states.viewport_offset()) {
      dict->Set("viewport_offset",
                ViewportOffsetToString(states.viewport_offset().value()));
    }

    if (states.toggled_state()) {
      dict->Set("toggled_state",
                ToggledStateToString(states.toggled_state().value()));
    }

    if (states.focusable()) {
      dict->Set("focusable", states.focusable().value());
    }

    if (states.has_input_focus()) {
      dict->Set("has_input_focus", states.has_input_focus().value());
    }
  }

  if (fuchsia_node.location()) {
    dict->Set("location", LocationToString(fuchsia_node.location().value()));
  }

  if (fuchsia_node.transform()) {
    dict->Set("transform",
              Mat4ToString(fuchsia_node.node_to_container_transform().value()));
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
    if (node.FindBool(bool_attribute).value_or(false)) {
      WriteAttribute(/*include_by_default=*/true, bool_attribute, &line);
    }
  }

  for (const char* string_attribute : kStringAttributes) {
    const std::string* value = node.FindString(string_attribute);
    if (!value || value->empty()) {
      continue;
    }

    WriteAttribute(
        /*include_by_default=*/true,
        base::StringPrintf("%s='%s'", string_attribute, value->c_str()), &line);
  }

  for (const char* attribute_name : kIntAttributes) {
    int value = node.FindInt(attribute_name).value_or(0);
    if (value == 0) {
      continue;
    }
    WriteAttribute(true, base::StringPrintf("%s=%d", attribute_name, value),
                   &line);
  }

  for (const char* attribute_name : kDoubleAttributes) {
    int value = node.FindInt(attribute_name).value_or(0);
    if (value == 0) {
      continue;
    }
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
