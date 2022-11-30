// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_

#include <algorithm>
#include <iosfwd>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "cc/trees/property_tree.h"
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

class ClipPaintPropertyNodeOrAlias;
class EffectPaintPropertyNodeOrAlias;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNodeOrAlias;

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
  // changing in the [0, 1) range. PaintPropertyTreeBuilder may try to directly
  // update the associated compositor node through PaintArtifactCompositor::
  // DirectlyUpdate*(), and if that's successful, the change will be downgraded
  // to kChangeOnlyCompositedValues.
  kChangedOnlySimpleValues,
  // We only changed values and not the hierarchy of the tree, but nothing is
  // known about the kind of value change. The difference between
  // kChangedOnlySimpleValues and kChangedOnlyValues is only meaningful in
  // PaintPropertyTreeBuilder for eligibility of direct update of compositor
  // node. Otherwise we should never distinguish between them.
  kChangedOnlyValues,
  // We have directly modified the tree topology by adding or removing a node.
  kNodeAddedOrRemoved,
};

PLATFORM_EXPORT const char* PaintPropertyChangeTypeToString(
    PaintPropertyChangeType);

PLATFORM_EXPORT const ClipPaintPropertyNodeOrAlias&
LowestCommonAncestorInternal(const ClipPaintPropertyNodeOrAlias&,
                             const ClipPaintPropertyNodeOrAlias&);
PLATFORM_EXPORT const EffectPaintPropertyNodeOrAlias&
LowestCommonAncestorInternal(const EffectPaintPropertyNodeOrAlias&,
                             const EffectPaintPropertyNodeOrAlias&);
PLATFORM_EXPORT const ScrollPaintPropertyNode& LowestCommonAncestorInternal(
    const ScrollPaintPropertyNode&,
    const ScrollPaintPropertyNode&);
PLATFORM_EXPORT const TransformPaintPropertyNodeOrAlias&
LowestCommonAncestorInternal(const TransformPaintPropertyNodeOrAlias&,
                             const TransformPaintPropertyNodeOrAlias&);

template <typename NodeTypeOrAlias, typename NodeType>
struct PaintPropertyNodeRefCountedTraits {
  static void Destruct(const NodeTypeOrAlias* node) {
    if (node->IsParentAlias())
      delete node;
    else
      delete static_cast<const NodeType*>(node);
  }
};

