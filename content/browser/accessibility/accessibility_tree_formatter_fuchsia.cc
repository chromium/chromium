// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_fuchsia.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"

namespace content {
namespace {

constexpr const char* const kBoolAttributes[] = {
    "hidden",
    "focusable",
    "has_input_focus",
    "is_keyboard_key",
};

constexpr const char* const kStringAttributes[] = {
    "label",
    "location",
    "transform",
};

std::string FuchsiaRoleToString(
    const fuchsia::accessibility::semantics::Role role) {
  switch (role) {
    case fuchsia::accessibility::semantics::Role::BUTTON:
      return "BUTTON";
    case fuchsia::accessibility::semantics::Role::CELL:
      return "CELL";
    case fuchsia::accessibility::semantics::Role::CHECK_BOX:
      return "CHECK_BOX";
    case fuchsia::accessibility::semantics::Role::COLUMN_HEADER:
      return "COLUMN_HEADER";
    case fuchsia::accessibility::semantics::Role::GRID:
      return "GRID";
    case fuchsia::accessibility::semantics::Role::HEADER:
      return "HEADER";
    case fuchsia::accessibility::semantics::Role::IMAGE:
      return "IMAGE";
    case fuchsia::accessibility::semantics::Role::LINK:
      return "LINK";
    case fuchsia::accessibility::semantics::Role::LIST:
      return "LIST";
    case fuchsia::accessibility::semantics::Role::LIST_ELEMENT_MARKER:
      return "LIST_ELEMENT_MARKER";
    case fuchsia::accessibility::semantics::Role::PARAGRAPH:
      return "PARAGRAPH";
    case fuchsia::accessibility::semantics::Role::RADIO_BUTTON:
      return "RADIO_BUTTON";
    case fuchsia::accessibility::semantics::Role::ROW_GROUP:
      return "ROW_GROUP";
    case fuchsia::accessibility::semantics::Role::ROW_HEADER:
      return "ROW_HEADER";
    case fuchsia::accessibility::semantics::Role::SEARCH_BOX:
      return "SEARCH_BOX";
    case fuchsia::accessibility::semantics::Role::SLIDER:
      return "SLIDER";
    case fuchsia::accessibility::semantics::Role::STATIC_TEXT:
      return "STATIC_TEXT";
    case fuchsia::accessibility::semantics::Role::TABLE:
      return "TABLE";
    case fuchsia::accessibility::semantics::Role::TABLE_ROW:
      return "TABLE_ROW";
    case fuchsia::accessibility::semantics::Role::TEXT_FIELD:
      return "TEXT_FIELD";
    case fuchsia::accessibility::semantics::Role::TEXT_FIELD_WITH_COMBO_BOX:
      return "TEXT_FIELD_WITH_COMBO_BOX";
    case fuchsia::accessibility::semantics::Role::TOGGLE_SWITCH:
      return "TOGGLE_SWITCH";
    case fuchsia::accessibility::semantics::Role::UNKNOWN:
      return "UNKNOWN";
    default:
      return std::string();
  }
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

base::Value AccessibilityTreeFormatterFuchsia::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  CHECK(root);

  BrowserAccessibility* root_internal =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);

  base::DictionaryValue dict;
  RecursiveBuildTree(*root_internal, &dict);
  return dict;
}

void AccessibilityTreeFormatterFuchsia::RecursiveBuildTree(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  if (!ShouldDumpNode(node))
    return;

  AddProperties(node, dict);
  if (!ShouldDumpChildren(node))
    return;

  base::ListValue children;

  for (size_t i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* child_node = node.PlatformGetChild(i);
    DCHECK(child_node);

    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildTree(*child_node, child_dict.get());
    children.Append(std::move(child_dict));
  }
  dict->SetKey(kChildrenDictAttr, std::move(children));
}

base::Value AccessibilityTreeFormatterFuchsia::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::DictionaryValue dict;
  AddProperties(*BrowserAccessibility::FromAXPlatformNodeDelegate(node), &dict);
  return std::move(dict);
}

void AccessibilityTreeFormatterFuchsia::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  dict->SetInteger("id", node.GetId());

  const BrowserAccessibilityFuchsia* browser_accessibility_fuchsia =
      static_cast<const BrowserAccessibilityFuchsia*>(&node);

  CHECK(browser_accessibility_fuchsia);

  const fuchsia::accessibility::semantics::Node& fuchsia_node =
      browser_accessibility_fuchsia->ToFuchsiaNodeData();

  // Add fuchsia node attributes.
  dict->SetString("role", FuchsiaRoleToString(fuchsia_node.role()));

  if (fuchsia_node.has_attributes()) {
    const fuchsia::accessibility::semantics::Attributes& attributes =
        fuchsia_node.attributes();

    if (attributes.has_label() && !attributes.label().empty())
      dict->SetString("label", attributes.label());

    if (attributes.has_is_keyboard_key())
      dict->SetBoolean("is_keyboard_key", attributes.is_keyboard_key());
  }

  if (fuchsia_node.has_states()) {
    const fuchsia::accessibility::semantics::States& states =
        fuchsia_node.states();

    if (states.has_hidden())
      dict->SetBoolean("hidden", states.hidden());

    if (states.has_focusable())
      dict->SetBoolean("focusable", states.focusable());

    if (states.has_has_input_focus())
      dict->SetBoolean("has_input_focus", states.has_input_focus());
  }

  if (fuchsia_node.has_location())
    dict->SetString("location", LocationToString(fuchsia_node.location()));

  if (fuchsia_node.has_transform()) {
    dict->SetString("transform",
                    Mat4ToString(fuchsia_node.node_to_container_transform()));
  }

  // TODO(fuchsia:88125): Add more fields.
}

std::string AccessibilityTreeFormatterFuchsia::ProcessTreeForOutput(
    const base::DictionaryValue& node) const {
  std::string error_value;
  if (node.GetString("error", &error_value))
    return error_value;

  std::string line;

  if (show_ids()) {
    int id_value = node.FindIntKey("id").value_or(0);
    WriteAttribute(true, base::NumberToString(id_value), &line);
  }

  std::string role_value;
  node.GetString("role", &role_value);
  WriteAttribute(true, role_value, &line);

  for (unsigned i = 0; i < base::size(kBoolAttributes); i++) {
    const char* bool_attribute = kBoolAttributes[i];
    absl::optional<bool> value = node.FindBoolPath(bool_attribute);
    if (value && *value)
      WriteAttribute(/*include_by_default=*/true, bool_attribute, &line);
  }

  for (unsigned i = 0; i < base::size(kStringAttributes); i++) {
    const char* string_attribute = kStringAttributes[i];
    std::string value;
    if (!node.GetString(string_attribute, &value) || value.empty())
      continue;

    bool include_by_default = strcmp(string_attribute, "transform") &&
                              strcmp(string_attribute, "location");
    WriteAttribute(
        include_by_default,
        base::StringPrintf("%s='%s'", string_attribute, value.c_str()), &line);
  }

  return line;
}

base::Value AccessibilityTreeFormatterFuchsia::BuildTreeForSelector(
    const AXTreeSelector&) const {
  NOTIMPLEMENTED();
  return base::Value(base::Value::Type::DICTIONARY);
}

}  // namespace content
