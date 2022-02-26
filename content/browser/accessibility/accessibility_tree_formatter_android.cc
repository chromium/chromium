// Copyright 2013 The Chromium Authors. All rights reserved.
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
    "dismissable",
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
    "scrollable",
    "selected",
    "interesting"
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
    "action_scroll_forward",
    "action_scroll_backward",
    "action_scroll_up",
    "action_scroll_down",
    "action_scroll_left",
    "action_scroll_right",
    "action_expand",
    "action_collapse",
};
// clang-format on
}  // namespace

AccessibilityTreeFormatterAndroid::AccessibilityTreeFormatterAndroid() {}

AccessibilityTreeFormatterAndroid::~AccessibilityTreeFormatterAndroid() {}

base::Value AccessibilityTreeFormatterAndroid::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  CHECK(root);

  BrowserAccessibility* root_internal =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);

  // XXX: Android formatter should walk native Android tree (not internal one).
  base::DictionaryValue dict;
  RecursiveBuildTree(*root_internal, &dict);
  return std::move(dict);
}

base::Value AccessibilityTreeFormatterAndroid::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  NOTREACHED();
  return base::Value(base::Value::Type::DICTIONARY);
}

base::Value AccessibilityTreeFormatterAndroid::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::DictionaryValue dict;
  AddProperties(*BrowserAccessibility::FromAXPlatformNodeDelegate(node), &dict);
  return std::move(dict);
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
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildTree(*child_node, child_dict.get());
    children.Append(std::move(child_dict));
  }
  dict->SetKey(kChildrenDictAttr, std::move(children));
}

void AccessibilityTreeFormatterAndroid::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  dict->SetIntKey("id", node.GetId());

  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // Class name.
  dict->SetStringKey("class", android_node->GetClassName());

  // Bool attributes.
  dict->SetBoolKey("checkable", android_node->IsCheckable());
  dict->SetBoolKey("checked", android_node->IsChecked());
  dict->SetBoolKey("clickable", android_node->IsClickable());
  dict->SetBoolKey("collapsed", android_node->IsCollapsed());
  dict->SetBoolKey("collection", android_node->IsCollection());
  dict->SetBoolKey("collection_item", android_node->IsCollectionItem());
  dict->SetBoolKey("content_invalid", android_node->IsContentInvalid());
  dict->SetBoolKey("disabled", !android_node->IsEnabled());
  dict->SetBoolKey("dismissable", android_node->IsDismissable());
  dict->SetBoolKey("editable_text", android_node->IsTextField());
  dict->SetBoolKey("expanded", android_node->IsExpanded());
  dict->SetBoolKey("focusable", android_node->IsFocusable());
  dict->SetBoolKey("focused", android_node->IsFocused());
  dict->SetBoolKey("has_character_locations",
                   android_node->HasCharacterLocations());
  dict->SetBoolKey("has_image", android_node->HasImage());
  dict->SetBoolKey("has_non_empty_value", android_node->HasNonEmptyValue());
  dict->SetBoolKey("heading", android_node->IsHeading());
  dict->SetBoolKey("hierarchical", android_node->IsHierarchical());
  dict->SetBoolKey("invisible", !android_node->IsVisibleToUser());
  dict->SetBoolKey("link", android_node->IsLink());
  dict->SetBoolKey("multiline", android_node->IsMultiLine());
  dict->SetBoolKey("multiselectable", android_node->IsMultiselectable());
  dict->SetBoolKey("range", android_node->GetData().IsRangeValueSupported());
  dict->SetBoolKey("password", android_node->IsPasswordField());
  dict->SetBoolKey("scrollable", android_node->IsScrollable());
  dict->SetBoolKey("selected", android_node->IsSelected());
  dict->SetBoolKey("interesting", android_node->IsInterestingOnAndroid());

  // String attributes.
  dict->SetStringKey("name", android_node->GetTextContentUTF16());
  dict->SetStringKey("hint", android_node->GetHint());
  dict->SetStringKey("role_description", android_node->GetRoleDescription());
  dict->SetStringKey("state_description", android_node->GetStateDescription());

  // Int attributes.
  dict->SetIntKey("item_index", android_node->GetItemIndex());
  dict->SetIntKey("item_count", android_node->GetItemCount());
  dict->SetIntKey("row_count", android_node->RowCount());
  dict->SetIntKey("column_count", android_node->ColumnCount());
  dict->SetIntKey("row_index", android_node->RowIndex());
  dict->SetIntKey("row_span", android_node->RowSpan());
  dict->SetIntKey("column_index", android_node->ColumnIndex());
  dict->SetIntKey("column_span", android_node->ColumnSpan());
  dict->SetIntKey("input_type", android_node->AndroidInputType());
  dict->SetIntKey("live_region_type", android_node->AndroidLiveRegionType());
  dict->SetIntKey("range_min", static_cast<int>(android_node->RangeMin()));
  dict->SetIntKey("range_max", static_cast<int>(android_node->RangeMax()));
  dict->SetIntKey("range_current_value",
                  static_cast<int>(android_node->RangeCurrentValue()));
  dict->SetIntKey("text_change_added_count",
                  android_node->GetTextChangeAddedCount());
  dict->SetIntKey("text_change_removed_count",
                  android_node->GetTextChangeRemovedCount());

  // Actions.
  dict->SetBoolKey("action_scroll_forward", android_node->CanScrollForward());
  dict->SetBoolKey("action_scroll_backward", android_node->CanScrollBackward());
  dict->SetBoolKey("action_scroll_up", android_node->CanScrollUp());
  dict->SetBoolKey("action_scroll_down", android_node->CanScrollDown());
  dict->SetBoolKey("action_scroll_left", android_node->CanScrollLeft());
  dict->SetBoolKey("action_scroll_right", android_node->CanScrollRight());
  dict->SetBoolKey("action_expand", android_node->IsCollapsed());
  dict->SetBoolKey("action_collapse", android_node->IsExpanded());
}

