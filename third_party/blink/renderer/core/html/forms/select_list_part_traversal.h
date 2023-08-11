// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_LIST_PART_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_LIST_PART_TRAVERSAL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLSelectListElement;
class Node;

// Used by <selectlist> to find child parts. The <selectlist> parts search
// pierces into shadow roots, but ignores all children of nested <selectlist>
// and <select> elements. So this traversal is similar to a FlatTreeTraversal,
// except that when a <selectlist> or <select> element is encountered, that
// element and its children are skipped over.
class CORE_EXPORT SelectListPartTraversal {
  STATIC_ONLY(SelectListPartTraversal);

 public:
  // Returns the first non-<select> or <selectlist> child of node in a flat tree
  // traversal.
  static Node* FirstChild(const Node& node);
  // Returns the last non-<select> or <selectlist> child of node in a flat tree
  // traversal.
  static Node* LastChild(const Node& node);
  // Returns the next non-<select> or <selectlist> sibling of node in a flat
  // tree traversal.
  static Node* NextSibling(const Node& node);
  // Returns the previous non-<select> or <selectlist> sibling of node in a flat
  // tree traversal.
  static Node* PreviousSibling(const Node& node);
  // Returns the next Node in a flat tree pre-order traversal that skips
  // <select> and <selectemenu> elements and their children.
  static Node* Next(const Node& node, const Node* stay_within);
  // Returns the previous Node in a flat tree pre-order traversal that skips
  // <select> and <selectemenu> elements and their children.
  static Node* Previous(const Node& node, const Node* stay_within);

  // Returns true if other is an ancestor of node, and there are no <select> or
  // <selectlist> ancestors in the parent chain between node and other.
  static bool IsDescendantOf(const Node& node, const Node& other);

  // Returns the nearest ancestor of node that is a <selectlist>, stopping when
  // a <select> is encountered before a <selectlist> in the ancestor chain.
  static HTMLSelectListElement* NearestSelectListAncestor(const Node& node);

 private:
  static Node* NextSkippingChildren(const Node&, const Node* stay_within);
  static bool IsNestedSelectList(const Node& node);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_LIST_PART_TRAVERSAL_H_
