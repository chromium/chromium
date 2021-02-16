// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_mac.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/accessibility/accessibility_tools_utils_mac.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

// This file uses the deprecated NSObject accessibility interface.
// TODO(crbug.com/948844): Migrate to the new NSAccessibility interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

using base::StringPrintf;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using content::a11y::AttributeInvoker;
using content::a11y::AttributeNamesOf;
using content::a11y::AttributeValueOf;
using content::a11y::ChildrenOf;
using content::a11y::SizeOf;
using content::a11y::PositionOf;
using content::a11y::IsAXUIElement;
using content::a11y::IsBrowserAccessibilityCocoa;
using content::a11y::LineIndexer;
using content::a11y::OptionalNSObject;
using std::string;
using ui::AXPropertyFilter;
using ui::AXPropertyNode;

namespace content {

namespace {

const char kLocalPositionDictAttr[] = "LocalPosition";
const char kRangeLocDictAttr[] = "loc";
const char kRangeLenDictAttr[] = "len";

const char kSetKeyPrefixDictAttr[] = "_setkey_";
const char kConstValuePrefix[] = "_const_";
const char kNULLValue[] = "_const_NULL";
const char kFailedToParseError[] = "_const_ERROR:FAILED_TO_PARSE";

}  // namespace

AccessibilityTreeFormatterMac::AccessibilityTreeFormatterMac() = default;

AccessibilityTreeFormatterMac::~AccessibilityTreeFormatterMac() = default;

void AccessibilityTreeFormatterMac::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  static NSArray* default_attributes = [@[
    @"AXAutocompleteValue=*", @"AXDescription=*", @"AXRole=*", @"AXTitle=*",
    @"AXTitleUIElement=*", @"AXHelp=*", @"AXValue=*"
  ] retain];

  for (NSString* attribute : default_attributes) {
    AddPropertyFilter(property_filters, SysNSStringToUTF8(attribute));
  }

  if (show_ids()) {
    AddPropertyFilter(property_filters, "ChromeAXNodeId");
  }
}

base::Value AccessibilityTreeFormatterMac::BuildTree(
    ui::AXPlatformNodeDelegate* root) const {
  DCHECK(root);
  BrowserAccessibility* internal_root =
      BrowserAccessibility::FromAXPlatformNodeDelegate(root);
  return BuildTree(ToBrowserAccessibilityCocoa(internal_root));
}

base::Value AccessibilityTreeFormatterMac::BuildTreeForWindow(
    gfx::AcceleratedWidget widget) const {
  return BuildTreeForAXUIElement(AXUIElementCreateApplication(widget));
}

base::Value AccessibilityTreeFormatterMac::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  AXUIElementRef node = nil;
  std::tie(node, std::ignore) = a11y::FindAXUIElement(selector);
  if (node == nil) {
    return base::Value(base::Value::Type::DICTIONARY);
  }
  return BuildTreeForAXUIElement(node);
}

base::Value AccessibilityTreeFormatterMac::BuildTreeForAXUIElement(
    AXUIElementRef node) const {
  return BuildTree(static_cast<id>(node));
}

base::Value AccessibilityTreeFormatterMac::BuildTree(const id root) const {
  DCHECK(root);

  LineIndexer line_indexer(root);
  base::Value dict(base::Value::Type::DICTIONARY);

  NSPoint position = PositionOf(root);
  NSSize size = SizeOf(root);
  NSRect rect = NSMakeRect(position.x, position.y, size.width, size.height);

  EvaluateScripts(&line_indexer, &dict);
  RecursiveBuildTree(root, rect, &line_indexer, &dict);

  return dict;
}

void AccessibilityTreeFormatterMac::EvaluateScripts(
    const LineIndexer* line_indexer,
    base::Value* dict) const {
  base::Value scripts(base::Value::Type::LIST);
  for (const AXPropertyNode& property_node : ScriptPropertyNodes()) {
    AttributeInvoker invoker(line_indexer);
    OptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsNotApplicable()) {
      continue;
    }

    base::Value result = value.IsError() ? base::Value(kFailedToParseError)
                                         : PopulateObject(*value, line_indexer);

    std::string code = property_node.original_property;
    scripts.Append(code + "=" + FormatAttributeValue(result));
  }
  dict->SetPath(kScriptsDictAttr, std::move(scripts));
}