std::string AccessibilityTreeFormatterAndroid::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  std::string line;
  if (show_ids()) {
    int id_value = dict.FindIntKey("id").value_or(0);
    WriteAttribute(true, base::NumberToString(id_value), &line);
  }

  std::string class_value;
  dict.GetString("class", &class_value);
  WriteAttribute(true, class_value, &line);

  std::string role_description;
  dict.GetString("role_description", &role_description);
  if (!role_description.empty()) {
    WriteAttribute(
        true, StringPrintf("role_description='%s'", role_description.c_str()),
        &line);
  }

  for (unsigned i = 0; i < std::size(BOOL_ATTRIBUTES); i++) {
    const char* attribute_name = BOOL_ATTRIBUTES[i];
    absl::optional<bool> value = dict.FindBoolPath(attribute_name);
    if (value && *value)
      WriteAttribute(true, attribute_name, &line);
  }

  for (unsigned i = 0; i < std::size(STRING_ATTRIBUTES); i++) {
    const char* attribute_name = STRING_ATTRIBUTES[i];
    std::string value;
    if (!dict.GetString(attribute_name, &value) || value.empty())
      continue;
    WriteAttribute(true, StringPrintf("%s='%s'", attribute_name, value.c_str()),
                   &line);
  }

  for (unsigned i = 0; i < std::size(INT_ATTRIBUTES); i++) {
    const char* attribute_name = INT_ATTRIBUTES[i];
    int value = dict.FindIntKey(attribute_name).value_or(0);
    if (value == 0)
      continue;
    WriteAttribute(true, StringPrintf("%s=%d", attribute_name, value), &line);
  }

  for (unsigned i = 0; i < std::size(ACTION_ATTRIBUTES); i++) {
    const char* attribute_name = ACTION_ATTRIBUTES[i];
    absl::optional<bool> value = dict.FindBoolPath(attribute_name);
    if (value && *value) {
      WriteAttribute(false /* Exclude actions by default */, attribute_name,
                     &line);
    }
  }

  return line;
}

}  // namespace content
