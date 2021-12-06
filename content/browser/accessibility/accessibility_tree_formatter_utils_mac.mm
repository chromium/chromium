// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_mac.h"

#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

using ui::AXPropertyNode;
using ui::AXTreeIndexerMac;

#if !defined(MAC_OS_VERSION_12_0) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0
using AXTextMarkerRangeRef = CFTypeRef;
using AXTextMarkerRef = CFTypeRef;
extern "C" {
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef);
}  // extern "C"
#endif

namespace ui {

// Template specialization Nit: ui::AXOptional<id>.
template <>
std::string ui::AXOptional<id>::ToString() const {
  if (IsNotNull())
    return base::SysNSStringToUTF8([NSString stringWithFormat:@"%@", value_]);
  return StateToString();
}

}  // namespace ui

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

using ui::AXActionNamesOf;
using ui::AXAttributeNamesOf;
using ui::AXAttributeValueOf;
using ui::IsNSAccessibilityElement;
using ui::IsAXUIElement;
using ui::IsValidAXAttribute;
using ui::AXParameterizedAttributeNamesOf;
using ui::AXParameterizedAttributeValueOf;
using ui::PerformAXAction;
using ui::PerformAXSelector;
using ui::SetAXAttributeValueOf;

// AttributeInvoker

AttributeInvoker::AttributeInvoker(const AXTreeIndexerMac* indexer,
                                   std::map<std::string, id>* storage)
    : node(nullptr), indexer_(indexer), storage_(storage) {}

