// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "ui/accessibility/ax_role_properties.h"

using base::StringPrintf;

namespace content {

namespace {
// clang-format off
const char* const BOOL_ATTRIBUTES[] = {
    "checkable",
    "checked",
    "clickable",
    "collapsed",
    "collection",
    "collection_item",
    "content_invalid",
    "disabled",
    "editable_text",
    "expanded",
    "focusable",
    "focused",
    "has_character_locations",
    "has_image",
    "has_non_empty_value",
    "heading",
    "hierarchical",
    "invisible",
    "link",
    "multiline",
    "multiselectable",
    "password",
    "range",
    "selected",
    "interesting",
    "table_header"
};

const char* const STRING_ATTRIBUTES[] = {
    "name",
    "hint",
    "state_description",
};

const char* const INT_ATTRIBUTES[] = {
    "item_index",
    "item_count",
    "row_count",
    "column_count",
    "row_index",
    "row_span",
    "column_index",
    "column_span",
    "input_type",
    "live_region_type",
    "range_min",
    "range_max",
    "range_current_value",
    "text_change_added_count",
    "text_change_removed_count",
};

const char* const ACTION_ATTRIBUTES[] = {
    "action_expand",
    "action_collapse",
};
// clang-format on
}  // namespace

AccessibilityTreeFormatterAndroid::AccessibilityTreeFormatterAndroid() {}

AccessibilityTreeFormatterAndroid::~AccessibilityTreeFormatterAndroid() {}

base::Value::Dict AccessibilityTreeFormatterAndroid::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  if (!root) {
    return base::Value::Dict();
  }

  // XXX: Android formatter should walk native Android tree (not internal one).
  base::Value::Dict dict;
  RecursiveBuildTree(*root, &dict);
  return dict;
}

base::Value::Dict AccessibilityTreeFormatterAndroid::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  NOTREACHED_IN_MIGRATION();
  return base::Value::Dict();
}

base::Value::Dict AccessibilityTreeFormatterAndroid::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::Value::Dict dict;
  AddProperties(*node, &dict);
  return dict;
}

void AccessibilityTreeFormatterAndroid::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  AddPropertyFilter(property_filters, "hint=*");
  AddPropertyFilter(property_filters, "interesting", AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "has_character_locations",
                    AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "has_image", AXPropertyFilter::DENY);
}

