// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include <iosfwd>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#endif

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

// Used to report whether and how paint properties have changed. The order is
// important - it must go from no change to the most significant change.
enum class PaintPropertyChangeType : unsigned char {
  // Nothing has changed.
  kUnchanged,
  // We only changed values that are either mutated by compositor animations
  // which are updated automatically during the compositor-side animation tick,
  // or have been updated directly on the associated compositor node during the
  // PrePaint lifecycle phase.
  kChangedOnlyCompositedValues,
  // We only changed values that don't require re-raster (e.g. compositor
  // element id changed).
  kChangedOnlyNonRerasterValues,
  // We only changed values and not the hierarchy of the tree, and we know that
  // the value changes are 'simple' in that they don't cause cascading changes.
  // For example, they do not cause a new render surface to be created, which
  // may otherwise cause tree changes elsewhere. An example of this is opacity
  // changing in the [0, 1) range.
  kChangedOnlySimpleValues,
  // We only changed values and not the hierarchy of the tree, but nothing is
  // known about the kind of value change.
  kChangedOnlyValues,
  // We have directly modified the tree topology by adding or removing a node.
  kNodeAddedOrRemoved,
};

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
  return node ? &node->Unalias() : nullptr;
}

template <typename NodeType>
class PaintPropertyNode : public RefCounted<NodeType> {
  USING_FAST_MALLOC(PaintPropertyNode);

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

  void ClearChangedToRoot() const { ClearChangedTo(nullptr); }
  void ClearChangedTo(const NodeType* node) const {
    for (auto* n = this; n && n != node; n = n->Parent())
      n->changed_ = PaintPropertyChangeType::kUnchanged;
  }

  // Returns true if this node is an alias for its parent. A parent alias is a
  // node which on its own does not contribute to the rendering output, and only
  // exists to enforce a particular structure of the paint property tree. Its
  // value is ignored during display item list generation, instead the parent
  // value is used. See Unalias().
  bool IsParentAlias() const { return is_parent_alias_; }
  // Returns the first node up the parent chain that is not an alias; return the
  // root node if every node is an alias.
  const NodeType& Unalias() const {
    const auto* node = static_cast<const NodeType*>(this);
    while (node->Parent() && node->IsParentAlias())
      node = node->Parent();
    return *node;
  }

  void CompositorSimpleValuesUpdated() const {
    if (changed_ == PaintPropertyChangeType::kChangedOnlySimpleValues)
      changed_ = PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }

  String ToString() const {
    auto s = static_cast<const NodeType*>(this)->ToJSON()->ToJSONString();
#if DCHECK_IS_ON()
    return debug_name_ + String::Format(" %p ", this) + s;
#else
    return s;
#endif
  }

  int CcNodeId(int sequence_number) const {
    return cc_sequence_number_ == sequence_number ? cc_node_id_ : -1;
  }
  void SetCcNodeId(int sequence_number, int id) const {
    cc_sequence_number_ = sequence_number;
    cc_node_id_ = id;
  }

  PaintPropertyChangeType NodeChanged() const { return changed_; }
  bool NodeChangeAffectsRaster() const {
    return changed_ != PaintPropertyChangeType::kUnchanged &&
           changed_ != PaintPropertyChangeType::kChangedOnlyNonRerasterValues;
  }

#if DCHECK_IS_ON()
  String ToTreeString() const;

  String DebugName() const { return debug_name_; }
  void SetDebugName(const String& name) { debug_name_ = name; }
#endif

 protected:
  PaintPropertyNode(const NodeType* parent, bool is_parent_alias = false)
      : is_parent_alias_(is_parent_alias),
        changed_(parent ? PaintPropertyChangeType::kNodeAddedOrRemoved
                        : PaintPropertyChangeType::kUnchanged),
        parent_(parent) {}

  PaintPropertyChangeType SetParent(const NodeType* parent) {
    DCHECK(!IsRoot());
    DCHECK(parent != this);
    if (parent == parent_)
      return PaintPropertyChangeType::kUnchanged;

    parent_ = parent;
    static_cast<NodeType*>(this)->AddChanged(
        PaintPropertyChangeType::kChangedOnlyValues);
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  void AddChanged(PaintPropertyChangeType changed) {
    DCHECK(!IsRoot());
    changed_ = std::max(changed_, changed);
  }

 private:
  friend class PaintPropertyNodeTest;
  // Object paint properties can set the parent directly for an alias update.
  friend class ObjectPaintProperties;

  // Indicates whether this node is an alias for its parent. Parent aliases are
  // nodes that do not affect rendering and are ignored for the purposes of
  // display item list generation.
  bool is_parent_alias_;

  // Indicates that the paint property value changed in the last update in the
  // prepaint lifecycle step. This is used for raster invalidation and damage
  // in the compositor. This value is cleared through |ClearChangedTo*|. Before
  // CompositeAfterPaint, this is cleared explicitly at the end of paint (see:
  // LocalFrameView::RunPaintLifecyclePhase), otherwise this is cleared through
  // PaintController::FinishCycle.
  mutable PaintPropertyChangeType changed_;

  scoped_refptr<const NodeType> parent_;

  // Caches the id of the associated cc property node. It's valid only when
  // cc_sequence_number_ matches the sequence number of the cc property tree.
  mutable int cc_node_id_ = -1;
  mutable int cc_sequence_number_ = 0;

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

  String PathAsString(const NodeType& last_node) {
    for (const auto* n = &last_node; n; n = n->Parent())
      AddNode(n);
    return NodesAsTreeString();
  }

 private:
  void BuildTreeString(StringBuilder& string_builder,
                       const NodeType& node,
                       unsigned indent) {
    for (unsigned i = 0; i < indent; i++)
      string_builder.Append(' ');
    string_builder.Append(node.ToString());
    string_builder.Append("\n");

    for (const auto* child_node : nodes_) {
      if (child_node->Parent() == &node)
        BuildTreeString(string_builder, *child_node, indent + 2);
    }
  }

  const NodeType& RootNode() {
    const auto* node = nodes_.back();
    while (!node->IsRoot())
      node = node->Parent();
    if (node->DebugName().IsEmpty())
      const_cast<NodeType*>(node)->SetDebugName("root");
    nodes_.insert(node);
    return *node;
  }

  LinkedHashSet<const NodeType*> nodes_;
};

template <typename NodeType>
String PaintPropertyNode<NodeType>::ToTreeString() const {
  return PropertyTreePrinter<NodeType>().PathAsString(
      *static_cast<const NodeType*>(this));
}

#endif  // DCHECK_IS_ON()

template <typename NodeType>
std::ostream& operator<<(std::ostream& os,
                         const PaintPropertyNode<NodeType>& node) {
  return os << static_cast<const NodeType&>(node).ToString().Utf8();
}

PLATFORM_EXPORT const char* PaintPropertyChangeTypeToString(
    PaintPropertyChangeType);

inline std::ostream& operator<<(std::ostream& os,
                                PaintPropertyChangeType change) {
  return os << PaintPropertyChangeTypeToString(change);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