AttributeInvoker::AttributeInvoker(const id node,
                                   const AXTreeIndexerMac* indexer)
    : node(node), indexer_(indexer), storage_(nullptr) {}

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
      if (!target)
        return OptionalNSObject(target);
    }
  }
  // Case 2: try to get target from the tree indexer. The target may refer to
  // an accessible element by DOM id or by a line number (:LINE_NUM format) in
  // a result accessible tree. The tree indexer keeps the mappings between
  // accesible elements and their DOM ids and line numbers.
  if (!target)
    target = indexer_->NodeBy(property_node.name_or_value);

  // Case 3: no target either indicates an error or default target (if
  // applicable) or the property node is an object or a scalar value (for
  // example, `0` in `AXChildren[0]` or [3, 4] integer array).
  if (!target) {
    // If default target is given, i.e. |node| is not null, then the target is
    // deemed and we use the default target. This case is about ax tree dumping
    // where a scripting instruction with no target are used. For example,
    // `AXRole` property filter means it is applied to all nodes and `AXRole`
    // attribute should be called for all nodes in the tree.
    if (IsDumpingTree()) {
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
    // Result of the current step is state. Don't go any further.
    if (!target_optional.HasValue())
      return target_optional;

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
  if (IsNSAccessibilityElement(target) || IsAXUIElement(target))
    return InvokeForAXElement(target, property_node);

  if (content::IsAXTextMarkerRange(target)) {
    return InvokeForAXTextMarkerRange(target, property_node);
  }

  if ([target isKindOfClass:[NSArray class]])
    return InvokeForArray(target, property_node);

  if ([target isKindOfClass:[NSDictionary class]])
    return InvokeForDictionary(target, property_node);

  LOG(ERROR) << "Unexpected target type for " << property_node.ToFlatString();
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForAXElement(
    const id target,
    const AXPropertyNode& property_node) const {
  // Actions.
  if (property_node.name_or_value == "AXActionNames") {
    return OptionalNSObject::NotNullOrNotApplicable(AXActionNamesOf(target));
  }
  if (property_node.name_or_value == "AXPerformAction") {
    OptionalNSObject param = ParamByPropertyNode(property_node);
    if (param.IsNotNull()) {
      PerformAXAction(target, *param);
      return OptionalNSObject::Unsupported();
    }
    return OptionalNSObject::Error();
  }

  // Attributes.
  for (NSString* attribute : AXAttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      // Setter
      if (property_node.rvalue) {
        OptionalNSObject rvalue = Invoke(*property_node.rvalue);
        if (rvalue.IsNotNull()) {
          SetAXAttributeValueOf(target, attribute, *rvalue);
          return {rvalue};
        }
        return rvalue;
      }
      // Getter. Make sure to expose null values in ax scripts.
      id value = AXAttributeValueOf(target, attribute);
      return IsDumpingTree() ? OptionalNSObject::NotNullOrNotApplicable(value)
                             : OptionalNSObject(value);
    }
  }

  // Parameterized attributes.
  for (NSString* attribute : AXParameterizedAttributeNamesOf(target)) {
    if (property_node.IsMatching(base::SysNSStringToUTF8(attribute))) {
      OptionalNSObject param = ParamByPropertyNode(property_node);
      if (param.IsNotNull()) {
        return OptionalNSObject(
            AXParameterizedAttributeValueOf(target, attribute, *param));
      }
      return param;
    }
  }

  // Invoke any methods that are declared in the NSAccessibility protocol. Note
  // that they all start with the prefix "accessibility...", ignore all
  // other selectors the object may respond.
  if (base::StartsWith(property_node.name_or_value, "accessibility")) {
    auto optional_id = PerformAXSelector(target, property_node.name_or_value);
    if (optional_id) {
      return OptionalNSObject(*optional_id);
    }
  }

  // Unmatched attribute.
  // * We choose not to return an error when dumping the accessibility tree,
  // because during this process the same set of NSAccessibility attributes
  // listed in property filters are queried on all nodes and, naturally, not all
  // nodes support all attributes.
  // * We also explicitly choose not to return an error if the NSAccessibility
  // attribute is valid and is in the list of attributes that our tree formatter
  // supports, but is not exposed on a given node.
  if (IsDumpingTree() || IsValidAXAttribute(property_node.name_or_value)) {
    return OptionalNSObject::NotApplicable();
  }

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXElement in '"
             << property_node.ToFlatString() << "' statement";
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForAXTextMarkerRange(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "anchor")
    return OptionalNSObject(static_cast<id>(AXTextMarkerRangeCopyStartMarker(
        static_cast<AXTextMarkerRangeRef>(target))));

  if (property_node.name_or_value == "focus")
    return OptionalNSObject(static_cast<id>(AXTextMarkerRangeCopyEndMarker(
        static_cast<AXTextMarkerRangeRef>(target))));

  // Unmatched attribute. We choose not to return an error when dumping the
  // accessibility tree, because during this process the same set of
  // NSAccessibility attributes listed in property filters are queried on all
  // nodes and, naturally, not all nodes support all attributes.
  if (IsDumpingTree())
    return OptionalNSObject::Unsupported();

  LOG(ERROR) << "Unrecognized '" << property_node.name_or_value
             << "' attribute called on AXTextMarkerRange in '"
             << property_node.ToFlatString() << "' statement";
  return OptionalNSObject::Error();
}

OptionalNSObject AttributeInvoker::InvokeForArray(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "count") {
    if (property_node.arguments.size()) {
      LOG(ERROR) << "count attribute is called as a method";
      return OptionalNSObject::Error();
    }
    return OptionalNSObject([NSNumber numberWithInt:[target count]]);
  }
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

OptionalNSObject AttributeInvoker::InvokeForDictionary(
    const id target,
    const AXPropertyNode& property_node) const {
  if (property_node.arguments.size() > 0) {
    LOG(ERROR) << "dictionary key is expected, got: "
               << property_node.ToString();
    return OptionalNSObject::Error();
  }

  NSString* key = PropertyNodeToString(property_node);
  NSDictionary* dictionary = target;
  return OptionalNSObject::NotNullOrError(dictionary[key]);
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
    return OptionalNSObject::NotNullOrError(PropertyNodeToInt(arg_node));
  }
  if (property_name == "AXPerformAction") {
    return OptionalNSObject::NotNullOrError(PropertyNodeToString(arg_node));
  }
  if (property_name == "AXCellForColumnAndRow") {  // IntArray
    return OptionalNSObject::NotNullOrError(PropertyNodeToIntArray(arg_node));
  }
  if (property_name ==
      "AXTextMarkerRangeForUnorderedTextMarkers") {  // TextMarkerArray
    return OptionalNSObject::NotNullOrError(
        PropertyNodeToTextMarkerArray(arg_node));
  }
  if (property_name == "AXStringForRange") {  // NSRange
    return OptionalNSObject::NotNullOrError(PropertyNodeToRange(arg_node));
  }
  if (property_name == "AXIndexForChildUIElement" ||
      property_name == "AXTextMarkerRangeForUIElement") {  // UIElement
    return OptionalNSObject::NotNullOrError(PropertyNodeToUIElement(arg_node));
  }
  if (property_name == "AXIndexForTextMarker" ||
      property_name == "AXNextWordEndTextMarkerForTextMarker" ||
      property_name ==
          "AXPreviousWordStartTextMarkerForTextMarker") {  // TextMarker
    return OptionalNSObject::NotNullOrError(PropertyNodeToTextMarker(arg_node));
  }
  if (property_name == "AXSelectedTextMarkerRangeAttribute" ||
      property_name == "AXStringForTextMarkerRange") {  // TextMarkerRange
    return OptionalNSObject::NotNullOrError(
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
    if (!text_marker.IsNotNull()) {
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
      indexer_->NodeBy(uielement_node.name_or_value);
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
      indexer_->NodeBy(dictnode.arguments[0].name_or_value);
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

}  // namespace a11y
}  // namespace content
