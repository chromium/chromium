// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
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

base::Value AccessibilityTreeFormatterAndroid::BuildTreeForWindow(
    gfx::AcceleratedWidget widget) const {
  NOTREACHED();
  return base::Value(base::Value::Type::DICTIONARY);
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

  auto children = std::make_unique<base::ListValue>();

  for (size_t i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* child_node = node.PlatformGetChild(i);
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildTree(*child_node, child_dict.get());
    children->Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(children));
}

void AccessibilityTreeFormatterAndroid::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) const {
  dict->SetInteger("id", node.GetId());

  const BrowserAccessibilityAndroid* android_node =
      static_cast<const BrowserAccessibilityAndroid*>(&node);

  // Class name.
  dict->SetString("class", android_node->GetClassName());

  // Bool attributes.
  dict->SetBoolean("checkable", android_node->IsCheckable());
  dict->SetBoolean("checked", android_node->IsChecked());
  dict->SetBoolean("clickable", android_node->IsClickable());
  dict->SetBoolean("collapsed", android_node->IsCollapsed());
  dict->SetBoolean("collection", android_node->IsCollection());
  dict->SetBoolean("collection_item", android_node->IsCollectionItem());
  dict->SetBoolean("content_invalid", android_node->IsContentInvalid());
  dict->SetBoolean("disabled", !android_node->IsEnabled());
  dict->SetBoolean("dismissable", android_node->IsDismissable());
  dict->SetBoolean("editable_text", android_node->IsTextField());
  dict->SetBoolean("expanded", android_node->IsExpanded());
  dict->SetBoolean("focusable", android_node->IsFocusable());
  dict->SetBoolean("focused", android_node->IsFocused());
  dict->SetBoolean("has_character_locations",
                   android_node->HasCharacterLocations());
  dict->SetBoolean("has_image", android_node->HasImage());
  dict->SetBoolean("has_non_empty_value", android_node->HasNonEmptyValue());
  dict->SetBoolean("heading", android_node->IsHeading());
  dict->SetBoolean("hierarchical", android_node->IsHierarchical());
  dict->SetBoolean("invisible", !android_node->IsVisibleToUser());
  dict->SetBoolean("link", android_node->IsLink());
  dict->SetBoolean("multiline", android_node->IsMultiLine());
  dict->SetBoolean("multiselectable", android_node->IsMultiselectable());
  dict->SetBoolean("range", android_node->GetData().IsRangeValueSupported());
  dict->SetBoolean("password", android_node->IsPasswordField());
  dict->SetBoolean("scrollable", android_node->IsScrollable());
  dict->SetBoolean("selected", android_node->IsSelected());
  dict->SetBoolean("interesting", android_node->IsInterestingOnAndroid());

  // String attributes.
  dict->SetString("name", android_node->GetInnerText());
  dict->SetString("hint", android_node->GetHint());
  dict->SetString("role_description", android_node->GetRoleDescription());
  dict->SetString("state_description", android_node->GetStateDescription());

  // Int attributes.
  dict->SetInteger("item_index", android_node->GetItemIndex());
  dict->SetInteger("item_count", android_node->GetItemCount());
  dict->SetInteger("row_count", android_node->RowCount());
  dict->SetInteger("column_count", android_node->ColumnCount());
  dict->SetInteger("row_index", android_node->RowIndex());
  dict->SetInteger("row_span", android_node->RowSpan());
  dict->SetInteger("column_index", android_node->ColumnIndex());
  dict->SetInteger("column_span", android_node->ColumnSpan());
  dict->SetInteger("input_type", android_node->AndroidInputType());
  dict->SetInteger("live_region_type", android_node->AndroidLiveRegionType());
  dict->SetInteger("range_min", static_cast<int>(android_node->RangeMin()));
  dict->SetInteger("range_max", static_cast<int>(android_node->RangeMax()));
  dict->SetInteger("range_current_value",
                   static_cast<int>(android_node->RangeCurrentValue()));
  dict->SetInteger("text_change_added_count",
                   android_node->GetTextChangeAddedCount());
  dict->SetInteger("text_change_removed_count",
                   android_node->GetTextChangeRemovedCount());

  // Actions.
  dict->SetBoolean("action_scroll_forward", android_node->CanScrollForward());
  dict->SetBoolean("action_scroll_backward", android_node->CanScrollBackward());
  dict->SetBoolean("action_scroll_up", android_node->CanScrollUp());
  dict->SetBoolean("action_scroll_down", android_node->CanScrollDown());
  dict->SetBoolean("action_scroll_left", android_node->CanScrollLeft());
  dict->SetBoolean("action_scroll_right", android_node->CanScrollRight());
  dict->SetBoolean("action_expand", android_node->IsCollapsed());
  dict->SetBoolean("action_collapse", android_node->IsExpanded());
}

std::string AccessibilityTreeFormatterAndroid::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  std::string line;
  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
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

  for (unsigned i = 0; i < base::size(BOOL_ATTRIBUTES); i++) {
    const char* attribute_name = BOOL_ATTRIBUTES[i];
    bool value;
    if (dict.GetBoolean(attribute_name, &value) && value)
      WriteAttribute(true, attribute_name, &line);
  }

  for (unsigned i = 0; i < base::size(STRING_ATTRIBUTES); i++) {
    const char* attribute_name = STRING_ATTRIBUTES[i];
    std::string value;
    if (!dict.GetString(attribute_name, &value) || value.empty())
      continue;
    WriteAttribute(true, StringPrintf("%s='%s'", attribute_name, value.c_str()),
                   &line);
  }

  for (unsigned i = 0; i < base::size(INT_ATTRIBUTES); i++) {
    const char* attribute_name = INT_ATTRIBUTES[i];
    int value;
    if (!dict.GetInteger(attribute_name, &value) || value == 0)
      continue;
    WriteAttribute(true, StringPrintf("%s=%d", attribute_name, value), &line);
  }

  for (unsigned i = 0; i < base::size(ACTION_ATTRIBUTES); i++) {
    const char* attribute_name = ACTION_ATTRIBUTES[i];
    bool value;
    if (dict.GetBoolean(attribute_name, &value) && value) {
      WriteAttribute(false /* Exclude actions by default */, attribute_name,
                     &line);
    }
  }

  return line;
}

}  // namespace content
