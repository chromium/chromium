// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_UTILS_MAC_H_

#include "content/browser/accessibility/accessibility_tree_formatter_base.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"

namespace content {
namespace a11y {

/**
 * Converts accessible node object to a line index in the formatted
 * accessibility tree, the node is placed at, and vice versa.
 */
class LineIndexer final {
 public:
  LineIndexer(const gfx::NativeViewAccessible node);
  virtual ~LineIndexer();

  std::string IndexBy(const gfx::NativeViewAccessible node) const;
  gfx::NativeViewAccessible NodeBy(const std::string& index) const;

 private:
  void Build(const gfx::NativeViewAccessible node, int* counter);

  std::map<const gfx::NativeViewAccessible, std::string> map;
};

// Implements stateful id values. Can be either id or be in
// error or not applciable state. Similar to base::Optional, but tri-state
// allowing nullable values.
class OptionalNSObject final {
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

  OptionalNSObject(int flag) : value(nil), flag(flag) {}
  OptionalNSObject(id value, int flag = ID) : value(value), flag(flag) {}

  bool IsNotApplicable() const { return flag == NOT_APPLICABLE; }
  bool IsError() const { return flag == ERROR; }
  bool IsNotNil() const { return value != nil; }
  constexpr const id& operator*() const& { return value; }

 private:
  id value;
  int flag;
};

// Invokes attributes matching the given property filter.
class AttributeInvoker final {
 public:
  AttributeInvoker(const id node, const LineIndexer* line_indexer);

  // Invokes an attribute matching to a property filter.
  OptionalNSObject Invoke(const PropertyNode& property_node) const;

 private:
  // Returns a parameterized attribute parameter by a property node.
  OptionalNSObject ParamByPropertyNode(const PropertyNode&) const;

  NSNumber* PropertyNodeToInt(const PropertyNode&) const;
  NSArray* PropertyNodeToIntArray(const PropertyNode&) const;
  NSValue* PropertyNodeToRange(const PropertyNode&) const;
  gfx::NativeViewAccessible PropertyNodeToUIElement(const PropertyNode&) const;

  id DictNodeToTextMarker(const PropertyNode&) const;
  id PropertyNodeToTextMarker(const PropertyNode&) const;
  id PropertyNodeToTextMarkerRange(const PropertyNode&) const;

  gfx::NativeViewAccessible LineIndexToNode(
      const base::string16 line_index) const;

  const id node;
  const LineIndexer* line_indexer;
  const NSArray* attributes;
  const NSArray* parameterized_attributes;
};

}  // namespace a11y
}  // namespace content

#endif
