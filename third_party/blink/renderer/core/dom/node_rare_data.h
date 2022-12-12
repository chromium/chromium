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
class NodeRenderingData;
class NodeRareData;
class ScrollTimeline;

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
    kNumberOfDynamicRestyleFlags = 14
  };

  enum class ClassType : uint8_t {
    kNodeRareData,
    kElementRareData,
    kNodeRenderingData,
    kLastType = kNodeRenderingData
  };

  virtual ~NodeData() = default;
  virtual void Trace(Visitor*) const;

 protected:
  using BitField = WTF::ConcurrentlyReadBitField<uint16_t>;
  using RestyleFlags =
      BitField::DefineFirstValue<uint16_t, kNumberOfDynamicRestyleFlags>;
  static constexpr size_t kClassTypeBits = 2;
  static_assert(static_cast<size_t>(ClassType::kLastType) <
                    ((size_t{1} << kClassTypeBits)),
                "Too many subtypes to fit into bitfield.");
  using ClassTypeData =
      RestyleFlags::DefineNextValue<uint8_t,
                                    kClassTypeBits,
                                    WTF::BitFieldValueConstness::kConst>;

  explicit NodeData(ClassType sub_type)
      : connected_frame_count_(0),
        element_flags_(0),
        is_pseudo_element_(false),
        bit_field_(RestyleFlags::encode(0) |
                   ClassTypeData::encode(static_cast<uint8_t>(sub_type))) {}

  ClassType GetClassType() const {
    return static_cast<ClassType>(bit_field_.get_concurrently<ClassTypeData>());
  }

  uint16_t connected_frame_count_ : kConnectedFrameCountBits;
  uint16_t element_flags_ : kNumberOfElementFlags;
  bool is_pseudo_element_ : 1;
  BitField bit_field_;

  friend struct DowncastTraits<NodeRareData>;
  friend struct DowncastTraits<NodeRenderingData>;
  friend struct DowncastTraits<ElementRareData>;
};

template <>
struct DowncastTraits<NodeRenderingData> {
  static bool AllowFrom(const NodeData& node_data) {
    return node_data.GetClassType() == NodeData::ClassType::kNodeRenderingData;
  }
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

class CORE_EXPORT NodeRenderingData final : public NodeData {
 public:
  NodeRenderingData(LayoutObject*,
                    scoped_refptr<const ComputedStyle> computed_style);
  NodeRenderingData(const NodeRenderingData&) = delete;
  NodeRenderingData& operator=(const NodeRenderingData&) = delete;

  LayoutObject* GetLayoutObject() const { return layout_object_; }
  void SetLayoutObject(LayoutObject* layout_object) {
    DCHECK_NE(&SharedEmptyData(), this);
    layout_object_ = layout_object;
  }

  const ComputedStyle* GetComputedStyle() const {
    return computed_style_.get();
  }
  void SetComputedStyle(scoped_refptr<const ComputedStyle> computed_style);

  static NodeRenderingData& SharedEmptyData();
  bool IsSharedEmptyData() { return this == &SharedEmptyData(); }

  void Trace(Visitor*) const override;

 private:
  Member<LayoutObject> layout_object_;
  scoped_refptr<const ComputedStyle> computed_style_;
};

class NodeRareData : public NodeData {
 public:
  explicit NodeRareData(NodeRenderingData* node_layout_data)
      : NodeRareData(ClassType::kNodeRareData, node_layout_data) {}
  NodeRareData(const NodeRareData&) = delete;
  NodeRareData& operator=(const NodeRareData&) = delete;

  NodeRenderingData* GetNodeRenderingData() const { return node_layout_data_; }
  void SetNodeRenderingData(NodeRenderingData* node_layout_data) {
    DCHECK(node_layout_data);
    node_layout_data_ = node_layout_data;
  }

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

  FlatTreeNodeData* GetFlatTreeNodeData() const { return flat_tree_node_data_; }
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

  void SetIsPseudoElement(bool value) { is_pseudo_element_ = value; }
  bool IsPseudoElement() const { return is_pseudo_element_; }

  void Trace(blink::Visitor*) const override;

 protected:
  NodeRareData(ClassType class_type, NodeRenderingData* node_layout_data)
      : NodeData(class_type), node_layout_data_(node_layout_data) {
    CHECK_NE(node_layout_data, nullptr);
  }

  Member<NodeRenderingData> node_layout_data_;

 private:
  NodeListsNodeData& CreateNodeLists();

  Member<NodeListsNodeData> node_lists_;
  Member<NodeMutationObserverData> mutation_observer_data_;
  Member<FlatTreeNodeData> flat_tree_node_data_;
  // Keeps strong scroll timeline pointers linked to this node to ensure
  // the timelines are alive as long as the node is alive.
  Member<HeapHashSet<Member<ScrollTimeline>>> scroll_timelines_;
};

template <typename T>
struct ThreadingTrait<
    T,
    std::enable_if_t<std::is_base_of<blink::NodeRareData, T>::value>> {
  static constexpr ThreadAffinity kAffinity = kMainThreadOnly;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
