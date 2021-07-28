// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

using ui::AXPropertyNode;

extern "C" {

CFTypeRef AXTextMarkerRangeCopyStartMarker(CFTypeRef);

CFTypeRef AXTextMarkerRangeCopyEndMarker(CFTypeRef);
}  // extern "C"

namespace content {
namespace a11y {

namespace {

#define INT_FAIL(property_node, msg)                              \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to Int: " << msg;

#define INTARRAY_FAIL(property_node, msg)                         \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to IntArray: " << msg;

#define NSRANGE_FAIL(property_node, msg)                          \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to NSRange: " << msg;

#define UIELEMENT_FAIL(property_node, msg)                        \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value \
             << " to UIElement: " << msg;

#define TEXTMARKER_FAIL(property_node, msg)                                    \
  LOG(ERROR) << "Failed to parse " << property_node.name_or_value              \
             << " to AXTextMarker: " << msg                                    \
             << ". Expected format: {anchor, offset, affinity}, where anchor " \
                "is :line_num, offset is integer, affinity is either down, "   \
                "up or none";

}  // namespace

// OptionalNSObject

std::string OptionalNSObject::ToString() const {
  if (IsNotApplicable()) {
    return "<n/a>";
  } else if (IsError()) {
    return "<error>";
  } else if (value == nil) {
    return "<nil>";
  } else {
    return base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", value]);
  }
}

// AttributeInvoker

AttributeInvoker::AttributeInvoker(const LineIndexer* line_indexer,
                                   std::map<std::string, id>* storage)
    : node(nullptr), line_indexer(line_indexer), storage_(storage) {}

AttributeInvoker::AttributeInvoker(const id node,
                                   const LineIndexer* line_indexer)
    : node(node), line_indexer(line_indexer), storage_(nullptr) {}

OptionalNSObject AttributeInvoker::Invoke(const AXPropertyNode& property_node,
                                          bool no_object_parse) const {
  // TODO(alexs): failing the tests when filters are incorrect is a good idea,
  // however crashing ax_dump tools on wrong input might be not. Figure out
  // a working solution that works nicely in both cases. Use LOG(ERROR) for now
  // as a console warning.

  // Executes a scripting statement coded in a given property node.
  // The statement represents a chainable sequence of attribute calls, where
  // each subsequent call is invoked on an object returned by a previous call.
  // For example, p.AXChildren[0].AXRole will unroll into a sequence of
  // `p.AXChildren`, `(p.AXChildren)[0]` and `((p.AXChildren)[0]).AXRole`.

  // Get an initial target to invoke an attribute for. First, check the storage
  // if it has an associated target for the property node, then query the tree
  // indexer if the property node refers to a DOM id or line index of
  // an accessible object. If the property node doesn't provide a target then
  // use the default one (if any, the default node is provided in case of
  // a tree dumping only, the scripts never have default target).
  id target = nil;

  // Case 1: try to get a target from the storage. The target may refer to
  // a variable which is kept in the storage. For example,
  // `text_leaf:= p.AXChildren[0]` will define `text_leaf` variable and put it
  // into the storage, and then the variable value will be extracted from
  // the storage for other instruction referring the variable, for example,
  // `text_leaf.AXRole`.
  if (storage_) {
    auto storage_iterator = storage_->find(property_node.name_or_value);
    if (storage_iterator != storage_->end()) {
      target = storage_iterator->second;
      if (!target) {
        LOG(ERROR) << "Stored " << property_node.name_or_value
                   << " target is null.";
        return OptionalNSObject::Error();
      }
    }
  }
  // Case 2: try to get target from the tree indexer. The target may refer to
  // an accessible element by DOM id or by a line number (:LINE_NUM format) in
  // a result accessible tree. The tree indexer keeps the mappings between
  // accesible elements and their DOM ids and line numbers.
  if (!target)
    target = line_indexer->NodeBy(property_node.name_or_value);

  // Case 3: no target either indicates an error or default target (if
  // applicable) or the property node is an object or a scalar value (for
  // example, `0` in `AXChildren[0]` or [3, 4] integer array).
  if (!target) {
    // If default target is given, i.e. |node| is not null, then the target is
    // deemed and we use the default target. This case is about ax tree dumping
    // where a scripting instruction with no target are used. For example,
    // `AXRole` property filter means it is applied to all nodes and `AXRole`
    // attribute should be called for all nodes in the tree.
    if (node) {
      if (property_node.IsTarget()) {
        LOG(ERROR) << "Failed to parse '" << property_node.name_or_value
                   << "' target in '" << property_node.ToFlatString() << "'";
        return OptionalNSObject::Error();
      }
    } else if (no_object_parse) {
      return OptionalNSObject::NotApplicable();
    } else {
      // Object or scalar case.
      target = PropertyNodeToNSObject(property_node);
      if (!target) {
        LOG(ERROR) << "Failed to parse '" << property_node.ToFlatString()
                   << "' to NSObject";
        return OptionalNSObject::Error();
      }
    }
  }

  // If target is deemed, then start from the given property node. Otherwise the
  // given property node is a target, and its next property node is a
  // method/property to invoke.
  auto* current_node = &property_node;
  if (target) {
    current_node = property_node.next.get();
  } else {
    target = node;
  }

  // Invoke the call chain.
  while (current_node) {
    auto target_optional = InvokeFor(target, *current_node);
    // Result of the current step is either null or error. Don't go any further.
    if (!target_optional.IsNotNil()) {
      return target_optional;
    }
    target = *target_optional;
    current_node = current_node->next.get();
  }

  // Variable case: store the variable value in the storage.
  if (!property_node.key.empty())
    (*storage_)[property_node.key] = target;

  return OptionalNSObject(target);
}

OptionalNSObject AttributeInvoker::InvokeFor(
    const id target,
    const AXPropertyNode& property_node) const {
  if (IsBrowserAccessibilityCocoa(target) || IsAXUIElement(target))
    return InvokeForAXElement(target, property_node);

  if (content::IsAXTextMarkerRange(target)) {
    return InvokeForAXTextMarkerRange(target, property_node);
  }

  if ([target isKindOfClass:[NSArray class]])
    return InvokeForArray(target, property_node);

  LOG(ERROR) << "Unexpected target type for " << property_node.ToFlatString();
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForAXElement(
    const id target,
    const AXPropertyNode& property_node) const {
  // Actions.
  if (property_node.name_or_value == "AXActionNames") {
    return OptionalNSObject::NotNullOrNotApplicable(ActionNamesOf(target));
  }
  if (property_node.name_or_value == "AXPerformAction") {
    OptionalNSObject param = ParamByPropertyNode(property_node);
    if (param.IsNotNil()) {
      PerformAction(target, *param);
      return OptionalNSObject::NotApplicable();
    }
    return OptionalNSObject::Error();
  }

  // Attributes.
  for (NSString* attribute : AttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      // Setter
      if (property_node.rvalue) {
        OptionalNSObject rvalue = Invoke(*property_node.rvalue);
        if (rvalue.IsNotNil()) {
          SetAttributeValueOf(target, attribute, *rvalue);
          return {rvalue};
        }
        return rvalue;
      }
      // Getter
      return OptionalNSObject::NotNullOrNotApplicable(
          AttributeValueOf(target, attribute));
    }
  }

  // Parameterized attributes.
  for (NSString* attribute : ParameterizedAttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      OptionalNSObject param = ParamByPropertyNode(property_node);
      if (param.IsNotNil()) {
        return OptionalNSObject(
            ParameterizedAttributeValueOf(target, attribute, *param));
      }
      return param;
    }
  }

