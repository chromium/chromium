// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/accessibility_tools_utils_mac.h"

using base::SysNSStringToUTF8;

namespace content {
namespace a11y {

namespace {

#define INT_FAIL(property_node, msg)                              \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to Int: " << msg;                               \
  return nil;

#define INTARRAY_FAIL(property_node, msg)                         \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to IntArray: " << msg;                          \
  return nil;

#define NSRANGE_FAIL(property_node, msg)                          \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to NSRange: " << msg;                           \
  return nil;

#define UIELEMENT_FAIL(property_node, msg)                        \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to UIElement: " << msg;                         \
  return nil;

#define TEXTMARKER_FAIL(property_node, msg)                                    \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value              \
             << " to AXTextMarker: " << msg                                    \
             << ". Expected format: {anchor, offset, affinity}, where anchor " \
                "is :line_num, offset is integer, affinity is either down, "   \
                "up or none";                                                  \
  return nil;

}  // namespace

// Line indexers

LineIndexer::LineIndexer(const gfx::NativeViewAccessible node) {
  int counter = 0;
  Build(node, &counter);
}

LineIndexer::~LineIndexer() {}

std::string LineIndexer::IndexBy(const gfx::NativeViewAccessible node) const {
  std::string line_index = ":unknown";
  if (IsBrowserAccessibilityCocoa(node)) {
    auto iter = map.find(node);
    if (iter != map.end()) {
      line_index = iter->second;
    }
  } else if (IsAXUIElement(node)) {
    for (auto& iter : map) {
      if (CFEqual(iter.first, node)) {
        line_index = iter.second;
        break;
      }
    }
  }
  return line_index;
}

gfx::NativeViewAccessible LineIndexer::NodeBy(
    const std::string& line_index) const {
  for (std::pair<const gfx::NativeViewAccessible, std::string> item : map) {
    if (item.second == line_index) {
      return item.first;
    }
  }
  return nil;
}

void LineIndexer::Build(const gfx::NativeViewAccessible node, int* counter) {
  const std::string line_index =
      std::string(1, ':') + base::NumberToString(++(*counter));
  map.insert({node, line_index});
  NSArray* children = ChildrenOf(node);
  for (gfx::NativeViewAccessible child in children) {
    Build(child, counter);
  }
}

// AttributeInvoker

AttributeInvoker::AttributeInvoker(const id node,
                                   const LineIndexer* line_indexer)
    : node(node), line_indexer(line_indexer) {
  attributes = AttributeNamesOf(node);
  parameterized_attributes = ParameterizedAttributeNamesOf(node);
}

OptionalNSObject AttributeInvoker::Invoke(
    const PropertyNode& property_node) const {
  // Attributes
  for (NSString* attribute : attributes) {
    if (property_node.IsMatching(SysNSStringToUTF8(attribute))) {
      return OptionalNSObject::NotNullOrNotApplicable(
          AttributeValueOf(node, attribute));
    }
  }

  // Parameterized attributes
  for (NSString* attribute : parameterized_attributes) {
    if (property_node.IsMatching(SysNSStringToUTF8(attribute))) {
      OptionalNSObject param = ParamByPropertyNode(property_node);
      if (param.IsNotNil()) {
        return OptionalNSObject(
            ParameterizedAttributeValueOf(node, attribute, *param));
      }
      return param;
    }
  }

  return OptionalNSObject::NotApplicable();
}

OptionalNSObject AttributeInvoker::ParamByPropertyNode(
    const PropertyNode& property_node) const {
  // NSAccessibility attributes always take a single parameter.
  if (property_node.parameters.size() != 1) {
    LOG(ERROR) << "Failed to parse " << property_node.original_property
               << ": single parameter is expected";
    return OptionalNSObject::Error();
  }

  // Nested attribute case: attempt to invoke an attribute for an argument node.
  const PropertyNode& arg_node = property_node.parameters[0];
  OptionalNSObject subvalue = Invoke(arg_node);
  if (!subvalue.IsNotApplicable()) {
    return subvalue;
  }

  // Otherwise parse argument node value.
  const std::string& property_name = property_node.name_or_value;
  if (property_name == "AXLineForIndex" ||
      property_name == "AXTextMarkerForIndex") {  // Int
    return OptionalNSObject::NotNilOrError(PropertyNodeToInt(arg_node));
  }
  if (property_name == "AXCellForColumnAndRow") {  // IntArray
    return OptionalNSObject::NotNilOrError(PropertyNodeToIntArray(arg_node));
  }
  if (property_name == "AXStringForRange") {  // NSRange
    return OptionalNSObject::NotNilOrError(PropertyNodeToRange(arg_node));
  }
  if (property_name == "AXIndexForChildUIElement") {  // UIElement
    return OptionalNSObject::NotNilOrError(PropertyNodeToUIElement(arg_node));
  }
  if (property_name == "AXIndexForTextMarker") {  // TextMarker
    return OptionalNSObject::NotNilOrError(PropertyNodeToTextMarker(arg_node));
  }
  if (property_name == "AXStringForTextMarkerRange") {  // TextMarkerRange
    return OptionalNSObject::NotNilOrError(
        PropertyNodeToTextMarkerRange(arg_node));
  }

  return OptionalNSObject::NotApplicable();
}

// NSNumber. Format: integer.
NSNumber* AttributeInvoker::PropertyNodeToInt(
    const PropertyNode& intnode) const {
  base::Optional<int> param = intnode.AsInt();
  if (!param) {
    INT_FAIL(intnode, "not a number")
  }
  return [NSNumber numberWithInt:*param];
}

// NSArray of two NSNumber. Format: [integer, integer].
NSArray* AttributeInvoker::PropertyNodeToIntArray(
    const PropertyNode& arraynode) const {
  if (arraynode.name_or_value != "[]") {
    INTARRAY_FAIL(arraynode, "not array")
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.parameters.size()];
  for (const auto& paramnode : arraynode.parameters) {
    base::Optional<int> param = paramnode.AsInt();
    if (!param) {
      INTARRAY_FAIL(arraynode, paramnode.name_or_value + " is not a number")
    }
    [array addObject:@(*param)];
  }
  return array;
}

// NSRange. Format: {loc: integer, len: integer}.
NSValue* AttributeInvoker::PropertyNodeToRange(
    const PropertyNode& dictnode) const {
  if (!dictnode.IsDict()) {
    NSRANGE_FAIL(dictnode, "dictionary is expected")
  }

  base::Optional<int> loc = dictnode.FindIntKey("loc");
  if (!loc) {
    NSRANGE_FAIL(dictnode, "no loc or loc is not a number")
  }

  base::Optional<int> len = dictnode.FindIntKey("len");
  if (!len) {
    NSRANGE_FAIL(dictnode, "no len or len is not a number")
  }

  return [NSValue valueWithRange:NSMakeRange(*loc, *len)];
}

// UIElement. Format: :line_num.
gfx::NativeViewAccessible AttributeInvoker::PropertyNodeToUIElement(
    const PropertyNode& uielement_node) const {
  gfx::NativeViewAccessible uielement =
      line_indexer->NodeBy(uielement_node.name_or_value);
  if (!uielement) {
    UIELEMENT_FAIL(uielement_node,
                   "no corresponding UIElement was found in the tree")
  }
  return uielement;
}

id AttributeInvoker::DictNodeToTextMarker(const PropertyNode& dictnode) const {
  if (!dictnode.IsDict()) {
    TEXTMARKER_FAIL(dictnode, "dictionary is expected")
  }
  if (dictnode.parameters.size() != 3) {
    TEXTMARKER_FAIL(dictnode, "wrong number of dictionary elements")
  }

  BrowserAccessibilityCocoa* anchor_cocoa =
      line_indexer->NodeBy(dictnode.parameters[0].name_or_value);
  if (!anchor_cocoa) {
    TEXTMARKER_FAIL(dictnode, "1st argument: wrong anchor")
  }

  base::Optional<int> offset = dictnode.parameters[1].AsInt();
  if (!offset) {
    TEXTMARKER_FAIL(dictnode, "2nd argument: wrong offset")
  }

  ax::mojom::TextAffinity affinity;
  const std::string& affinity_str = dictnode.parameters[2].name_or_value;
  if (affinity_str == "none") {
    affinity = ax::mojom::TextAffinity::kNone;
  } else if (affinity_str == "down") {
    affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (affinity_str == "up") {
    affinity = ax::mojom::TextAffinity::kUpstream;
  } else {
    TEXTMARKER_FAIL(dictnode, "3rd argument: wrong affinity")
  }

  return content::AXTextMarkerFrom(anchor_cocoa, *offset, affinity);
}

id AttributeInvoker::PropertyNodeToTextMarker(
    const PropertyNode& dictnode) const {
  return DictNodeToTextMarker(dictnode);
}

id AttributeInvoker::PropertyNodeToTextMarkerRange(
    const PropertyNode& rangenode) const {
  if (!rangenode.IsDict()) {
    TEXTMARKER_FAIL(rangenode, "dictionary is expected")
  }

  const PropertyNode* anchornode = rangenode.FindKey("anchor");
  if (!anchornode) {
    TEXTMARKER_FAIL(rangenode, "no anchor")
  }

  id anchor_textmarker = DictNodeToTextMarker(*anchornode);
  if (!anchor_textmarker) {
    TEXTMARKER_FAIL(rangenode, "failed to parse anchor")
  }

  const PropertyNode* focusnode = rangenode.FindKey("focus");
  if (!focusnode) {
    TEXTMARKER_FAIL(rangenode, "no focus")
  }

  id focus_textmarker = DictNodeToTextMarker(*focusnode);
  if (!focus_textmarker) {
    TEXTMARKER_FAIL(rangenode, "failed to parse focus")
  }

  return content::AXTextMarkerRangeFrom(anchor_textmarker, focus_textmarker);
}

}  // namespace a11y
}  // namespace content
