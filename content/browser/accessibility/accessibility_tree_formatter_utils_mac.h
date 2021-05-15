// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_

#include "content/browser/accessibility/accessibility_tools_utils_mac.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer.h"

namespace ui {
class AXPropertyNode;
}

namespace content {
namespace a11y {

// IsBrowserAccessibilityCocoa or IsAXUIElement accessible node comparator.
struct NodeComparator {
  constexpr bool operator()(const gfx::NativeViewAccessible& lhs,
                            const gfx::NativeViewAccessible& rhs) const {
    if (IsAXUIElement(lhs)) {
      DCHECK(IsAXUIElement(rhs));
      return CFHash(lhs) < CFHash(rhs);
    }
    DCHECK(IsBrowserAccessibilityCocoa(lhs));
    DCHECK(IsBrowserAccessibilityCocoa(rhs));
    return lhs < rhs;
  }
};

using LineIndexer =
    ui::AXTreeIndexer<GetDOMId, NSArray*, ChildrenOf, NodeComparator>;

// Implements stateful id values. Can be either id or be in
// error or not applciable state. Similar to absl::optional, but tri-state
// allowing nullable values.
class CONTENT_EXPORT OptionalNSObject final {
 public:
  enum { ID, ERROR, NOT_APPLICABLE };

  static OptionalNSObject Error() { return OptionalNSObject(ERROR); }
  static OptionalNSObject NotApplicable() {
    return OptionalNSObject(NOT_APPLICABLE);
  }
  static OptionalNSObject NotNilOrError(id other_value) {
    return OptionalNSObject(other_value, other_value ? ID : ERROR);
  }
  static OptionalNSObject NotNullOrNotApplicable(id other_value) {
    return OptionalNSObject(other_value, other_value ? ID : NOT_APPLICABLE);
  }

  explicit OptionalNSObject(int flag) : value(nil), flag(flag) {}
  explicit OptionalNSObject(id value, int flag = ID)
      : value(value), flag(flag) {}

  bool IsNotApplicable() const { return flag == NOT_APPLICABLE; }
  bool IsError() const { return flag == ERROR; }
  bool IsNotNil() const { return value != nil; }
  constexpr const id& operator*() const& { return value; }

  std::string ToString() const;

 private:
  id value;
  int flag;
};

// Invokes attributes matching the given property filter.
class CONTENT_EXPORT AttributeInvoker final {
 public:
  AttributeInvoker(const LineIndexer* line_indexer);
  AttributeInvoker(const id node, const LineIndexer* line_indexer);

  // Invokes an attribute matching to a property filter.
  OptionalNSObject Invoke(const ui::AXPropertyNode& property_node) const;
  // Gets the value of a parameterized attribute by name.
  OptionalNSObject GetValue(const std::string& property_name,
                            const OptionalNSObject& param) const;
  // Gets the value of a non-parameterized attribute by name.
  OptionalNSObject GetValue(const std::string& property_name) const;
  // Sets the value of a non-parameterized attribute by name.
  void SetValue(const std::string& property_name,
                const OptionalNSObject& value) const;

 private:
  // Returns an accessible object of the given property node or default one.
  id TargetOf(const ui::AXPropertyNode& property_node) const;

  // Returns a parameterized attribute parameter by a property node.
  OptionalNSObject ParamByPropertyNode(const ui::AXPropertyNode&) const;

  NSNumber* PropertyNodeToInt(const ui::AXPropertyNode&) const;
  NSArray* PropertyNodeToIntArray(const ui::AXPropertyNode&) const;
  NSValue* PropertyNodeToRange(const ui::AXPropertyNode&) const;
  gfx::NativeViewAccessible PropertyNodeToUIElement(
      const ui::AXPropertyNode&) const;

  id DictNodeToTextMarker(const ui::AXPropertyNode&) const;
  id PropertyNodeToTextMarker(const ui::AXPropertyNode&) const;
  id PropertyNodeToTextMarkerRange(const ui::AXPropertyNode&) const;

  gfx::NativeViewAccessible LineIndexToNode(
      const std::u16string line_index) const;

  const id node;
  const LineIndexer* line_indexer;
};

// bindings
CONTENT_EXPORT OptionalNSObject
TextMarkerRangeGetStartMarker(const OptionalNSObject& obj);

CONTENT_EXPORT OptionalNSObject
TextMarkerRangeGetEndMarker(const OptionalNSObject& obj);

CONTENT_EXPORT OptionalNSObject MakePairArray(const OptionalNSObject& obj1,
                                              const OptionalNSObject& obj2);

}  // namespace a11y
}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
