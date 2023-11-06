/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/wtf/bit_field.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ComputedStyle;
enum class DynamicRestyleFlags;
enum class ElementFlags;
class ElementRareData;
class FlatTreeNodeData;
class LayoutObject;
class MutationObserverRegistration;
class NodeListsNodeData;
class NodeRareData;
class Part;
class ScrollTimeline;

using PartsList = HeapDeque<Member<Part>>;

class NodeMutationObserverData final
    : public GarbageCollected<NodeMutationObserverData> {
 public:
  NodeMutationObserverData() = default;
  NodeMutationObserverData(const NodeMutationObserverData&) = delete;
  NodeMutationObserverData& operator=(const NodeMutationObserverData&) = delete;

  const HeapVector<Member<MutationObserverRegistration>>& Registry() {
    return registry_;
  }

  const HeapHashSet<Member<MutationObserverRegistration>>& TransientRegistry() {
    return transient_registry_;
  }

  void AddTransientRegistration(MutationObserverRegistration* registration);
  void RemoveTransientRegistration(MutationObserverRegistration* registration);
  void AddRegistration(MutationObserverRegistration* registration);
  void RemoveRegistration(MutationObserverRegistration* registration);

  void Trace(Visitor* visitor) const;

 private:
  HeapVector<Member<MutationObserverRegistration>> registry_;
  HeapHashSet<Member<MutationObserverRegistration>> transient_registry_;
};

class NodeData : public GarbageCollected<NodeData> {
 public:
  enum {
    kConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
    kNumberOfElementFlags = 6,
    kNumberOfDynamicRestyleFlags = 15
  };

  // NOTE: This can only distinguish between NodeRareData and ElementRareData,
  // not a regular NodeData (because we never need to do that).
  enum class ClassType : uint8_t {
    kNodeRareData,
    kElementRareData,
    kLastType = kElementRareData,
  };

  virtual ~NodeData();
  virtual void Trace(Visitor*) const;

  CORE_EXPORT NodeData(LayoutObject*, const ComputedStyle* computed_style);
  NodeData(const NodeData&) = delete;
  NodeData(NodeData&&);

  LayoutObject* GetLayoutObject() const { return layout_object_.Get(); }
  void SetLayoutObject(LayoutObject* layout_object) {
    DCHECK_NE(&SharedEmptyData(), this);
    layout_object_ = layout_object;
  }

  const ComputedStyle* GetComputedStyle() const {
    return computed_style_.Get();
  }
  void SetComputedStyle(const ComputedStyle* computed_style);

  void SetIsPseudoElement(bool value) { is_pseudo_element_ = value; }
  bool IsPseudoElement() const { return is_pseudo_element_; }

  static NodeData& SharedEmptyData();
  bool IsSharedEmptyData() { return this == &SharedEmptyData(); }

 protected:
  using BitField = WTF::ConcurrentlyReadBitField<uint16_t>;
  using RestyleFlags =
      BitField::DefineFirstValue<uint16_t, kNumberOfDynamicRestyleFlags>;
  static constexpr size_t kClassTypeBits = 1;
  static_assert(static_cast<size_t>(ClassType::kLastType) <
                    ((size_t{1} << kClassTypeBits)),
                "Too many subtypes to fit into bitfield.");
  using ClassTypeData =
      RestyleFlags::DefineNextValue<uint8_t,
                                    kClassTypeBits,
                                    WTF::BitFieldValueConstness::kConst>;

  ClassType GetClassType() const {
    return static_cast<ClassType>(bit_field_.get_concurrently<ClassTypeData>());
  }

 protected:
  subtle::UncompressedMember<const ComputedStyle> computed_style_;
  Member<LayoutObject> layout_object_;
  BitField bit_field_;
  bool is_pseudo_element_ = false;
  // 8 free bits here (or 16, if moving is_pseudo_element_ into bit_field_).

  friend struct DowncastTraits<NodeRareData>;
  friend struct DowncastTraits<ElementRareData>;
};

template <>
struct DowncastTraits<NodeRareData> {
  static bool AllowFrom(const NodeData& node_data) {
    return node_data.GetClassType() == NodeData::ClassType::kNodeRareData;
  }
};

template <>
struct DowncastTraits<ElementRareData> {
  static bool AllowFrom(const NodeData& node_data) {
    return node_data.GetClassType() == NodeData::ClassType::kElementRareData;
  }
};