void AccessibilityTreeFormatterMac::RecursiveBuildTree(
    const id node,
    const NSRect& root_rect,
    const LineIndexer* line_indexer,
    base::Value* dict) const {
  AddProperties(node, root_rect, line_indexer, dict);

  NSArray* children = ChildrenOf(node);
  base::Value child_dict_list(base::Value::Type::LIST);
  for (id child in children) {
    base::Value child_dict(base::Value::Type::DICTIONARY);
    RecursiveBuildTree(child, root_rect, line_indexer, &child_dict);
    child_dict_list.Append(std::move(child_dict));
  }
  dict->SetPath(kChildrenDictAttr, std::move(child_dict_list));
}

void AccessibilityTreeFormatterMac::AddProperties(
    const id node,
    const NSRect& root_rect,
    const LineIndexer* line_indexer,
    base::Value* dict) const {
  // Chromium special attributes.
  dict->SetPath(kLocalPositionDictAttr, PopulateLocalPosition(node, root_rect));

  // Dump all attributes if match-all filter is specified.
  if (HasMatchAllPropertyFilter()) {
    NSArray* attributes = AttributeNamesOf(node);
    for (NSString* attribute : attributes) {
      dict->SetPath(
          SysNSStringToUTF8(attribute),
          PopulateObject(AttributeValueOf(node, attribute), line_indexer));
    }
    return;
  }

  // Otherwise dump attributes matching allow filters only.
  std::string line_index = line_indexer->IndexBy(node);
  for (const AXPropertyNode& property_node :
       PropertyFilterNodesFor(line_index)) {
    AttributeInvoker invoker(node, line_indexer);
    OptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsNotApplicable()) {
      continue;
    }
    if (value.IsError()) {
      dict->SetPath(property_node.original_property,
                    base::Value(kFailedToParseError));
      continue;
    }
    dict->SetPath(property_node.original_property,
                  PopulateObject(*value, line_indexer));
  }
}

base::Value AccessibilityTreeFormatterMac::PopulateLocalPosition(
    const id node,
    const NSRect& root_rect) const {
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and
  // less confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  int root_top = -static_cast<int>(root_rect.origin.y + root_rect.size.height);
  int root_left = static_cast<int>(root_rect.origin.x);

  NSPoint node_position = PositionOf(node);
  NSSize node_size = SizeOf(node);

  return PopulatePoint(NSMakePoint(
      static_cast<int>(node_position.x - root_left),
      static_cast<int>(-node_position.y - node_size.height - root_top)));
}

base::Value AccessibilityTreeFormatterMac::PopulateObject(
    id value,
    const LineIndexer* line_indexer) const {
  if (value == nil) {
    return base::Value(kNULLValue);
  }

  // NSArray
  if ([value isKindOfClass:[NSArray class]]) {
    return PopulateArray((NSArray*)value, line_indexer);
  }

  // NSNumber
  if ([value isKindOfClass:[NSNumber class]]) {
    return base::Value([value intValue]);
  }

  // NSRange, NSSize
  if ([value isKindOfClass:[NSValue class]]) {
    if (0 == strcmp([value objCType], @encode(NSRange))) {
      return PopulateRange([value rangeValue]);
    }
    if (0 == strcmp([value objCType], @encode(NSSize))) {
      return PopulateSize([value sizeValue]);
    }
  }

  // AXTextMarker
  if (content::IsAXTextMarker(value)) {
    return PopulateTextPosition(content::AXTextMarkerToAXPosition(value),
                                line_indexer);
  }

  // AXTextMarkerRange
  if (content::IsAXTextMarkerRange(value)) {
    return PopulateTextMarkerRange(value, line_indexer);
  }

  // AXValue
  if (CFGetTypeID(value) == AXValueGetTypeID()) {
    AXValueType type = AXValueGetType(static_cast<AXValueRef>(value));
    switch (type) {
      case kAXValueCGPointType: {
        NSPoint point;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &point)) {
          return PopulatePoint(point);
        }
      } break;
      case kAXValueCGSizeType: {
        NSSize size;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &size)) {
          return PopulateSize(size);
        }
      } break;
      case kAXValueCGRectType: {
        NSRect rect;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &rect)) {
          return PopulateRect(rect);
        }
      } break;
      case kAXValueCFRangeType: {
        NSRange range;
        if (AXValueGetValue(static_cast<AXValueRef>(value), type, &range)) {
          return PopulateRange(range);
        }
      } break;
      default:
        break;
    }
  }

  // Accessible object
  if (IsBrowserAccessibilityCocoa(value) || IsAXUIElement(value)) {
    return base::Value(NodeToLineIndex(value, line_indexer));
  }

  // Scalar value.
  return base::Value(
      SysNSStringToUTF16([NSString stringWithFormat:@"%@", value]));
}