  // Unmatched attribute. No error for a tree dump calls because the tree dump
  // sets generic property filters not depending on a node, so we can be called
  // for an attribute not supported by the node.
  if (node)
    return OptionalNSObject::NotApplicable();

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXElement in '"
             << property_node.ToFlatString() << "' statement";
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForAXTextMarkerRange(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "anchor")
    return OptionalNSObject(static_cast<id>(
        AXTextMarkerRangeCopyStartMarker(static_cast<CFTypeRef>(target))));

  if (property_node.name_or_value == "focus")
    return OptionalNSObject(static_cast<id>(
        AXTextMarkerRangeCopyEndMarker(static_cast<CFTypeRef>(target))));

  // Unmatched attribute. No error for a tree dump calls because the tree dump
  // sets generic property filters not depending on a node, so we can be called
  // for an attribute not supported by the node.
  if (node)
    return OptionalNSObject::NotApplicable();

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXTextMarkerRange in '"
             << property_node.ToFlatString() << "' statement";
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForArray(
    const id target,
    const AXPropertyNode& property_node) const {
  if (!property_node.IsArray() || property_node.arguments.size() != 1) {
    LOG(ERROR) << "Array operator[] is expected, got: "
               << property_node.ToString();
    return OptionalNSObject::Error();
  }

  absl::optional<int> maybe_index = property_node.arguments[0].AsInt();
  if (!maybe_index || *maybe_index < 0) {
    LOG(ERROR) << "Wrong index for array operator[], got: "
               << property_node.arguments[0].ToString();
    return OptionalNSObject::Error();
  }

  if (static_cast<int>([target count]) <= *maybe_index) {
    LOG(ERROR) << "Out of range array operator[] index, got: "
               << property_node.arguments[0].ToString()
               << ", length: " << [target count];
    return OptionalNSObject::Error();
  }

  return OptionalNSObject(target[*maybe_index]);
}

OptionalNSObject AttributeInvoker::GetValue(
    const std::string& property_name,
    const OptionalNSObject& param) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* attributes = ParameterizedAttributeNamesOf(node);
  if ([attributes containsObject:attribute]) {
    if (param.IsNotNil()) {
      return OptionalNSObject(
          ParameterizedAttributeValueOf(node, attribute, *param));
    } else {
      return param;
    }
  }
  return OptionalNSObject::NotApplicable();
}

OptionalNSObject AttributeInvoker::GetValue(
    const std::string& property_name) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* parameterized_attributes = AttributeNamesOf(node);
  if ([parameterized_attributes containsObject:attribute]) {
    return OptionalNSObject::NotNullOrNotApplicable(
        AttributeValueOf(node, attribute));
  }
  return OptionalNSObject::NotApplicable();
}

