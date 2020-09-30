// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"

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
using content::a11y::IsAXUIElement;
using content::a11y::IsBrowserAccessibilityCocoa;
using content::a11y::LineIndexer;
using content::a11y::OptionalNSObject;
using std::string;

namespace content {

namespace {

const char kPositionDictAttr[] = "position";
const char kXCoordDictAttr[] = "x";
const char kYCoordDictAttr[] = "y";
const char kSizeDictAttr[] = "size";
const char kWidthDictAttr[] = "width";
const char kHeightDictAttr[] = "height";
const char kRangeLocDictAttr[] = "loc";
const char kRangeLenDictAttr[] = "len";

const char kSetKeyPrefixDictAttr[] = "_setkey_";
const char kConstValuePrefix[] = "_const_";
const char kNULLValue[] = "_const_NULL";
const char kFailedToParseArgsError[] = "_const_ERROR:FAILED_TO_PARSE_ARGS";

}  // namespace

class AccessibilityTreeFormatterMac : public AccessibilityTreeFormatterBase {
 public:
  explicit AccessibilityTreeFormatterMac();
  ~AccessibilityTreeFormatterMac() override;

  void AddDefaultFilters(
      std::vector<PropertyFilter>* property_filters) override;

  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTree(
      BrowserAccessibility* root) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForWindow(
      gfx::AcceleratedWidget widget) override;
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForSelector(
      const TreeSelector& selector) override;

 private:
  std::unique_ptr<base::DictionaryValue> BuildAccessibilityTreeForAXUIElement(
      AXUIElementRef node) const;

  void RecursiveBuildAccessibilityTree(const id node,
                                       const LineIndexer* line_indexer,
                                       base::DictionaryValue* dict) const;

  base::FilePath::StringType GetExpectedFileSuffix() override;
  const std::string GetAllowEmptyString() override;
  const std::string GetAllowString() override;
  const std::string GetDenyString() override;
  const std::string GetDenyNodeString() override;
  const std::string GetRunUntilEventString() override;

  void AddProperties(const id node,
                     const LineIndexer* line_indexer,
                     base::Value* dict) const;

  // Invokes an attributes by a property node.
  OptionalNSObject InvokeAttributeFor(
      const BrowserAccessibilityCocoa* cocoa_node,
      const PropertyNode& property_node,
      const LineIndexer* line_indexer) const;

  base::Value PopulateSize(const BrowserAccessibilityCocoa*) const;
  base::Value PopulatePosition(const BrowserAccessibilityCocoa*) const;
  base::Value PopulatePoint(NSPoint) const;
  base::Value PopulateSize(NSSize) const;
  base::Value PopulateRect(NSRect) const;
  base::Value PopulateRange(NSRange) const;
  base::Value PopulateTextPosition(
      BrowserAccessibilityPosition::AXPositionInstance::pointer,
      const LineIndexer*) const;
  base::Value PopulateTextMarkerRange(id, const LineIndexer*) const;
  base::Value PopulateObject(id, const LineIndexer* line_indexer) const;
  base::Value PopulateArray(NSArray*, const LineIndexer* line_indexer) const;

  std::string NodeToLineIndex(id, const LineIndexer*) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node,
      base::DictionaryValue* filtered_dict_result = nullptr) override;

  std::string FormatAttributeValue(const base::Value& value);
};

// static
std::unique_ptr<AccessibilityTreeFormatter>
AccessibilityTreeFormatter::Create() {
  return std::make_unique<AccessibilityTreeFormatterMac>();
}

// static
std::vector<AccessibilityTreeFormatter::TestPass>
AccessibilityTreeFormatter::GetTestPasses() {
  return {
      {"blink", &AccessibilityTreeFormatterBlink::CreateBlink},
      {"mac", &AccessibilityTreeFormatter::Create},
  };
}

AccessibilityTreeFormatterMac::AccessibilityTreeFormatterMac() {}

