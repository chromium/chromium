// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_

#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/common/content_export.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_optional.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer_mac.h"

namespace ui {
class AXPropertyNode;

}  // namespace ui

namespace content {
namespace a11y {

// Optional tri-state id object.
using OptionalNSObject = ui::AXOptional<id>;

// Invokes attributes matching the given property filter.
class CONTENT_EXPORT AttributeInvoker final {
 public:
  // Generic version, all calls are executed in context of property nodes.
  // Note: both |indexer| and |storage| must outlive this object.
  AttributeInvoker(const ui::AXTreeIndexerMac* indexer,
                   std::map<std::string, id>* storage);

  // Single target version, all calls are executed in the context of the given
  // target node.
  // Note: |indexer| must outlive this object.
  AttributeInvoker(const id node, const ui::AXTreeIndexerMac* indexer);

  // Invokes an attribute matching to a property filter.
  OptionalNSObject Invoke(const ui::AXPropertyNode& property_node,
                          bool no_object_parse = false) const;

 private:
  // Returns true if the invoker is instantiated to invoke an ax_script
  // instruction, as opposite to processing ax_dump_tree filters.
  bool IsDumpingTree() const { return !!node; }

  // Invokes a property node for a given target.
  OptionalNSObject InvokeFor(const id target,
                             const ui::AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXElement.
  OptionalNSObject InvokeForAXElement(
      const id target,
      const ui::AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXTextMarkerRange.
  OptionalNSObject InvokeForAXTextMarkerRange(
      const id target,
      const ui::AXPropertyNode& property_node) const;

  // Invokes a property node for a given array.
  OptionalNSObject InvokeForArray(
      const id target,
      const ui::AXPropertyNode& property_node) const;

  // Invokes a property node for a given dictionary.
  OptionalNSObject InvokeForDictionary(
      const id target,
      const ui::AXPropertyNode& property_node) const;

  // Returns a parameterized attribute parameter by a property node.
  OptionalNSObject ParamByPropertyNode(const ui::AXPropertyNode&) const;

  // Converts a given property node to NSObject. If not convertible, returns
  // nil.
  id PropertyNodeToNSObject(const ui::AXPropertyNode& property_node) const;

  NSNumber* PropertyNodeToInt(const ui::AXPropertyNode&,
                              bool log_failure = true) const;
  NSString* PropertyNodeToString(const ui::AXPropertyNode&,
                                 bool log_failure = true) const;
  NSArray* PropertyNodeToIntArray(const ui::AXPropertyNode&,
                                  bool log_failure = true) const;
  NSArray* PropertyNodeToTextMarkerArray(const ui::AXPropertyNode&,
                                         bool log_failure = true) const;
  NSValue* PropertyNodeToRange(const ui::AXPropertyNode&,
                               bool log_failure = true) const;
  gfx::NativeViewAccessible PropertyNodeToUIElement(
      const ui::AXPropertyNode&,
      bool log_failure = true) const;

  id DictNodeToTextMarker(const ui::AXPropertyNode&,
                          bool log_failure = true) const;
  id PropertyNodeToTextMarker(const ui::AXPropertyNode&,
                              bool log_failure = true) const;
  id PropertyNodeToTextMarkerRange(const ui::AXPropertyNode&,
                                   bool log_failure = true) const;

  gfx::NativeViewAccessible LineIndexToNode(
      const std::u16string line_index) const;

  const id node;

  // Map between AXUIElement objects and their DOMIds/accessible tree
  // line numbers. Owned by the caller and outlives this object.
  const ui::AXTreeIndexerMac* indexer_;

  // Variables storage. Owned by the caller and outlives this object.
  std::map<std::string, id>* storage_;
};

}  // namespace a11y
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