void AttributeInvoker::SetValue(const std::string& property_name,
                                const OptionalNSObject& value) const {
  NSString* attribute = base::SysUTF8ToNSString(property_name);
  NSArray* attributes = AttributeNamesOf(node);
  if ([attributes containsObject:attribute] &&
      IsAttributeSettable(node, attribute)) {
    SetAttributeValueOf(node, attribute, *value);
  }
}

OptionalNSObject AttributeInvoker::ParamByPropertyNode(
    const AXPropertyNode& property_node) const {
  // NSAccessibility attributes always take a single parameter.
  if (property_node.arguments.size() != 1) {
    LOG(ERROR) << "Failed to parse '" << property_node.ToFlatString()
               << "': single parameter is expected";
    return OptionalNSObject::Error();
  }

  // Nested attribute case: attempt to invoke an attribute for an argument node.
  const AXPropertyNode& arg_node = property_node.arguments[0];
  OptionalNSObject subvalue = Invoke(arg_node, /* no_object_parse= */ true);
  if (!subvalue.IsNotApplicable()) {
    return subvalue;
  }

  // Otherwise parse argument node value.
  const std::string& property_name = property_node.name_or_value;
  if (property_name == "AXLineForIndex" ||
      property_name == "AXTextMarkerForIndex") {  // Int
    return OptionalNSObject::NotNilOrError(PropertyNodeToInt(arg_node));
  }
  if (property_name == "AXPerformAction") {
    return OptionalNSObject::NotNilOrError(PropertyNodeToString(arg_node));
  }
  if (property_name == "AXCellForColumnAndRow") {  // IntArray
    return OptionalNSObject::NotNilOrError(PropertyNodeToIntArray(arg_node));
  }
  if (property_name ==
      "AXTextMarkerRangeForUnorderedTextMarkers") {  // TextMarkerArray
    return OptionalNSObject::NotNilOrError(
        PropertyNodeToTextMarkerArray(arg_node));
  }
  if (property_name == "AXStringForRange") {  // NSRange
    return OptionalNSObject::NotNilOrError(PropertyNodeToRange(arg_node));
  }
  if (property_name == "AXIndexForChildUIElement" ||
      property_name == "AXTextMarkerRangeForUIElement") {  // UIElement
    return OptionalNSObject::NotNilOrError(PropertyNodeToUIElement(arg_node));
  }
  if (property_name == "AXIndexForTextMarker" ||
      property_name == "AXNextWordEndTextMarkerForTextMarker" ||
      property_name ==
          "AXPreviousWordStartTextMarkerForTextMarker") {  // TextMarker
    return OptionalNSObject::NotNilOrError(PropertyNodeToTextMarker(arg_node));
  }
  if (property_name == "AXSelectedTextMarkerRangeAttribute" ||
      property_name == "AXStringForTextMarkerRange") {  // TextMarkerRange
    return OptionalNSObject::NotNilOrError(
        PropertyNodeToTextMarkerRange(arg_node));
  }

  return OptionalNSObject::NotApplicable();
}