AccessibilityTreeFormatterMac::~AccessibilityTreeFormatterMac() {}

void AccessibilityTreeFormatterMac::AddDefaultFilters(
    std::vector<PropertyFilter>* property_filters) {
  static NSArray* default_attributes = [@[
    @"AXAutocompleteValue=*", @"AXDescription=*", @"AXRole=*", @"AXTitle=*",
    @"AXTitleUIElement=*", @"AXHelp=*", @"AXValue=*"
  ] retain];

  for (NSString* attribute : default_attributes) {
    AddPropertyFilter(property_filters, SysNSStringToUTF8(attribute));
  }

  if (show_ids()) {
    AddPropertyFilter(property_filters, "id");
  }
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTree(
    BrowserAccessibility* root) {
  DCHECK(root);
  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root);
  LineIndexer line_indexer(cocoa_root);
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(cocoa_root, &line_indexer, dict.get());
  return dict;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForWindow(
    gfx::AcceleratedWidget widget) {
  return BuildAccessibilityTreeForAXUIElement(
      AXUIElementCreateApplication(widget));
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForSelector(
    const TreeSelector& selector) {
  AXUIElementRef node = nil;
  std::tie(node, std::ignore) = a11y::FindAXUIElement(selector);
  return node != nil ? BuildAccessibilityTreeForAXUIElement(node) : nil;
}

std::unique_ptr<base::DictionaryValue>
AccessibilityTreeFormatterMac::BuildAccessibilityTreeForAXUIElement(
    AXUIElementRef node) const {
  LineIndexer line_indexer(static_cast<id>(node));
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  RecursiveBuildAccessibilityTree(static_cast<id>(node), &line_indexer,
                                  dict.get());
  return dict;
}

void AccessibilityTreeFormatterMac::RecursiveBuildAccessibilityTree(
    const id node,
    const LineIndexer* line_indexer,
    base::DictionaryValue* dict) const {
  AddProperties(node, line_indexer, dict);

  NSArray* children = ChildrenOf(node);
  auto child_dict_list = std::make_unique<base::ListValue>();
  for (id child in children) {
    std::unique_ptr<base::DictionaryValue> child_dict(
        new base::DictionaryValue);
    RecursiveBuildAccessibilityTree(child, line_indexer, child_dict.get());
    child_dict_list->Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(child_dict_list));
}

void AccessibilityTreeFormatterMac::AddProperties(
    const id node,
    const LineIndexer* line_indexer,
    base::Value* dict) const {
  // Chromium tree special processing
  if (IsBrowserAccessibilityCocoa(node)) {
    BrowserAccessibilityCocoa* cocoa_node =
        static_cast<BrowserAccessibilityCocoa*>(node);

    // DOM element id
    BrowserAccessibility* owner_node = [cocoa_node owner];
    dict->SetKey("id",
                 base::Value(base::NumberToString16(owner_node->GetId())));

    // Position and size
    dict->SetPath(kPositionDictAttr, PopulatePosition(cocoa_node));
    dict->SetPath(kSizeDictAttr, PopulateSize(cocoa_node));
  }

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
  for (const PropertyNode& property_node : PropertyFilterNodesFor(line_index)) {
    AttributeInvoker invoker(node, line_indexer);
    OptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsNotApplicable()) {
      continue;
    }
    if (value.IsError()) {
      dict->SetPath(property_node.original_property,
                    base::Value(kFailedToParseArgsError));
      continue;
    }
    dict->SetPath(property_node.original_property,
                  PopulateObject(*value, line_indexer));
  }
}

base::Value AccessibilityTreeFormatterMac::PopulateSize(
    const BrowserAccessibilityCocoa* cocoa_node) const {
  base::Value size(base::Value::Type::DICTIONARY);
  NSSize node_size = [[cocoa_node size] sizeValue];
  size.SetIntPath(kHeightDictAttr, static_cast<int>(node_size.height));
  size.SetIntPath(kWidthDictAttr, static_cast<int>(node_size.width));
  return size;
}

base::Value AccessibilityTreeFormatterMac::PopulatePosition(
    const BrowserAccessibilityCocoa* cocoa_node) const {
  BrowserAccessibility* node = [cocoa_node owner];
  BrowserAccessibilityManager* root_manager = node->manager()->GetRootManager();
  DCHECK(root_manager);

  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and
  // less confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  BrowserAccessibility* root = root_manager->GetRoot();
  BrowserAccessibilityCocoa* cocoa_root = ToBrowserAccessibilityCocoa(root);
  NSPoint root_position = [[cocoa_root position] pointValue];
  NSSize root_size = [[cocoa_root size] sizeValue];
  int root_top = -static_cast<int>(root_position.y + root_size.height);
  int root_left = static_cast<int>(root_position.x);

  NSPoint node_position = [[cocoa_node position] pointValue];
  NSSize node_size = [[cocoa_node size] sizeValue];

  base::Value position(base::Value::Type::DICTIONARY);
  position.SetIntPath(kXCoordDictAttr,
                      static_cast<int>(node_position.x - root_left));
  position.SetIntPath(
      kYCoordDictAttr,
      static_cast<int>(-node_position.y - node_size.height - root_top));
  return position;
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

  // NSRange
  if ([value isKindOfClass:[NSValue class]] &&
      0 == strcmp([value objCType], @encode(NSRange))) {
    return PopulateRange([value rangeValue]);
  }

  // AXTextMarker
  if (content::IsAXTextMarker(value)) {
    return PopulateTextPosition(content::AXTextMarkerToPosition(value).get(),
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
    BrowserAccessibilityPosition::AXPositionInstance::pointer position,
    const LineIndexer* line_indexer) const {
  if (position->IsNullPosition()) {
    return base::Value(kNULLValue);
  }

  BrowserAccessibility* anchor = position->GetAnchor();
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
    id object,
    const LineIndexer* line_indexer) const {
  auto range = content::AXTextMarkerRangeToRange(object);
  if (range.IsNull()) {
    return base::Value(kNULLValue);
  }

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetPath("anchor", PopulateTextPosition(range.anchor(), line_indexer));
  dict.SetPath("focus", PopulateTextPosition(range.focus(), line_indexer));
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
    const base::DictionaryValue& dict,
    base::DictionaryValue* filtered_dict_result) {
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

    // Special processing for position and size.
    if (item.first == kPositionDictAttr) {
      WriteAttribute(false,
                     FormatCoordinates(
                         base::Value::AsDictionaryValue(item.second),
                         kPositionDictAttr, kXCoordDictAttr, kYCoordDictAttr),
                     &line);
      continue;
    }
    if (item.first == kSizeDictAttr) {
      WriteAttribute(
          false,
          FormatCoordinates(base::Value::AsDictionaryValue(item.second),
                            kSizeDictAttr, kWidthDictAttr, kHeightDictAttr),
          &line);
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
    const base::Value& value) {
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

base::FilePath::StringType
AccessibilityTreeFormatterMac::GetExpectedFileSuffix() {
  return FILE_PATH_LITERAL("-expected-mac.txt");
}

const string AccessibilityTreeFormatterMac::GetAllowEmptyString() {
  return "@MAC-ALLOW-EMPTY:";
}

const string AccessibilityTreeFormatterMac::GetAllowString() {
  return "@MAC-ALLOW:";
}

const string AccessibilityTreeFormatterMac::GetDenyString() {
  return "@MAC-DENY:";
}

const string AccessibilityTreeFormatterMac::GetDenyNodeString() {
  return "@MAC-DENY-NODE:";
}

const std::string AccessibilityTreeFormatterMac::GetRunUntilEventString() {
  return "@MAC-RUN-UNTIL-EVENT:";
}

}  // namespace content

#pragma clang diagnostic pop
