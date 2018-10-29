// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

#include <math.h>
#include <stddef.h>

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace content {

AccessibilityTreeFormatterBlink::AccessibilityTreeFormatterBlink()
    : AccessibilityTreeFormatterBrowser() {}

AccessibilityTreeFormatterBlink::~AccessibilityTreeFormatterBlink() {
}

const char* const TREE_DATA_ATTRIBUTES[] = {"TreeData.textSelStartOffset",
                                            "TreeData.textSelEndOffset"};

const char* STATE_FOCUSED = "focused";
const char* STATE_OFFSCREEN = "offscreen";

uint32_t AccessibilityTreeFormatterBlink::ChildCount(
    const BrowserAccessibility& node) const {
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId))
    return node.PlatformChildCount();
  else
    return node.InternalChildCount();
}

BrowserAccessibility* AccessibilityTreeFormatterBlink::GetChild(
    const BrowserAccessibility& node,
    uint32_t i) const {
  if (node.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId))
    return node.PlatformGetChild(i);
  else
    return node.InternalGetChild(i);
}

// TODO(aleventhal) Convert ax enums to friendly strings, e.g.
// ax::mojom::CheckedState.
std::string AccessibilityTreeFormatterBlink::IntAttrToString(
    const BrowserAccessibility& node,
    ax::mojom::IntAttribute attr,
    int value) const {
  if (ui::IsNodeIdIntAttribute(attr)) {
    // Relation
    BrowserAccessibility* target = node.manager()->GetFromID(value);
    return target ? ui::ToString(target->GetData().role) : std::string("null");
  }

  switch (attr) {
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return ui::ToString(static_cast<ax::mojom::AriaCurrentState>(value));
    case ax::mojom::IntAttribute::kCheckedState:
      return ui::ToString(static_cast<ax::mojom::CheckedState>(value));
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return ui::ToString(static_cast<ax::mojom::DefaultActionVerb>(value));
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return ui::ToString(static_cast<ax::mojom::DescriptionFrom>(value));
    case ax::mojom::IntAttribute::kHasPopup:
      return ui::ToString(static_cast<ax::mojom::HasPopup>(value));
    case ax::mojom::IntAttribute::kInvalidState:
      return ui::ToString(static_cast<ax::mojom::InvalidState>(value));
    case ax::mojom::IntAttribute::kNameFrom:
      return ui::ToString(static_cast<ax::mojom::NameFrom>(value));
    case ax::mojom::IntAttribute::kRestriction:
      return ui::ToString(static_cast<ax::mojom::Restriction>(value));
    case ax::mojom::IntAttribute::kSortDirection:
      return ui::ToString(static_cast<ax::mojom::SortDirection>(value));
    case ax::mojom::IntAttribute::kTextDirection:
      return ui::ToString(static_cast<ax::mojom::TextDirection>(value));
    case ax::mojom::IntAttribute::kTextPosition:
      return ui::ToString(static_cast<ax::mojom::TextPosition>(value));
    // No pretty printing necessary for these:
    case ax::mojom::IntAttribute::kActivedescendantId:
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
    case ax::mojom::IntAttribute::kAriaColumnCount:
    case ax::mojom::IntAttribute::kAriaRowCount:
    case ax::mojom::IntAttribute::kBackgroundColor:
    case ax::mojom::IntAttribute::kColor:
    case ax::mojom::IntAttribute::kColorValue:
    case ax::mojom::IntAttribute::kDetailsId:
    case ax::mojom::IntAttribute::kErrormessageId:
    case ax::mojom::IntAttribute::kHierarchicalLevel:
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
    case ax::mojom::IntAttribute::kMemberOfId:
    case ax::mojom::IntAttribute::kNextFocusId:
    case ax::mojom::IntAttribute::kNextOnLineId:
    case ax::mojom::IntAttribute::kPosInSet:
    case ax::mojom::IntAttribute::kPreviousFocusId:
    case ax::mojom::IntAttribute::kPreviousOnLineId:
    case ax::mojom::IntAttribute::kScrollX:
    case ax::mojom::IntAttribute::kScrollXMax:
    case ax::mojom::IntAttribute::kScrollXMin:
    case ax::mojom::IntAttribute::kScrollY:
    case ax::mojom::IntAttribute::kScrollYMax:
    case ax::mojom::IntAttribute::kScrollYMin:
    case ax::mojom::IntAttribute::kSetSize:
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
    case ax::mojom::IntAttribute::kTableCellRowIndex:
    case ax::mojom::IntAttribute::kTableCellRowSpan:
    case ax::mojom::IntAttribute::kTableColumnCount:
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
    case ax::mojom::IntAttribute::kTableColumnIndex:
    case ax::mojom::IntAttribute::kTableHeaderId:
    case ax::mojom::IntAttribute::kTableRowCount:
    case ax::mojom::IntAttribute::kTableRowHeaderId:
    case ax::mojom::IntAttribute::kTableRowIndex:
    case ax::mojom::IntAttribute::kTextSelEnd:
    case ax::mojom::IntAttribute::kTextSelStart:
    case ax::mojom::IntAttribute::kTextStyle:
    case ax::mojom::IntAttribute::kNone:
      break;
  }

  // Just return the number
  return std::to_string(value);
}