id AttributeInvoker::PropertyNodeToNSObject(
    const AXPropertyNode& property_node) const {
  // Integer array
  id value = PropertyNodeToIntArray(property_node, false);
  if (value)
    return value;

  // NSRange
  value = PropertyNodeToRange(property_node, false);
  if (value)
    return value;

  // TextMarker
  value = PropertyNodeToTextMarker(property_node, true);
  if (value)
    return value;

  // TextMarker array
  value = PropertyNodeToTextMarkerArray(property_node, false);
  if (value)
    return value;

  // TextMarkerRange
  return PropertyNodeToTextMarkerRange(property_node, false);
}

// NSNumber. Format: integer.
NSNumber* AttributeInvoker::PropertyNodeToInt(const AXPropertyNode& intnode,
                                              bool log_failure) const {
  absl::optional<int> param = intnode.AsInt();
  if (!param) {
    if (log_failure)
      INT_FAIL(intnode, "not a number")
    return nil;
  }
  return [NSNumber numberWithInt:*param];
}

NSString* AttributeInvoker::PropertyNodeToString(const AXPropertyNode& strnode,
                                                 bool log_failure) const {
  std::string str = strnode.AsString();
  return base::SysUTF8ToNSString(str);
}

// NSArray of two NSNumber. Format: [integer, integer].
NSArray* AttributeInvoker::PropertyNodeToIntArray(
    const AXPropertyNode& arraynode,
    bool log_failure) const {
  if (!arraynode.IsArray()) {
    if (log_failure)
      INTARRAY_FAIL(arraynode, "not array")
    return nil;
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.arguments.size()];
  for (const auto& paramnode : arraynode.arguments) {
    absl::optional<int> param = paramnode.AsInt();
    if (!param) {
      if (log_failure)
        INTARRAY_FAIL(arraynode, paramnode.name_or_value + " is not a number")
      return nil;
    }
    [array addObject:@(*param)];
  }
  return array;
}

// NSArray of AXTextMarker objects.
NSArray* AttributeInvoker::PropertyNodeToTextMarkerArray(
    const AXPropertyNode& arraynode,
    bool log_failure) const {
  if (!arraynode.IsArray()) {
    if (log_failure)
      INTARRAY_FAIL(arraynode, "not array")
    return nil;
  }

  NSMutableArray* array =
      [[NSMutableArray alloc] initWithCapacity:arraynode.arguments.size()];
  for (const auto& paramnode : arraynode.arguments) {
    OptionalNSObject text_marker = Invoke(paramnode);
    if (!text_marker.IsNotNil()) {
      if (log_failure)
        INTARRAY_FAIL(arraynode,
                      paramnode.ToFlatString() + "is not a text marker")
      return nil;
    }
    [array addObject:(*text_marker)];
  }
  return array;
}

// NSRange. Format: {loc: integer, len: integer}.
NSValue* AttributeInvoker::PropertyNodeToRange(const AXPropertyNode& dictnode,
                                               bool log_failure) const {
  if (!dictnode.IsDict()) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "dictionary is expected")
    return nil;
  }

  absl::optional<int> loc = dictnode.FindIntKey("loc");
  if (!loc) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "no loc or loc is not a number")
    return nil;
  }

  absl::optional<int> len = dictnode.FindIntKey("len");
  if (!len) {
    if (log_failure)
      NSRANGE_FAIL(dictnode, "no len or len is not a number")
    return nil;
  }

  return [NSValue valueWithRange:NSMakeRange(*loc, *len)];
}

// UIElement. Format: :line_num.
gfx::NativeViewAccessible AttributeInvoker::PropertyNodeToUIElement(
    const AXPropertyNode& uielement_node,
    bool log_failure) const {
  gfx::NativeViewAccessible uielement =
      line_indexer->NodeBy(uielement_node.name_or_value);
  if (!uielement) {
    if (log_failure)
      UIELEMENT_FAIL(uielement_node,
                     "no corresponding UIElement was found in the tree")
    return nil;
  }
  return uielement;
}

