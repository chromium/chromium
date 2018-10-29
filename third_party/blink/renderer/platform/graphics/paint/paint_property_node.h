// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#endif

#include <iosfwd>

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

// Returns the lowest common ancestor in the paint property tree.
template <typename NodeType>
const NodeType& LowestCommonAncestor(const NodeType& a, const NodeType& b) {
  // Fast path of common cases.
  if (&a == &b || !a.Parent() || b.Parent() == &a) {
    DCHECK(a.IsAncestorOf(b));
    return a;
  }
  if (!b.Parent() || a.Parent() == &b) {
    DCHECK(b.IsAncestorOf(a));
    return b;
  }

  return LowestCommonAncestorInternal(a, b);
}

PLATFORM_EXPORT const ClipPaintPropertyNode& LowestCommonAncestorInternal(
    const ClipPaintPropertyNode&,
    const ClipPaintPropertyNode&);
PLATFORM_EXPORT const EffectPaintPropertyNode& LowestCommonAncestorInternal(
    const EffectPaintPropertyNode&,
    const EffectPaintPropertyNode&);
PLATFORM_EXPORT const ScrollPaintPropertyNode& LowestCommonAncestorInternal(
    const ScrollPaintPropertyNode&,
    const ScrollPaintPropertyNode&);
PLATFORM_EXPORT const TransformPaintPropertyNode& LowestCommonAncestorInternal(
    const TransformPaintPropertyNode&,
    const TransformPaintPropertyNode&);

template <typename NodeType>
const NodeType* SafeUnalias(const NodeType* node) {
  return node ? node->Unalias() : nullptr;
}

template <typename NodeType>
class PaintPropertyNode : public RefCounted<NodeType> {
 public:
  // Parent property node, or nullptr if this is the root node.
  const NodeType* Parent() const { return parent_.get(); }
  bool IsRoot() const { return !parent_; }

  bool IsAncestorOf(const NodeType& other) const {
    for (const NodeType* node = &other; node != this; node = node->Parent()) {
      if (!node)
        return false;
    }
    return true;
  }

  void ClearChangedToRoot() const {
    for (auto* n = this; n; n = n->Parent())
      n->changed_ = false;
  }

  // Returns true if this node is an alias for its parent. A parent alias is a
  // node which on its own does not contribute to the rendering output, and only
  // exists to enforce a particular structure of the paint property tree. Its
  // value is ignored during display item list generation, instead the parent
  // value is used. See Unalias().
  bool IsParentAlias() const { return is_parent_alias_; }
  // Returns the first node up the parent chain that is not an alias; return the
  // root node if every node is an alias.
  const NodeType* Unalias() const {
    const auto* node = static_cast<const NodeType*>(this);
    while (node->Parent() && node->IsParentAlias())
      node = node->Parent();
    return node;
  }

  String ToString() const {
    auto s = static_cast<const NodeType*>(this)->ToJSON()->ToJSONString();
#if DCHECK_IS_ON()
    return debug_name_ + String::Format(" %p ", this) + s;
#else
    return s;
#endif
  }

#if DCHECK_IS_ON()
  String ToTreeString() const;

  String DebugName() const { return debug_name_; }
  void SetDebugName(const String& name) { debug_name_ = name; }
#endif

 protected:
  PaintPropertyNode(const NodeType* parent, bool is_parent_alias = false)
      : parent_(parent),
        is_parent_alias_(is_parent_alias),
        changed_(!!parent) {}

  bool SetParent(const NodeType* parent) {
    DCHECK(!IsRoot());
    DCHECK(parent != this);
    if (parent == parent_)
      return false;

    parent_ = parent;
    static_cast<NodeType*>(this)->SetChanged();
    return true;
  }

  void SetChanged() {
    DCHECK(!IsRoot());
    changed_ = true;
  }
  bool NodeChanged() const { return changed_; }

 private:
  friend class PaintPropertyNodeTest;
  // Object paint properties can set the parent directly for an alias update.
  friend class ObjectPaintProperties;

  scoped_refptr<const NodeType> parent_;
  // Indicates whether this node is an alias for its parent. Parent aliases are
  // nodes that do not affect rendering and are ignored for the purposes of
  // display item list generation.
  bool is_parent_alias_ = false;
  mutable bool changed_ = true;

#if DCHECK_IS_ON()
  String debug_name_;
#endif
};

#if DCHECK_IS_ON()

template <typename NodeType>
class PropertyTreePrinter {
 public:
  void AddNode(const NodeType* node) {
    if (node)
      nodes_.insert(node);
  }

  String NodesAsTreeString() {
    if (nodes_.IsEmpty())
      return "";
    StringBuilder string_builder;
    BuildTreeString(string_builder, RootNode(), 0);
    return string_builder.ToString();
  }

  String PathAsString(const NodeType* last_node) {
    for (const auto* n = last_node; n; n = n->Parent())
      AddNode(n);
    return NodesAsTreeString();
  }

 private:
  void BuildTreeString(StringBuilder& string_builder,
                       const NodeType* node,
                       unsigned indent) {
    DCHECK(node);
    for (unsigned i = 0; i < indent; i++)
      string_builder.Append(' ');
    string_builder.Append(node->ToString());
    string_builder.Append("\n");

    for (const auto* child_node : nodes_) {
      if (child_node->Parent() == node)
        BuildTreeString(string_builder, child_node, indent + 2);
    }
  }

  const NodeType* RootNode() {
    const auto* node = nodes_.back();
    while (!node->IsRoot())
      node = node->Parent();
    if (node->DebugName().IsEmpty())
      const_cast<NodeType*>(node)->SetDebugName("root");
    nodes_.insert(node);
    return node;
  }

  ListHashSet<const NodeType*> nodes_;
};

template <typename NodeType>
String PaintPropertyNode<NodeType>::ToTreeString() const {
  return PropertyTreePrinter<NodeType>().PathAsString(
      static_cast<const NodeType*>(this));
}

#endif  // DCHECK_IS_ON()

template <typename NodeType>
std::ostream& operator<<(std::ostream& os,
                         const PaintPropertyNode<NodeType>& node) {
  return os << static_cast<const NodeType&>(node).ToString().Utf8().data();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