template <typename NodeTypeOrAlias, typename NodeType>
class PaintPropertyNode
    : public RefCounted<
          NodeTypeOrAlias,
          PaintPropertyNodeRefCountedTraits<NodeTypeOrAlias, NodeType>> {
  USING_FAST_MALLOC(PaintPropertyNode);

 public:
  // Parent property node, or nullptr if this is the root node.
  const NodeTypeOrAlias* Parent() const { return parent_.get(); }
  bool IsRoot() const { return !parent_; }

  bool IsAncestorOf(const NodeTypeOrAlias& other) const {
    for (const auto* node = &other; node != this; node = node->Parent()) {
      if (!node)
        return false;
    }
    return true;
  }

  // Clear changed flags along the path to the root, and set sequence number.
  // If a subclass needs to clear changed flags along additional paths, the
  // subclass must override this.
  // For information about |sequence_number|, see: |changed_sequence_number_|.
  void ClearChangedToRoot(int sequence_number) const {
    for (auto* n = this; n && n->changed_sequence_number_ != sequence_number;
         n = n->Parent())
      n->ClearChanged(sequence_number);
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
    const auto* node = static_cast<const NodeTypeOrAlias*>(this);
    while (node->Parent() && node->IsParentAlias())
      node = node->Parent();
    return *static_cast<const NodeType*>(node);
  }

  const NodeType* UnaliasedParent() const {
    return Parent() ? &Parent()->Unalias() : nullptr;
  }

  void CompositorSimpleValuesUpdated() const {
    if (changed_ == PaintPropertyChangeType::kChangedOnlySimpleValues)
      changed_ = PaintPropertyChangeType::kChangedOnlyCompositedValues;
  }

  String ToString() const {
    String s = ToJSON()->ToJSONString();
#if DCHECK_IS_ON()
    return debug_name_ + String::Format(" %p ", this) + s;
#else
    return s;
#endif
  }

  std::unique_ptr<JSONObject> ToJSON() const {
    if (IsParentAlias())
      return ToJSONBase();
    return static_cast<const NodeType*>(this)->ToJSON();
  }

  int CcNodeId(int sequence_number) const {
    return cc_sequence_number_ == sequence_number ? cc_node_id_
                                                  : cc::kInvalidPropertyNodeId;
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

  // Returns the lowest common ancestor in the paint property tree.
  const NodeTypeOrAlias& LowestCommonAncestor(const NodeTypeOrAlias& b) const {
    // Fast path of common cases.
    const auto& a = *static_cast<const NodeTypeOrAlias*>(this);
    if (&a == &b || !a.Parent() || b.Parent() == &a) {
      DCHECK(IsAncestorOf(b));
      return a;
    }
    if (!b.Parent() || a.Parent() == &b) {
      DCHECK(b.IsAncestorOf(a));
      return b;
    }
    return LowestCommonAncestorInternal(a, b);
  }

#if DCHECK_IS_ON()
  String ToTreeString() const;

  String DebugName() const { return debug_name_; }
  void SetDebugName(const String& name) { debug_name_ = name; }
#endif

 protected:
  explicit PaintPropertyNode(const NodeTypeOrAlias* parent)
      : changed_(parent ? PaintPropertyChangeType::kNodeAddedOrRemoved
                        : PaintPropertyChangeType::kUnchanged),
        parent_(parent) {}

  // A parent alias node must have a parent, so ensure that we can always find
  // a unaliased ancestor for any node.
  enum ParentAliasTag { kParentAlias };
  PaintPropertyNode(const NodeTypeOrAlias& parent, ParentAliasTag)
      : is_parent_alias_(true),
        changed_(PaintPropertyChangeType::kNodeAddedOrRemoved),
        parent_(&parent) {}

  PaintPropertyChangeType SetParent(const NodeTypeOrAlias& parent) {
    DCHECK(!IsRoot());
    DCHECK_NE(&parent, this);
    if (&parent == parent_)
      return PaintPropertyChangeType::kUnchanged;

    parent_ = &parent;
    if (IsParentAlias()) {
      static_cast<NodeTypeOrAlias*>(this)->AddChanged(
          PaintPropertyChangeType::kChangedOnlyValues);
    } else {
      static_cast<NodeType*>(this)->AddChanged(
          PaintPropertyChangeType::kChangedOnlyValues);
    }
    return PaintPropertyChangeType::kChangedOnlyValues;
  }

  void AddChanged(PaintPropertyChangeType changed) {
    DCHECK(!IsRoot());
    changed_ = std::max(changed_, changed);
  }

  // For information about |sequence_number|, see: |changed_sequence_number_|.
  void ClearChanged(int sequence_number) const {
    DCHECK_NE(changed_sequence_number_, sequence_number);
    changed_ = PaintPropertyChangeType::kUnchanged;
    changed_sequence_number_ = sequence_number;
  }

  int ChangedSequenceNumber() const { return changed_sequence_number_; }

  std::unique_ptr<JSONObject> ToJSONBase() const {
    auto json = std::make_unique<JSONObject>();
    if (Parent())
      json->SetString("parent", String::Format("%p", Parent()));
    if (IsParentAlias())
      json->SetBoolean("is_alias", true);
    if (NodeChanged() != PaintPropertyChangeType::kUnchanged) {
      json->SetString("changed",
                      PaintPropertyChangeTypeToString(NodeChanged()));
    }
    return json;
  }

 private:
  friend class PaintPropertyNodeTest;

  // Indicates whether this node is an alias for its parent. Parent aliases are
  // nodes that do not affect rendering and are ignored for the purposes of
  // display item list generation.
  bool is_parent_alias_ = false;

  // Indicates that the paint property value changed in the last update in the
  // prepaint lifecycle step. This is used for raster invalidation and damage
  // in the compositor. This value is cleared through ClearChangedToRoot()
  // called by PaintArtifactCompositor::ClearPropertyTreeChangedState().
  mutable PaintPropertyChangeType changed_;
  // The changed sequence number is an optimization to avoid an O(n^2) to O(n^4)
  // treewalk when clearing the changed bits for the entire tree. When starting
  // to clear the changed bits, a new (unique) number is selected for the entire
  // tree, and |changed_sequence_number_| is set to this number if the node (and
  // ancestors) have already been visited for clearing.
  mutable int changed_sequence_number_ = 0;

  // Caches the id of the associated cc property node. It's valid only when
  // cc_sequence_number_ matches the sequence number of the cc property tree.
  mutable int cc_node_id_ = cc::kInvalidPropertyNodeId;
  mutable int cc_sequence_number_ = 0;

  scoped_refptr<const NodeTypeOrAlias> parent_;

#if DCHECK_IS_ON()
  String debug_name_;
#endif
};

#if DCHECK_IS_ON()

template <typename NodeType>
class PropertyTreePrinter {
  STACK_ALLOCATED();

 public:
  void AddNode(const NodeType* node) {
    if (node)
      nodes_.insert(node);
  }

  String NodesAsTreeString() {
    if (nodes_.empty())
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
    if (node->DebugName().empty())
      const_cast<NodeType*>(node)->SetDebugName("root");
    nodes_.insert(node);
    return *node;
  }

  LinkedHashSet<const NodeType*> nodes_;
};

template <typename NodeTypeOrAlias, typename NodeType>
String PaintPropertyNode<NodeTypeOrAlias, NodeType>::ToTreeString() const {
  return PropertyTreePrinter<NodeTypeOrAlias>().PathAsString(
      *static_cast<const NodeTypeOrAlias*>(this));
}

#endif  // DCHECK_IS_ON()

template <typename NodeTypeOrAlias, typename NodeType>
std::ostream& operator<<(
    std::ostream& os,
    const PaintPropertyNode<NodeTypeOrAlias, NodeType>& node) {
  return os << node.ToString().Utf8();
}

inline std::ostream& operator<<(std::ostream& os,
                                PaintPropertyChangeType change) {
  return os << PaintPropertyChangeTypeToString(change);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_PROPERTY_NODE_H_
