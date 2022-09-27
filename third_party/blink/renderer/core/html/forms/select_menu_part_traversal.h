// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_MENU_PART_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_MENU_PART_TRAVERSAL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class HTMLSelectMenuElement;
class Node;

// Used by <selectmenu> to find child parts. The <selectmenu> parts search
// pierces into shadow roots, but ignores all children of nested <selectmenu>
// and <select> elements. So this traversal is similar to a FlatTreeTraversal,
// except that when a <selectmenu> or <select> element is encountered, that
// element and its children are skipped over.
class CORE_EXPORT SelectMenuPartTraversal {
  STATIC_ONLY(SelectMenuPartTraversal);

 public:
  // Returns the first non-<select> or <selectmenu> child of node in a flat tree
  // traversal.
  static Node* FirstChild(const Node& node);
  // Returns the last non-<select> or <selectmenu> child of node in a flat tree
  // traversal.
  static Node* LastChild(const Node& node);
  // Returns the next non-<select> or <selectmenu> sibling of node in a flat
  // tree traversal.
  static Node* NextSibling(const Node& node);
  // Returns the previous non-<select> or <selectmenu> sibling of node in a flat
  // tree traversal.
  static Node* PreviousSibling(const Node& node);
  // Returns the next Node in a flat tree pre-order traversal that skips
  // <select> and <selectemenu> elements and their children.
  static Node* Next(const Node& node, const Node* stay_within);
  // Returns the previous Node in a flat tree pre-order traversal that skips
  // <select> and <selectemenu> elements and their children.
  static Node* Previous(const Node& node, const Node* stay_within);

  // Returns true if other is an ancestor of node, and there are no <select> or
  // <selectmenu> ancestors in the parent chain between node and other.
  static bool IsDescendantOf(const Node& node, const Node& other);

  // Returns the nearest ancestor of node that is a <selectmenu>, stopping when
  // a <select> is encountered before a <selectmenu> in the ancestor chain.
  static HTMLSelectMenuElement* NearestSelectMenuAncestor(const Node& node);

 private:
  static Node* NextSkippingChildren(const Node&, const Node* stay_within);
  static bool IsNestedSelectMenu(const Node& node);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_MENU_PART_TRAVERSAL_H_
