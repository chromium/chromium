// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/compute_attributes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace content {
namespace {

std::optional<std::string> GetStringAttribute(const ui::AXNode& node,
                                              ax::mojom::StringAttribute attr) {
  // Language is different from other string attributes as it inherits and has
  // a method to compute it.
  if (attr == ax::mojom::StringAttribute::kLanguage) {
    std::string value = node.GetLanguage();
    if (value.empty()) {
      return std::nullopt;
    }
    return value;
  }

  // Font Family is different from other string attributes as it inherits.
  if (attr == ax::mojom::StringAttribute::kFontFamily) {
    std::string value = node.GetInheritedStringAttribute(attr);
    if (value.empty()) {
      return std::nullopt;
    }
    return value;
  }

  // Always return the attribute if the node has it, even if the value is an
  // empty string.
  if (node.HasStringAttribute(attr)) {
    return node.GetStringAttribute(attr);
  }
  return std::nullopt;
}

std::string FormatColor(int argb) {
  // Don't output the alpha component; only the red, green and blue
  // actually matter.
  int rgb = (static_cast<uint32_t>(argb) & 0xffffff);
  return base::StringPrintf("%06x", rgb);
}

std::string IntAttrToString(const ui::AXNode& node,
                            ax::mojom::IntAttribute attr,
                            int32_t value) {
  if (ui::IsNodeIdIntAttribute(attr)) {
    // Relation
    ui::AXTreeID tree_id = node.tree()->GetAXTreeID();
    ui::AXNode* target =
        ui::AXTreeManager::FromID(tree_id)->GetNodeFromTree(tree_id, value);
    if (!target) {
      return "null";
    }

    std::string result = ui::ToString(target->GetRole());
    // Provide some extra info about the related object via the name or
    // possibly the class (if an element).
    // TODO(accessibility) Include all relational attributes here.
    // TODO(accessibility) Consider using line numbers from the results instead.
    if (attr == ax::mojom::IntAttribute::kNextOnLineId ||
        attr == ax::mojom::IntAttribute::kPreviousOnLineId) {
      if (target->HasStringAttribute(ax::mojom::StringAttribute::kName)) {
        result += ":\"";
        result += target->GetStringAttribute(ax::mojom::StringAttribute::kName);
        result += "\"";
      } else if (target->HasStringAttribute(
                     ax::mojom::StringAttribute::kClassName)) {
        result += ".";
        result +=
            target->GetStringAttribute(ax::mojom::StringAttribute::kClassName);
      }
    }
    return result;
  }

  switch (attr) {
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return ui::ToString(static_cast<ax::mojom::AriaCurrentState>(value));
    case ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated:
      return ui::ToString(
          static_cast<ax::mojom::AriaNotificationInterrupt>(value));
    case ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated:
      return ui::ToString(
          static_cast<ax::mojom::AriaNotificationPriority>(value));
    case ax::mojom::IntAttribute::kCheckedState:
      return ui::ToString(static_cast<ax::mojom::CheckedState>(value));
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return ui::ToString(static_cast<ax::mojom::DefaultActionVerb>(value));
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return ui::ToString(static_cast<ax::mojom::DescriptionFrom>(value));
    case ax::mojom::IntAttribute::kDetailsFrom:
      return ui::ToString(static_cast<ax::mojom::DetailsFrom>(value));
    case ax::mojom::IntAttribute::kHasPopup:
      return ui::ToString(static_cast<ax::mojom::HasPopup>(value));
    case ax::mojom::IntAttribute::kInvalidState:
      return ui::ToString(static_cast<ax::mojom::InvalidState>(value));
    case ax::mojom::IntAttribute::kIsPopup:
      return ui::ToString(static_cast<ax::mojom::IsPopup>(value));
    case ax::mojom::IntAttribute::kListStyle:
      return ui::ToString(static_cast<ax::mojom::ListStyle>(value));
    case ax::mojom::IntAttribute::kNameFrom:
      return ui::ToString(static_cast<ax::mojom::NameFrom>(value));
    case ax::mojom::IntAttribute::kRestriction:
      return ui::ToString(static_cast<ax::mojom::Restriction>(value));
    case ax::mojom::IntAttribute::kSortDirection:
      return ui::ToString(static_cast<ax::mojom::SortDirection>(value));
    case ax::mojom::IntAttribute::kTextAlign:
      return ui::ToString(static_cast<ax::mojom::TextAlign>(value));
    case ax::mojom::IntAttribute::kTextOverlineStyle:
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
      return ui::ToString(static_cast<ax::mojom::TextDecorationStyle>(value));
    case ax::mojom::IntAttribute::kTextDirection:
      return ui::ToString(static_cast<ax::mojom::WritingDirection>(value));
    case ax::mojom::IntAttribute::kTextPosition:
      return ui::ToString(static_cast<ax::mojom::TextPosition>(value));
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
      return ui::ToString(static_cast<ax::mojom::ImageAnnotationStatus>(value));
    case ax::mojom::IntAttribute::kBackgroundColor:
      return FormatColor(node.ComputeBackgroundColor());
    case ax::mojom::IntAttribute::kColor:
      return FormatColor(node.ComputeColor());
    // No pretty printing necessary for these:
    case ax::mojom::IntAttribute::kActivedescendantId:
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
    case ax::mojom::IntAttribute::kAriaColumnCount:
    case ax::mojom::IntAttribute::kAriaCellColumnSpan:
    case ax::mojom::IntAttribute::kAriaCellRowSpan:
    case ax::mojom::IntAttribute::kAriaRowCount:
    case ax::mojom::IntAttribute::kColorValue:
    case ax::mojom::IntAttribute::kDOMNodeIdDeprecated:
    case ax::mojom::IntAttribute::kDropeffectDeprecated:
    case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
    case ax::mojom::IntAttribute::kHierarchicalLevel:
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
    case ax::mojom::IntAttribute::kMemberOfId:
    case ax::mojom::IntAttribute::kNextFocusId:
    case ax::mojom::IntAttribute::kNextOnLineId:
    case ax::mojom::IntAttribute::kNextWindowFocusId:
    case ax::mojom::IntAttribute::kPosInSet:
    case ax::mojom::IntAttribute::kPopupForId:
    case ax::mojom::IntAttribute::kPreviousFocusId:
    case ax::mojom::IntAttribute::kPreviousOnLineId:
    case ax::mojom::IntAttribute::kPreviousWindowFocusId:
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
  return base::NumberToString(value);
}

}  // namespace

AccessibilityTreeFormatterBlink::AccessibilityTreeFormatterBlink() = default;
AccessibilityTreeFormatterBlink::~AccessibilityTreeFormatterBlink() = default;

void AccessibilityTreeFormatterBlink::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  // Noisy, perhaps add later:
  //   editable, focus*, horizontal, linked, richlyEditable, vertical
  // Too flaky: hovered, offscreen
  // States
  AddPropertyFilter(property_filters, "collapsed");
  AddPropertyFilter(property_filters, "invisible");
  AddPropertyFilter(property_filters, "multiline");
  AddPropertyFilter(property_filters, "protected");
  AddPropertyFilter(property_filters, "required");
  AddPropertyFilter(property_filters, "select*");
  AddPropertyFilter(property_filters, "selectedFromFocus=*",
                    AXPropertyFilter::DENY);
  AddPropertyFilter(property_filters, "visited");
  // Other attributes
  AddPropertyFilter(property_filters, "busy=true");
  AddPropertyFilter(property_filters, "valueForRange*");
  AddPropertyFilter(property_filters, "minValueForRange*");
  AddPropertyFilter(property_filters, "maxValueForRange*");
  AddPropertyFilter(property_filters, "autoComplete*");
  AddPropertyFilter(property_filters, "restriction*");
  AddPropertyFilter(property_filters, "keyShortcuts*");
  AddPropertyFilter(property_filters, "activedescendantId*");
  AddPropertyFilter(property_filters, "controlsIds*");
  AddPropertyFilter(property_filters, "flowtoIds*");
  AddPropertyFilter(property_filters, "detailsIds*");
  AddPropertyFilter(property_filters, "invalidState=*");
  AddPropertyFilter(property_filters, "ignored*");
  AddPropertyFilter(property_filters, "invalidState=false",
                    AXPropertyFilter::DENY);  // Don't show false value
  AddPropertyFilter(property_filters, "roleDescription=*");
  AddPropertyFilter(property_filters, "errormessageId=*");
  AddPropertyFilter(property_filters, "virtualContent=*");
  AddPropertyFilter(property_filters, "descriptionFrom=prohibitedNameRepair");
  // Add the rare name-from values.
  AddPropertyFilter(property_filters, "nameFrom=caption");
  AddPropertyFilter(property_filters, "nameFrom=placeholder");
  AddPropertyFilter(property_filters, "nameFrom=prohibited");
  AddPropertyFilter(property_filters, "nameFrom=title");
  AddPropertyFilter(property_filters, "nameFrom=value");
}