class NodeRareData : public NodeData {
 public:
  explicit NodeRareData(NodeData&& node_layout_data)
      : NodeData(std::move(node_layout_data)),
        connected_frame_count_(0),
        element_flags_(0) {
    bit_field_.set<ClassTypeData>(
        ClassTypeData::encode(static_cast<uint8_t>(ClassType::kNodeRareData)));
  }
  NodeRareData(const NodeRareData&) = delete;
  NodeRareData& operator=(const NodeRareData&) = delete;

  void ClearNodeLists() { node_lists_.Clear(); }
  NodeListsNodeData* NodeLists() const { return node_lists_.Get(); }
  // EnsureNodeLists() and a following NodeListsNodeData functions must be
  // wrapped with a ThreadState::GCForbiddenScope in order to avoid an
  // initialized node_lists_ is cleared by NodeRareData::TraceAfterDispatch().
  NodeListsNodeData& EnsureNodeLists() {
    if (!node_lists_)
      return CreateNodeLists();
    return *node_lists_;
  }

  FlatTreeNodeData* GetFlatTreeNodeData() const {
    return flat_tree_node_data_.Get();
  }
  FlatTreeNodeData& EnsureFlatTreeNodeData();

  NodeMutationObserverData* MutationObserverData() {
    return mutation_observer_data_.Get();
  }
  NodeMutationObserverData& EnsureMutationObserverData() {
    if (!mutation_observer_data_) {
      mutation_observer_data_ =
          MakeGarbageCollected<NodeMutationObserverData>();
    }
    return *mutation_observer_data_;
  }

  uint16_t ConnectedSubframeCount() const { return connected_frame_count_; }
  void IncrementConnectedSubframeCount();
  void DecrementConnectedSubframeCount() {
    DCHECK(connected_frame_count_);
    --connected_frame_count_;
  }

  bool HasElementFlag(ElementFlags mask) const {
    return element_flags_ & static_cast<uint16_t>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) {
    element_flags_ =
        (element_flags_ & ~static_cast<uint16_t>(mask)) |
        (-static_cast<uint16_t>(value) & static_cast<uint16_t>(mask));
  }
  void ClearElementFlag(ElementFlags mask) {
    element_flags_ &= ~static_cast<uint16_t>(mask);
  }

  bool HasRestyleFlag(DynamicRestyleFlags mask) const {
    return bit_field_.get<RestyleFlags>() & static_cast<uint16_t>(mask);
  }
  void SetRestyleFlag(DynamicRestyleFlags mask) {
    bit_field_.set<RestyleFlags>(bit_field_.get<RestyleFlags>() |
                                 static_cast<uint16_t>(mask));
    CHECK(bit_field_.get<RestyleFlags>());
  }
  bool HasRestyleFlags() const { return bit_field_.get<RestyleFlags>(); }
  void ClearRestyleFlags() { bit_field_.set<RestyleFlags>(0); }

  void RegisterScrollTimeline(ScrollTimeline*);
  void UnregisterScrollTimeline(ScrollTimeline*);
  void InvalidateAssociatedAnimationEffects();

  void AddDOMPart(Part& part);
  void RemoveDOMPart(Part& part);
  PartsList* GetDOMParts() const { return dom_parts_.Get(); }

  void Trace(blink::Visitor*) const override;

 protected:
  NodeRareData(ClassType class_type, NodeData&& node_layout_data)
      : NodeData(std::move(node_layout_data)),
        connected_frame_count_(0),
        element_flags_(0) {
    bit_field_.set<ClassTypeData>(
        ClassTypeData::encode(static_cast<uint16_t>(class_type)));
  }

  uint16_t connected_frame_count_ : kConnectedFrameCountBits;
  uint16_t element_flags_ : kNumberOfElementFlags;
  // 16 free bits here.

 private:
  NodeListsNodeData& CreateNodeLists();

  Member<NodeListsNodeData> node_lists_;
  Member<NodeMutationObserverData> mutation_observer_data_;
  Member<FlatTreeNodeData> flat_tree_node_data_;
  // Keeps strong scroll timeline pointers linked to this node to ensure
  // the timelines are alive as long as the node is alive.
  Member<HeapHashSet<Member<ScrollTimeline>>> scroll_timelines_;
  // An ordered set of DOM Parts for this Node, in order of construction. This
  // order is important, since `getParts()` returns a tree-ordered set of parts,
  // with parts on the same `Node` returned in `Part` construction order.
  Member<PartsList> dom_parts_;
};

template <typename T>
struct ThreadingTrait<
    T,
    std::enable_if_t<std::is_base_of<blink::NodeRareData, T>::value>> {
  static constexpr ThreadAffinity kAffinity = kMainThreadOnly;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