void AccessibilityTreeFormatterAndroid::RecursiveBuildTree(
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

  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  for (size_t i = 0; i < android_node->PlatformChildCount(); ++i) {
    ui::BrowserAccessibility* child_node = android_node->PlatformGetChild(i);
    CHECK(child_node);
    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_node, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

void AccessibilityTreeFormatterAndroid::AddProperties(
    const ui::AXPlatformNodeDelegate& node,
    base::Value::Dict* dict) const {
  dict->Set("id", node.GetId());

  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // Class name.
  dict->Set("class", android_node->GetClassName());

  // Bool attributes.
  dict->Set("checkable", android_node->IsCheckable());
  dict->Set("checked", android_node->IsChecked());
  dict->Set("clickable", android_node->IsClickable());
  dict->Set("collapsed", android_node->IsCollapsed());
  dict->Set("collection", android_node->IsCollection());
  dict->Set("collection_item", android_node->IsCollectionItem());
  dict->Set("content_invalid", android_node->IsContentInvalid());
  dict->Set("disabled", !android_node->IsEnabled());
  dict->Set("editable_text", android_node->IsTextField());
  dict->Set("expanded", android_node->IsExpanded());
  dict->Set("focusable", android_node->IsFocusable());
  dict->Set("focused", android_node->IsFocused());
  dict->Set("has_character_locations", android_node->HasCharacterLocations());
  dict->Set("has_image", android_node->HasImage());
  dict->Set("has_non_empty_value", android_node->HasNonEmptyValue());
  dict->Set("heading", android_node->IsHeading());
  dict->Set("hierarchical", android_node->IsHierarchical());
  dict->Set("invisible", !android_node->IsVisibleToUser());
  dict->Set("link", ui::IsLink(android_node->GetRole()));
  dict->Set("multiline", android_node->IsMultiLine());
  dict->Set("multiselectable", android_node->IsMultiselectable());
  dict->Set("range", android_node->GetData().IsRangeValueSupported());
  dict->Set("password", android_node->IsPasswordField());
  dict->Set("selected", android_node->IsSelected());
  dict->Set("interesting", android_node->IsInterestingOnAndroid());
  dict->Set("table_header", android_node->IsTableHeader());

  // String attributes.
  dict->Set("name", android_node->GetTextContentUTF16());
  dict->Set("hint", android_node->GetHint());
  dict->Set("role_description", android_node->GetRoleDescription());
  dict->Set("state_description", android_node->GetStateDescription());

  // Int attributes.
  dict->Set("item_index", android_node->GetItemIndex());
  dict->Set("item_count", android_node->GetItemCount());
  dict->Set("row_count", android_node->RowCount());
  dict->Set("column_count", android_node->ColumnCount());
  dict->Set("row_index", android_node->RowIndex());
  dict->Set("row_span", android_node->RowSpan());
  dict->Set("column_index", android_node->ColumnIndex());
  dict->Set("column_span", android_node->ColumnSpan());
  dict->Set("input_type", android_node->AndroidInputType());
  dict->Set("live_region_type", android_node->AndroidLiveRegionType());
  dict->Set("range_min", static_cast<int>(android_node->RangeMin()));
  dict->Set("range_max", static_cast<int>(android_node->RangeMax()));
  dict->Set("range_current_value",
            static_cast<int>(android_node->RangeCurrentValue()));
  dict->Set("text_change_added_count", android_node->GetTextChangeAddedCount());
  dict->Set("text_change_removed_count",
            android_node->GetTextChangeRemovedCount());

  // Actions.
  dict->Set("action_expand", android_node->IsCollapsed());
  dict->Set("action_collapse", android_node->IsExpanded());
}

std::string AccessibilityTreeFormatterAndroid::ProcessTreeForOutput(
    const base::Value::Dict& dict) const {
  const std::string* error_value = dict.FindString("error");
  if (error_value) {
    return *error_value;
  }

  std::string line;
  if (show_ids()) {
    int id_value = dict.FindInt("id").value_or(0);
    WriteAttribute(true, base::NumberToString(id_value), &line);
  }

  const std::string* class_value = dict.FindString("class");
  if (class_value) {
    WriteAttribute(true, *class_value, &line);
  }

  const std::string* role_description = dict.FindString("role_description");
  if (role_description && !role_description->empty()) {
    WriteAttribute(
        true, StringPrintf("role_description='%s'", role_description->c_str()),
        &line);
  }

  for (const char* attribute_name : BOOL_ATTRIBUTES) {
    std::optional<bool> value = dict.FindBool(attribute_name);
    if (value && *value) {
      WriteAttribute(true, attribute_name, &line);
    }
  }

  for (const char* attribute_name : STRING_ATTRIBUTES) {
    const std::string* value = dict.FindString(attribute_name);
    if (!value || value->empty()) {
      continue;
    }
    WriteAttribute(
        true, StringPrintf("%s='%s'", attribute_name, value->c_str()), &line);
  }

  for (const char* attribute_name : INT_ATTRIBUTES) {
    int value = dict.FindInt(attribute_name).value_or(0);
    if (value == 0) {
      continue;
    }
    WriteAttribute(true, StringPrintf("%s=%d", attribute_name, value), &line);
  }

  for (const char* attribute_name : ACTION_ATTRIBUTES) {
    if (dict.FindBool(attribute_name).value_or(false)) {
      WriteAttribute(false /* Exclude actions by default */, attribute_name,
                     &line);
    }
  }

  return line;
}

}  // namespace content