const char* const TREE_DATA_ATTRIBUTES[] = {"TreeData.textSelStartOffset",
                                            "TreeData.textSelEndOffset"};

const char* STATE_FOCUSED = "focused";
const char* STATE_OFFSCREEN = "offscreen";

base::Value::Dict AccessibilityTreeFormatterBlink::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  if (!root) {
    return base::Value::Dict();
  }

  ui::BrowserAccessibility* root_internal =
      ui::BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  base::Value::Dict dict;
  RecursiveBuildTree(*root_internal, &dict);
  return dict;
}

base::Value::Dict AccessibilityTreeFormatterBlink::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  NOTREACHED_IN_MIGRATION();
  return base::Value::Dict();
}

base::Value::Dict AccessibilityTreeFormatterBlink::BuildTreeForNode(
    ui::AXNode* node) const {
  CHECK(node);
  base::Value::Dict dict;
  RecursiveBuildTree(*node, &dict);
  return dict;
}

base::Value::Dict AccessibilityTreeFormatterBlink::BuildNode(
    ui::AXPlatformNodeDelegate* node) const {
  CHECK(node);
  base::Value::Dict dict;
  AddProperties(*ui::BrowserAccessibility::FromAXPlatformNodeDelegate(node),
                &dict);
  return dict;
}