void AccessibilityTreeFormatterBlink::AddProperties(
    const BrowserAccessibility& node,
    base::DictionaryValue* dict) {
  int id = node.GetId();
  dict->SetInteger("id", id);

  dict->SetString("internalRole", ui::ToString(node.GetData().role));

  gfx::Rect bounds = gfx::ToEnclosingRect(node.GetData().location);
  dict->SetInteger("boundsX", bounds.x());
  dict->SetInteger("boundsY", bounds.y());
  dict->SetInteger("boundsWidth", bounds.width());
  dict->SetInteger("boundsHeight", bounds.height());

  bool offscreen = false;
  gfx::Rect page_bounds = node.GetPageBoundsRect(&offscreen);
  dict->SetInteger("pageBoundsX", page_bounds.x());
  dict->SetInteger("pageBoundsY", page_bounds.y());
  dict->SetInteger("pageBoundsWidth", page_bounds.width());
  dict->SetInteger("pageBoundsHeight", page_bounds.height());

  dict->SetBoolean("transform",
                   node.GetData().transform &&
                   !node.GetData().transform->IsIdentity());

  gfx::Rect unclipped_bounds = node.GetPageBoundsRect(&offscreen, false);
  dict->SetInteger("unclippedBoundsX", unclipped_bounds.x());
  dict->SetInteger("unclippedBoundsY", unclipped_bounds.y());
  dict->SetInteger("unclippedBoundsWidth", unclipped_bounds.width());
  dict->SetInteger("unclippedBoundsHeight", unclipped_bounds.height());

  // Language and Font Family are different from other string attributes
  // in that they inherit.
  std::string font_family =
      node.GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily);
  if (!font_family.empty())
    dict->SetString("fontFamily", font_family);
  std::string language =
      node.GetInheritedStringAttribute(ax::mojom::StringAttribute::kLanguage);
  if (!language.empty())
    dict->SetString("language", language);

  for (int32_t state_index = static_cast<int32_t>(ax::mojom::State::kNone);
       state_index <= static_cast<int32_t>(ax::mojom::State::kMaxValue);
       ++state_index) {
    auto state = static_cast<ax::mojom::State>(state_index);
    if (node.HasState(state))
      dict->SetBoolean(ui::ToString(state), true);
  }

  if (offscreen)
    dict->SetBoolean(STATE_OFFSCREEN, true);

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::StringAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::StringAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::StringAttribute>(attr_index);
    if (attr != ax::mojom::StringAttribute::kFontFamily &&
        attr != ax::mojom::StringAttribute::kLanguage) {
      if (node.HasStringAttribute(attr))
        dict->SetString(ui::ToString(attr), node.GetStringAttribute(attr));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::IntAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::IntAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntAttribute>(attr_index);
    if (node.HasIntAttribute(attr)) {
      int value = node.GetIntAttribute(attr);
      dict->SetString(ui::ToString(attr), IntAttrToString(node, attr, value));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::FloatAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::FloatAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::FloatAttribute>(attr_index);
    if (node.HasFloatAttribute(attr) &&
        std::isfinite(node.GetFloatAttribute(attr)))
      dict->SetDouble(ui::ToString(attr), node.GetFloatAttribute(attr));
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::BoolAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::BoolAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::BoolAttribute>(attr_index);
    if (node.HasBoolAttribute(attr))
      dict->SetBoolean(ui::ToString(attr), node.GetBoolAttribute(attr));
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::IntListAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::IntListAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntListAttribute>(attr_index);
    if (node.HasIntListAttribute(attr)) {
      std::vector<int32_t> values;
      node.GetIntListAttribute(attr, &values);
      auto value_list = std::make_unique<base::ListValue>();
      for (size_t i = 0; i < values.size(); ++i) {
        if (ui::IsNodeIdIntListAttribute(attr)) {
          BrowserAccessibility* target = node.manager()->GetFromID(values[i]);
          if (target)
            value_list->AppendString(ui::ToString(target->GetData().role));
          else
            value_list->AppendString("null");
        } else {
          value_list->AppendInteger(values[i]);
        }
      }
      dict->Set(ui::ToString(attr), std::move(value_list));
    }
  }

  //  Check for relevant rich text selection info in AXTreeData
  int anchor_id = node.manager()->GetTreeData().sel_anchor_object_id;
  if (id == anchor_id) {
    int anchor_offset = node.manager()->GetTreeData().sel_anchor_offset;
    dict->SetInteger("TreeData.textSelStartOffset", anchor_offset);
  }
  int focus_id = node.manager()->GetTreeData().sel_focus_object_id;
  if (id == focus_id) {
    int focus_offset = node.manager()->GetTreeData().sel_focus_offset;
    dict->SetInteger("TreeData.textSelEndOffset", focus_offset);
  }

  std::vector<std::string> actions_strings;
  for (int32_t action_index =
           static_cast<int32_t>(ax::mojom::Action::kNone) + 1;
       action_index <= static_cast<int32_t>(ax::mojom::Action::kMaxValue);
       ++action_index) {
    auto action = static_cast<ax::mojom::Action>(action_index);
    if (node.HasAction(action))
      actions_strings.push_back(ui::ToString(action));
  }
  if (!actions_strings.empty())
    dict->SetString("actions", base::JoinString(actions_strings, ","));
}