base::Value AccessibilityTreeFormatterMac::PopulatePoint(
    NSPoint point_value) const {
  base::Value point(base::Value::Type::DICTIONARY);
  point.SetIntPath("x", static_cast<int>(point_value.x));
  point.SetIntPath("y", static_cast<int>(point_value.y));
  return point;
}

base::Value AccessibilityTreeFormatterMac::PopulateSize(
    NSSize size_value) const {
  base::Value size(base::Value::Type::DICTIONARY);
  size.SetIntPath("w", static_cast<int>(size_value.width));
  size.SetIntPath("h", static_cast<int>(size_value.height));
  return size;
}

base::Value AccessibilityTreeFormatterMac::PopulateRect(
    NSRect rect_value) const {
  base::Value rect(base::Value::Type::DICTIONARY);
  rect.SetIntPath("x", static_cast<int>(rect_value.origin.x));
  rect.SetIntPath("y", static_cast<int>(rect_value.origin.y));
  rect.SetIntPath("w", static_cast<int>(rect_value.size.width));
  rect.SetIntPath("h", static_cast<int>(rect_value.size.height));
  return rect;
}

base::Value AccessibilityTreeFormatterMac::PopulateRange(
    NSRange node_range) const {
  base::Value range(base::Value::Type::DICTIONARY);
  range.SetIntPath(kRangeLocDictAttr, static_cast<int>(node_range.location));
  range.SetIntPath(kRangeLenDictAttr, static_cast<int>(node_range.length));
  return range;
}

base::Value AccessibilityTreeFormatterMac::PopulateTextPosition(
    const BrowserAccessibility::AXPosition& position,
    const LineIndexer* line_indexer) const {
  if (position->IsNullPosition())
    return base::Value(kNULLValue);

  auto* manager = BrowserAccessibilityManager::FromID(position->tree_id());
  DCHECK(manager) << "A non-null position should have an associated AX tree.";
  BrowserAccessibility* anchor = manager->GetFromID(position->anchor_id());
  DCHECK(anchor) << "A non-null position should have a non-null anchor node.";
  BrowserAccessibilityCocoa* cocoa_anchor = ToBrowserAccessibilityCocoa(anchor);

  std::string affinity;
  switch (position->affinity()) {
    case ax::mojom::TextAffinity::kNone:
      affinity = "none";
      break;
    case ax::mojom::TextAffinity::kDownstream:
      affinity = "down";
      break;
    case ax::mojom::TextAffinity::kUpstream:
      affinity = "up";
      break;
  }

  base::Value set(base::Value::Type::DICTIONARY);
  const std::string setkey_prefix = kSetKeyPrefixDictAttr;
  set.SetStringPath(setkey_prefix + "index1_anchor",
                    NodeToLineIndex(cocoa_anchor, line_indexer));
  set.SetIntPath(setkey_prefix + "index2_offset", position->text_offset());
  set.SetStringPath(setkey_prefix + "index3_affinity",
                    kConstValuePrefix + affinity);
  return set;
}

base::Value AccessibilityTreeFormatterMac::PopulateTextMarkerRange(
    id marker_range,
    const LineIndexer* line_indexer) const {
  BrowserAccessibility::AXRange ax_range =
      content::AXTextMarkerRangeToAXRange(marker_range);
  if (ax_range.IsNull())
    return base::Value(kNULLValue);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetPath("anchor",
               PopulateTextPosition(ax_range.anchor()->Clone(), line_indexer));
  dict.SetPath("focus",
               PopulateTextPosition(ax_range.focus()->Clone(), line_indexer));
  return dict;
}