std::string AccessibilityTreeFormatterBlink::DumpInternalAccessibilityTree(
    ui::AXTreeID tree_id,
    const std::vector<AXPropertyFilter>& property_filters) {
  ui::AXTreeManager* ax_mgr = ui::AXTreeManager::FromID(tree_id);
  DCHECK(ax_mgr);
  SetPropertyFilters(property_filters, kFiltersDefaultSet);
  return FormatTree(BuildTreeForNode(ax_mgr->GetRoot()));
}

void AccessibilityTreeFormatterBlink::RecursiveBuildTree(
    const ui::BrowserAccessibility& node,
    base::Value::Dict* dict) const {
  if (!ShouldDumpNode(node)) {
    return;
  }

  AddProperties(node, dict);
  if (!ShouldDumpChildren(node)) {
    return;
  }

  base::Value::List children;
  for (const auto* child_node : node.AllChildren()) {
    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_node, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, base::Value(std::move(children)));
}

void AccessibilityTreeFormatterBlink::RecursiveBuildTree(
    const ui::AXNode& node,
    base::Value::Dict* dict) const {
  AddProperties(node, dict);

  base::Value::List children;
  for (ui::AXNode* child_node : node.children()) {
    base::Value::Dict child_dict;
    RecursiveBuildTree(*child_node, &child_dict);
    children.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, base::Value(std::move(children)));
}