id AttributeInvoker::DictNodeToTextMarker(const AXPropertyNode& dictnode,
                                          bool log_failure) const {
  if (!dictnode.IsDict()) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "dictionary is expected")
    return nil;
  }
  if (dictnode.arguments.size() != 3) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "wrong number of dictionary elements")
    return nil;
  }

  BrowserAccessibilityCocoa* anchor_cocoa =
      line_indexer->NodeBy(dictnode.arguments[0].name_or_value);
  if (!anchor_cocoa) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "1st argument: wrong anchor")
    return nil;
  }

  absl::optional<int> offset = dictnode.arguments[1].AsInt();
  if (!offset) {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "2nd argument: wrong offset")
    return nil;
  }

  ax::mojom::TextAffinity affinity;
  const std::string& affinity_str = dictnode.arguments[2].name_or_value;
  if (affinity_str == "none") {
    affinity = ax::mojom::TextAffinity::kNone;
  } else if (affinity_str == "down") {
    affinity = ax::mojom::TextAffinity::kDownstream;
  } else if (affinity_str == "up") {
    affinity = ax::mojom::TextAffinity::kUpstream;
  } else {
    if (log_failure)
      TEXTMARKER_FAIL(dictnode, "3rd argument: wrong affinity")
    return nil;
  }

  return content::AXTextMarkerFrom(anchor_cocoa, *offset, affinity);
}

id AttributeInvoker::PropertyNodeToTextMarker(const AXPropertyNode& dictnode,
                                              bool log_failure) const {
  return DictNodeToTextMarker(dictnode, log_failure);
}

id AttributeInvoker::PropertyNodeToTextMarkerRange(
    const AXPropertyNode& rangenode,
    bool log_failure) const {
  if (!rangenode.IsDict()) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "dictionary is expected")
    return nil;
  }

  const AXPropertyNode* anchornode = rangenode.FindKey("anchor");
  if (!anchornode) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "no anchor")
    return nil;
  }

  id anchor_textmarker = DictNodeToTextMarker(*anchornode);
  if (!anchor_textmarker) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "failed to parse anchor")
    return nil;
  }

  const AXPropertyNode* focusnode = rangenode.FindKey("focus");
  if (!focusnode) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "no focus")
    return nil;
  }

  id focus_textmarker = DictNodeToTextMarker(*focusnode);
  if (!focus_textmarker) {
    if (log_failure)
      TEXTMARKER_FAIL(rangenode, "failed to parse focus")
    return nil;
  }

  return content::AXTextMarkerRangeFrom(anchor_textmarker, focus_textmarker);
}

OptionalNSObject TextMarkerRangeGetStartMarker(const OptionalNSObject& obj) {
  if (!IsAXTextMarkerRange(*obj))
    return OptionalNSObject::NotApplicable();

  const BrowserAccessibility::AXRange range = AXTextMarkerRangeToAXRange(*obj);
  if (range.IsNull())
    return OptionalNSObject::Error();

  auto* manager =
      BrowserAccessibilityManager::FromID(range.anchor()->tree_id());
  DCHECK(manager) << "A non-null range should have an associated AX tree.";
  const BrowserAccessibility* node =
      manager->GetFromID(range.anchor()->anchor_id());
  DCHECK(node) << "A non-null range should have a non-null anchor node.";
  const BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(node);
  return OptionalNSObject::NotNilOrError(content::AXTextMarkerFrom(
      cocoa_node, range.anchor()->text_offset(), range.anchor()->affinity()));
}

OptionalNSObject TextMarkerRangeGetEndMarker(const OptionalNSObject& obj) {
  if (!IsAXTextMarkerRange(*obj))
    return OptionalNSObject::NotApplicable();

  const BrowserAccessibility::AXRange range = AXTextMarkerRangeToAXRange(*obj);
  if (range.IsNull())
    return OptionalNSObject::Error();

  auto* manager = BrowserAccessibilityManager::FromID(range.focus()->tree_id());
  DCHECK(manager) << "A non-null range should have an associated AX tree.";
  const BrowserAccessibility* node =
      manager->GetFromID(range.focus()->anchor_id());
  DCHECK(node) << "A non-null range should have a non-null focus node.";
  const BrowserAccessibilityCocoa* cocoa_node =
      ToBrowserAccessibilityCocoa(node);
  return OptionalNSObject::NotNilOrError(content::AXTextMarkerFrom(
      cocoa_node, range.focus()->text_offset(), range.focus()->affinity()));
}

OptionalNSObject MakePairArray(const OptionalNSObject& obj1,
                               const OptionalNSObject& obj2) {
  if (!obj1.IsNotNil() || !obj2.IsNotNil())
    return OptionalNSObject::Error();
  return OptionalNSObject::NotNilOrError(
      [NSArray arrayWithObjects:*obj1, *obj2, nil]);
}

}  // namespace a11y
}  // namespace content