base::Value AccessibilityTreeFormatterMac::PopulateArray(
    NSArray* node_array,
    const LineIndexer* line_indexer) const {
  base::Value list(base::Value::Type::LIST);
  for (NSUInteger i = 0; i < [node_array count]; i++)
    list.Append(PopulateObject([node_array objectAtIndex:i], line_indexer));
  return list;
}

std::string AccessibilityTreeFormatterMac::NodeToLineIndex(
    id node,
    const LineIndexer* line_indexer) const {
  return kConstValuePrefix + line_indexer->IndexBy(node);
}

std::string AccessibilityTreeFormatterMac::ProcessTreeForOutput(
    const base::DictionaryValue& dict) const {
  std::string error_value;
  if (dict.GetString("error", &error_value))
    return error_value;

  std::string line;

  // AXRole and AXSubrole have own formatting and should be listed upfront.
  std::string role_attr = SysNSStringToUTF8(NSAccessibilityRoleAttribute);
  const std::string* value = dict.FindStringPath(role_attr);
  if (value) {
    WriteAttribute(true, *value, &line);
  }
  std::string subrole_attr = SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  value = dict.FindStringPath(subrole_attr);
  if (value) {
    if (*value == kNULLValue) {
      WriteAttribute(false, StringPrintf("%s=NULL", subrole_attr.c_str()),
                     &line);
    } else {
      WriteAttribute(
          false, StringPrintf("%s=%s", subrole_attr.c_str(), value->c_str()),
          &line);
    }
  }

  // Expose all other attributes.
  for (auto item : dict.DictItems()) {
    if (item.second.is_string() &&
        (item.first == role_attr || item.first == subrole_attr)) {
      continue;
    }

    // Special case: children.
    // Children are used to generate the tree
    // itself, thus no sense to expose them on each node.
    if (item.first == kChildrenDictAttr) {
      continue;
    }

    // Write formatted value.
    std::string formatted_value = FormatAttributeValue(item.second);
    WriteAttribute(
        false,
        StringPrintf("%s=%s", item.first.c_str(), formatted_value.c_str()),
        &line);
  }

  return line;
}

std::string AccessibilityTreeFormatterMac::FormatAttributeValue(
    const base::Value& value) const {
  // String.
  if (value.is_string()) {
    // Special handling for constants which are exposed as is, i.e. with no
    // quotation marks.
    std::string const_prefix = kConstValuePrefix;
    if (base::StartsWith(value.GetString(), const_prefix,
                         base::CompareCase::SENSITIVE)) {
      return value.GetString().substr(const_prefix.length());
    }
    return "'" + value.GetString() + "'";
  }

  // Integer.
  if (value.is_int()) {
    return base::NumberToString(value.GetInt());
  }

  // List: exposed as [value1, ..., valueN];
  if (value.is_list()) {
    std::string output;
    for (const auto& item : value.GetList()) {
      if (!output.empty()) {
        output += ", ";
      }
      output += FormatAttributeValue(item);
    }
    return "[" + output + "]";
  }

  // Dictionary. Exposed as {key1: value1, ..., keyN: valueN}. Set-like
  // dictionary is exposed as {value1, ..., valueN}.
  if (value.is_dict()) {
    const std::string setkey_prefix(kSetKeyPrefixDictAttr);
    std::string output;
    for (const auto& item : value.DictItems()) {
      if (!output.empty()) {
        output += ", ";
      }
      // Special set-like dictionaries handling: keys are prefixed by
      // "_setkey_".
      if (base::StartsWith(item.first, setkey_prefix,
                           base::CompareCase::SENSITIVE)) {
        output += FormatAttributeValue(item.second);
      } else {
        output += item.first + ": " + FormatAttributeValue(item.second);
      }
    }
    return "{" + output + "}";
  }
  return "";
}

}  // namespace content

#pragma clang diagnostic pop