void AccessibilityTreeFormatterBlink::AddProperties(
    const ui::BrowserAccessibility& node,
    base::Value::Dict* dict) const {
  int id = node.GetId();
  dict->Set("id", id);

  dict->Set("internalRole", ui::ToString(node.GetRole()));

  gfx::Rect bounds =
      gfx::ToEnclosingRect(node.GetData().relative_bounds.bounds);
  dict->Set("boundsX", bounds.x());
  dict->Set("boundsY", bounds.y());
  dict->Set("boundsWidth", bounds.width());
  dict->Set("boundsHeight", bounds.height());

  ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult::kOnscreen;
  gfx::Rect page_bounds = node.GetClippedRootFrameBoundsRect(&offscreen_result);
  dict->Set("pageBoundsX", page_bounds.x());
  dict->Set("pageBoundsY", page_bounds.y());
  dict->Set("pageBoundsWidth", page_bounds.width());
  dict->Set("pageBoundsHeight", page_bounds.height());

  dict->Set("transform",
            node.GetData().relative_bounds.transform &&
                !node.GetData().relative_bounds.transform->IsIdentity());

  gfx::Rect unclipped_bounds =
      node.GetUnclippedRootFrameBoundsRect(&offscreen_result);
  dict->Set("unclippedBoundsX", unclipped_bounds.x());
  dict->Set("unclippedBoundsY", unclipped_bounds.y());
  dict->Set("unclippedBoundsWidth", unclipped_bounds.width());
  dict->Set("unclippedBoundsHeight", unclipped_bounds.height());

  for (int32_t state_index = static_cast<int32_t>(ax::mojom::State::kNone);
       state_index <= static_cast<int32_t>(ax::mojom::State::kMaxValue);
       ++state_index) {
    auto state = static_cast<ax::mojom::State>(state_index);
    if (state == ax::mojom::State::kFocusable ? node.IsFocusable()
                                              : node.HasState(state)) {
      dict->SetByDottedPath(ui::ToString(state), true);
    }
  }

  if (offscreen_result == ui::AXOffscreenResult::kOffscreen) {
    dict->Set(STATE_OFFSCREEN, true);
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::StringAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::StringAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::StringAttribute>(attr_index);
    auto maybe_value = GetStringAttribute(*node.node(), attr);
    if (maybe_value.has_value()) {
      dict->SetByDottedPath(ui::ToString(attr), maybe_value.value());
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::IntAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::IntAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntAttribute>(attr_index);
    auto maybe_value = ui::ComputeAttribute(&node, attr);
    if (maybe_value.has_value()) {
      dict->SetByDottedPath(
          ui::ToString(attr),
          IntAttrToString(*node.node(), attr, maybe_value.value()));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::FloatAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::FloatAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::FloatAttribute>(attr_index);
    if (node.HasFloatAttribute(attr) &&
        std::isfinite(node.GetFloatAttribute(attr))) {
      dict->SetByDottedPath(ui::ToString(attr), node.GetFloatAttribute(attr));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::BoolAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::BoolAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::BoolAttribute>(attr_index);
    if (node.HasBoolAttribute(attr)) {
      dict->SetByDottedPath(ui::ToString(attr), node.GetBoolAttribute(attr));
    }
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
      base::Value::List value_list;
      for (const int& value : values) {
        if (ui::IsNodeIdIntListAttribute(attr)) {
          ui::BrowserAccessibility* target = node.manager()->GetFromID(value);
          if (target) {
            value_list.Append(ui::ToString(target->GetRole()));
          } else {
            value_list.Append("null");
          }
        } else {
          value_list.Append(value);
        }
      }
      dict->Set(ui::ToString(attr), std::move(value_list));
    }
  }

  //  Check for relevant rich text selection info in AXTreeData
  ui::AXSelection unignored_selection =
      node.manager()->ax_tree()->GetUnignoredSelection();
  int anchor_id = unignored_selection.anchor_object_id;
  if (id == anchor_id) {
    int anchor_offset = unignored_selection.anchor_offset;
    dict->SetByDottedPath("TreeData.textSelStartOffset", anchor_offset);
  }
  int focus_id = unignored_selection.focus_object_id;
  if (id == focus_id) {
    int focus_offset = unignored_selection.focus_offset;
    dict->SetByDottedPath("TreeData.textSelEndOffset", focus_offset);
  }

  std::vector<std::string> actions_strings;
  for (int32_t action_index =
           static_cast<int32_t>(ax::mojom::Action::kNone) + 1;
       action_index <= static_cast<int32_t>(ax::mojom::Action::kMaxValue);
       ++action_index) {
    auto action = static_cast<ax::mojom::Action>(action_index);
    if (node.HasAction(action)) {
      actions_strings.push_back(ui::ToString(action));
    }
  }
  if (!actions_strings.empty()) {
    dict->Set("actions", base::JoinString(actions_strings, ","));
  }
}

void AccessibilityTreeFormatterBlink::AddProperties(
    const ui::AXNode& node,
    base::Value::Dict* dict) const {
  int id = node.id();
  dict->Set("id", id);
  dict->Set("internalRole", ui::ToString(node.GetRole()));

  gfx::Rect bounds = gfx::ToEnclosingRect(node.data().relative_bounds.bounds);
  dict->Set("boundsX", bounds.x());
  dict->Set("boundsY", bounds.y());
  dict->Set("boundsWidth", bounds.width());
  dict->Set("boundsHeight", bounds.height());

  // TODO(kschmi): Add support for the following (potentially via AXTree):
  //  GetClippedRootFrameBoundsRect
  //    pageBoundsX
  //    pageBoundsY
  //    pageBoundsWidth
  //    pageBoundsHeight
  //  GetUnclippedRootFrameBoundsRect
  //    unclippedBoundsX
  //    unclippedBoundsY
  //    unclippedBoundsWidth
  //    unclippedBoundsHeight
  //    STATE_OFFSCREEN
  //  ComputeAttribute
  //    TableCellAriaColIndex
  //    TableCellAriaRowIndex
  //    TableCellColIndex
  //    TableCellRowIndex
  //    TableCellColSpan
  //    TableCellRowSpan
  //    TableRowRowIndex
  //    TableColCount
  //    TableRowCount
  //    TableAriaColCount
  //    TableAriaRowCount
  //    PosInSet
  //    SetSize
  //    SetSize

  dict->Set("transform",
            node.data().relative_bounds.transform &&
                !node.data().relative_bounds.transform->IsIdentity());

  for (int32_t state_index = static_cast<int32_t>(ax::mojom::State::kNone);
       state_index <= static_cast<int32_t>(ax::mojom::State::kMaxValue);
       ++state_index) {
    auto state = static_cast<ax::mojom::State>(state_index);
    if (node.HasState(state)) {
      dict->SetByDottedPath(ui::ToString(state), true);
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::StringAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::StringAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::StringAttribute>(attr_index);
    auto maybe_value = GetStringAttribute(node, attr);
    if (maybe_value.has_value()) {
      dict->SetByDottedPath(ui::ToString(attr), maybe_value.value());
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::IntAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::IntAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntAttribute>(attr_index);
    if (node.HasIntAttribute(attr)) {
      int32_t value = node.GetIntAttribute(attr);
      dict->SetByDottedPath(ui::ToString(attr),
                            IntAttrToString(node, attr, value));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::FloatAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::FloatAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::FloatAttribute>(attr_index);
    if (node.HasFloatAttribute(attr) &&
        std::isfinite(node.GetFloatAttribute(attr))) {
      dict->SetByDottedPath(ui::ToString(attr), node.GetFloatAttribute(attr));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::BoolAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::BoolAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::BoolAttribute>(attr_index);
    if (node.HasBoolAttribute(attr)) {
      dict->SetByDottedPath(ui::ToString(attr), node.GetBoolAttribute(attr));
    }
  }

  for (int32_t attr_index =
           static_cast<int32_t>(ax::mojom::IntListAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::IntListAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntListAttribute>(attr_index);
    if (node.HasIntListAttribute(attr)) {
      const std::vector<int32_t>& values = node.GetIntListAttribute(attr);
      base::Value::List value_list;
      for (auto value : values) {
        if (ui::IsNodeIdIntListAttribute(attr)) {
          ui::AXTreeID tree_id = node.tree()->GetAXTreeID();
          ui::AXNode* target =
              ui::AXTreeManager::FromID(tree_id)->GetNode(node.id());

          if (target) {
            value_list.Append(ui::ToString(target->GetRole()));
          } else {
            value_list.Append("null");
          }
        } else {
          value_list.Append(value);
        }
      }
      dict->Set(ui::ToString(attr), std::move(value_list));
    }
  }

  //  Check for relevant rich text selection info in AXTreeData
  ui::AXSelection unignored_selection = node.tree()->GetUnignoredSelection();
  int anchor_id = unignored_selection.anchor_object_id;
  if (id == anchor_id) {
    int anchor_offset = unignored_selection.anchor_offset;
    dict->SetByDottedPath("TreeData.textSelStartOffset", anchor_offset);
  }
  int focus_id = unignored_selection.focus_object_id;
  if (id == focus_id) {
    int focus_offset = unignored_selection.focus_offset;
    dict->SetByDottedPath("TreeData.textSelEndOffset", focus_offset);
  }

  std::vector<std::string> actions_strings;
  for (int32_t action_index =
           static_cast<int32_t>(ax::mojom::Action::kNone) + 1;
       action_index <= static_cast<int32_t>(ax::mojom::Action::kMaxValue);
       ++action_index) {
    auto action = static_cast<ax::mojom::Action>(action_index);
    if (node.HasAction(action)) {
      actions_strings.push_back(ui::ToString(action));
    }
  }
  if (!actions_strings.empty()) {
    dict->Set("actions", base::JoinString(actions_strings, ","));
  }
}

std::string AccessibilityTreeFormatterBlink::ProcessTreeForOutput(
    const base::Value::Dict& dict) const {
  const std::string* error_value = dict.FindString("error");
  if (error_value) {
    return *error_value;
  }

  std::string line;

  std::string id_value = base::NumberToString(dict.FindInt("id").value_or(0));
  if (show_ids()) {  // Show id on every line.
    WriteAttribute(true, id_value, &line);
  } else {  // Show id if @BlINK-ALLOW:id#=* specified.
    WriteAttribute(false, std::string("id#=") + id_value, &line);
  }

  const std::string* role_value = dict.FindString("internalRole");
  if (role_value) {
    WriteAttribute(true, *role_value, &line);
  }

  for (int state_index = static_cast<int32_t>(ax::mojom::State::kNone);
       state_index <= static_cast<int32_t>(ax::mojom::State::kMaxValue);
       ++state_index) {
    auto state = static_cast<ax::mojom::State>(state_index);
    const base::Value* value = dict.Find(ui::ToString(state));
    if (!value) {
      continue;
    }

    WriteAttribute(false, ui::ToString(state), &line);
  }

  // Offscreen and Focused states are not in the state list.
  if (dict.FindBool(STATE_OFFSCREEN).value_or(false)) {
    WriteAttribute(false, STATE_OFFSCREEN, &line);
  }
  if (dict.FindBool(STATE_FOCUSED).value_or(false)) {
    WriteAttribute(false, STATE_FOCUSED, &line);
  }

  if (dict.Find("boundsX") && dict.Find("boundsY")) {
    WriteAttribute(false,
                   FormatCoordinates(dict, "location", "boundsX", "boundsY"),
                   &line);
  }

  if (dict.Find("boundsWidth") && dict.Find("boundsHeight")) {
    WriteAttribute(
        false, FormatCoordinates(dict, "size", "boundsWidth", "boundsHeight"),
        &line);
  }

  if (!dict.FindBool("ignored").value_or(false)) {
    if (dict.Find("pageBoundsX") && dict.Find("pageBoundsY")) {
      WriteAttribute(
          false,
          FormatCoordinates(dict, "pageLocation", "pageBoundsX", "pageBoundsY"),
          &line);
    }
    if (dict.Find("pageBoundsWidth") && dict.Find("pageBoundsHeight")) {
      WriteAttribute(false,
                     FormatCoordinates(dict, "pageSize", "pageBoundsWidth",
                                       "pageBoundsHeight"),
                     &line);
    }
    if (dict.Find("unclippedBoundsX") && dict.Find("unclippedBoundsY")) {
      WriteAttribute(false,
                     FormatCoordinates(dict, "unclippedLocation",
                                       "unclippedBoundsX", "unclippedBoundsY"),
                     &line);
    }
    if (dict.Find("unclippedBoundsWidth") &&
        dict.Find("unclippedBoundsHeight")) {
      WriteAttribute(
          false,
          FormatCoordinates(dict, "unclippedSize", "unclippedBoundsWidth",
                            "unclippedBoundsHeight"),
          &line);
    }
  }

  if (dict.FindBool("transform").value_or(false)) {
    WriteAttribute(false, "transform", &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::StringAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::StringAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::StringAttribute>(attr_index);
    const std::string* string_value = dict.FindString(ui::ToString(attr));
    if (!string_value) {
      continue;
    }
    WriteAttribute(false,
                   base::StringPrintf("%s='%s'", ui::ToString(attr),
                                      string_value->c_str()),
                   &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::IntAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::IntAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntAttribute>(attr_index);
    const std::string* string_value = dict.FindString(ui::ToString(attr));
    if (!string_value) {
      continue;
    }
    WriteAttribute(
        false,
        base::StringPrintf("%s=%s", ui::ToString(attr), string_value->c_str()),
        &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::BoolAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::BoolAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::BoolAttribute>(attr_index);
    std::optional<bool> bool_value = dict.FindBool(ui::ToString(attr));
    if (!bool_value.has_value()) {
      continue;
    }
    WriteAttribute(false,
                   base::StringPrintf("%s=%s", ui::ToString(attr),
                                      *bool_value ? "true" : "false"),
                   &line);
  }

  for (int attr_index = static_cast<int32_t>(ax::mojom::FloatAttribute::kNone);
       attr_index <= static_cast<int32_t>(ax::mojom::FloatAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::FloatAttribute>(attr_index);
    std::optional<double> float_value = dict.FindDouble(ui::ToString(attr));
    if (!float_value) {
      continue;
    }
    WriteAttribute(
        false, base::StringPrintf("%s=%.2f", ui::ToString(attr), *float_value),
        &line);
  }

  for (int attr_index =
           static_cast<int32_t>(ax::mojom::IntListAttribute::kNone);
       attr_index <=
       static_cast<int32_t>(ax::mojom::IntListAttribute::kMaxValue);
       ++attr_index) {
    auto attr = static_cast<ax::mojom::IntListAttribute>(attr_index);
    const base::Value::List* value = dict.FindList(ui::ToString(attr));
    if (!value) {
      continue;
    }
    const base::Value::List& list = *value;
    std::string attr_string(ui::ToString(attr));
    attr_string.push_back('=');
    for (size_t i = 0; i < list.size(); ++i) {
      if (i > 0) {
        attr_string += ",";
      }
      if (ui::IsNodeIdIntListAttribute(attr)) {
        if (list[i].is_string()) {
          attr_string += list[i].GetString();
        }
      } else {
        attr_string += base::NumberToString(list[i].GetIfInt().value_or(0));
      }
    }
    WriteAttribute(false, attr_string, &line);
  }

  const std::string* actions_value = dict.FindString("actions");
  if (actions_value) {
    WriteAttribute(
        false, base::StringPrintf("actions=%s", actions_value->c_str()), &line);
  }

  for (const char* attribute_name : TREE_DATA_ATTRIBUTES) {
    const base::Value* value = dict.FindByDottedPath(attribute_name);
    if (!value) {
      continue;
    }

    switch (value->type()) {
      case base::Value::Type::STRING:
        WriteAttribute(false,
                       base::StringPrintf("%s=%s", attribute_name,
                                          value->GetString().c_str()),
                       &line);
        break;
      case base::Value::Type::INTEGER:
        WriteAttribute(
            false, base::StringPrintf("%s=%d", attribute_name, value->GetInt()),
            &line);
        break;
      case base::Value::Type::DOUBLE:
        WriteAttribute(
            false,
            base::StringPrintf("%s=%.2f", attribute_name, value->GetDouble()),
            &line);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  return line;
}

}  // namespace content