base::string16 AccessibilityTreeFormatterBlink::ProcessTreeForOutput(
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
  base::string16 error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  base::string16 line;

  if (show_ids()) {
    int id_value;
    dict.GetInteger("id", &id_value);
    WriteAttribute(true, base::IntToString16(id_value), &line);
  }

  base::string16 role_value;
  dict.GetString("internalRole", &role_value);
  WriteAttribute(true, base::UTF16ToUTF8(role_value), &line);

  for (int state_index = static_cast<int32_t>(ax::mojom::State::kNone);
       state_index <= static_cast<int32_t>(ax::mojom::State::kMaxValue);
       ++state_index) {
    auto state = static_cast<ax::mojom::State>(state_index);
    const base::Value* value;
    if (!dict.Get(ui::ToString(state), &value))
      continue;

    WriteAttribute(false, ui::ToString(state), &line);
  }

  // Offscreen and Focused states are not in the state list.
  bool offscreen = false;
  dict.GetBoolean(STATE_OFFSCREEN, &offscreen);
  if (offscreen)
    WriteAttribute(false, STATE_OFFSCREEN, &line);
  bool focused = false;
  dict.GetBoolean(STATE_FOCUSED, &focused);
  if (focused)
    WriteAttribute(false, STATE_FOCUSED, &line);

  WriteAttribute(false,
                 FormatCoordinates("location", "boundsX", "boundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("size", "boundsWidth", "boundsHeight", dict),
                 &line);

  WriteAttribute(false,
                 FormatCoordinates("pageLocation",
                                   "pageBoundsX", "pageBoundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("pageSize",
                                   "pageBoundsWidth", "pageBoundsHeight", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("unclippedLocation", "unclippedBoundsX",
                                   "unclippedBoundsY", dict),
                 &line);
  WriteAttribute(false,
                 FormatCoordinates("unclippedSize", "unclippedBoundsWidth",
                                   "unclippedBoundsHeight", dict),
                 &line);

  bool transform;
  if (dict.GetBoolean("transform", &transform) && transform)
    WriteAttribute(false, "transform", &line);

  for (int attr_index = static_cast<int32_t>(ax::mojom::StringAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::StringAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::StringAttribute>(attr_index);
    std::string string_value;
    if (!dict.GetString(ui::ToString(attr), &string_value))
      continue;
    WriteAttribute(
        false,
        base::StringPrintf("%s='%s'", ui::ToString(attr), string_value.c_str()),
        &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::IntAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::IntAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntAttribute>(attr_index);
    std::string string_value;
    if (!dict.GetString(ui::ToString(attr), &string_value))
      continue;
    WriteAttribute(
        false,
        base::StringPrintf("%s=%s", ui::ToString(attr), string_value.c_str()),
        &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::BoolAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::BoolAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::BoolAttribute>(attr_index);
    bool bool_value;
    if (!dict.GetBoolean(ui::ToString(attr), &bool_value))
      continue;
    WriteAttribute(false,
                   base::StringPrintf("%s=%s", ui::ToString(attr),
                                      bool_value ? "true" : "false"),
                   &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::FloatAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::FloatAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::FloatAttribute>(attr_index);
    double float_value;
    if (!dict.GetDouble(ui::ToString(attr), &float_value))
      continue;
    WriteAttribute(
        false, base::StringPrintf("%s=%.2f", ui::ToString(attr), float_value),
        &line);
  }

  for (int attr_index =
           static_cast<int32_t>(ax::mojom::IntListAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::IntListAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntListAttribute>(attr_index);
    const base::ListValue* value;
    if (!dict.GetList(ui::ToString(attr), &value))
      continue;
    std::string attr_string(ui::ToString(attr));
    attr_string.push_back('=');
    for (size_t i = 0; i < value->GetSize(); ++i) {
      if (i > 0)
        attr_string += ",";
      if (ui::IsNodeIdIntListAttribute(attr)) {
        std::string string_value;
        value->GetString(i, &string_value);
        attr_string += string_value;
      } else {
        int int_value;
        value->GetInteger(i, &int_value);
        attr_string += base::IntToString(int_value);
      }
    }
    WriteAttribute(false, attr_string, &line);
  }

  std::string actions_value;
  if (dict.GetString("actions", &actions_value)) {
    WriteAttribute(
        false, base::StringPrintf("%s=%s", "actions", actions_value.c_str()),
        &line);
  }

  for (const char* attribute_name : TREE_DATA_ATTRIBUTES) {
    const base::Value* value;
    if (!dict.Get(attribute_name, &value))
      continue;

    switch (value->type()) {
      case base::Value::Type::STRING: {
        std::string string_value;
        value->GetAsString(&string_value);
        WriteAttribute(
            false,
            base::StringPrintf("%s=%s", attribute_name, string_value.c_str()),
            &line);
        break;
      }
      case base::Value::Type::INTEGER: {
        int int_value = 0;
        value->GetAsInteger(&int_value);
        WriteAttribute(false,
                       base::StringPrintf("%s=%d", attribute_name, int_value),
                       &line);
        break;
      }
      case base::Value::Type::DOUBLE: {
        double double_value = 0.0;
        value->GetAsDouble(&double_value);
        WriteAttribute(
            false, base::StringPrintf("%s=%.2f", attribute_name, double_value),
            &line);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  return line;
}

const base::FilePath::StringType
AccessibilityTreeFormatterBlink::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-blink.txt");
}

const std::string AccessibilityTreeFormatterBlink::GetAllowEmptyString() {
  return "@BLINK-ALLOW-EMPTY:";
}

const std::string AccessibilityTreeFormatterBlink::GetAllowString() {
  return "@BLINK-ALLOW:";
}

const std::string AccessibilityTreeFormatterBlink::GetDenyString() {
  return "@BLINK-DENY:";
}

}  // namespace content
