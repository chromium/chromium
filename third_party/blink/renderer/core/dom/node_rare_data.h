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

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/bit_field.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
enum class DynamicRestyleFlags;
enum class ElementFlags;
class FlatTreeNodeData;
class LayoutObject;
class MutationObserverRegistration;
class NodeListsNodeData;
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

class GC_PLUGIN_IGNORE(
    "GC plugin reports that TraceAfterDispatch is not called but it is called "
    "by both NodeRareDate::TraceAfterDispatch and "
    "NodeRenderingData::TraceAfterDispatch.") NodeData
    : public GarbageCollected<NodeData> {
 public:
  NodeData(bool is_rare_data, bool is_element_rare_data)
      : connected_frame_count_(0),
        element_flags_(0),
        bit_field_(RestyleFlags::encode(0) |
                   IsElementRareData::encode(is_element_rare_data) |
                   IsRareData::encode(is_rare_data)) {
    DCHECK(!is_element_rare_data || is_rare_data);
  }
  void Trace(Visitor*) const;
  void TraceAfterDispatch(blink::Visitor*) const {}

  enum {
    kConnectedFrameCountBits = 10,  // Must fit Page::maxNumberOfFrames.
    kNumberOfElementFlags = 6,
    kNumberOfDynamicRestyleFlags = 14
  };

 protected:
  using BitField = WTF::ConcurrentlyReadBitField<uint16_t>;
  using RestyleFlags =
      BitField::DefineFirstValue<uint16_t, kNumberOfDynamicRestyleFlags>;
  using IsElementRareData = RestyleFlags::
      DefineNextValue<bool, 1, WTF::BitFieldValueConstness::kConst>;
  using IsRareData = IsElementRareData::
      DefineNextValue<bool, 1, WTF::BitFieldValueConstness::kConst>;

  uint16_t connected_frame_count_ : kConnectedFrameCountBits;
  uint16_t element_flags_ : kNumberOfElementFlags;
  BitField bit_field_;
};

class GC_PLUGIN_IGNORE("Manual dispatch implemented in NodeData.")
    NodeRenderingData final : public NodeData {
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

  void TraceAfterDispatch(Visitor* visitor) const {
    NodeData::TraceAfterDispatch(visitor);
  }

 private:
  LayoutObject* layout_object_;
  scoped_refptr<const ComputedStyle> computed_style_;
};

class GC_PLUGIN_IGNORE("Manual dispatch implemented in NodeData.") NodeRareData
    : public NodeData {
 public:
  explicit NodeRareData(NodeRenderingData* node_layout_data)
      : NodeRareData(node_layout_data, false) {}
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

  void TraceAfterDispatch(blink::Visitor*) const;
  void FinalizeGarbageCollectedObject();
  void RegisterScrollTimeline(ScrollTimeline*);
  void UnregisterScrollTimeline(ScrollTimeline*);

 protected:
  explicit NodeRareData(NodeRenderingData* node_layout_data,
                        bool is_element_rare_data)
      : NodeData(true, is_element_rare_data),
        node_layout_data_(node_layout_data) {
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_RARE_DATA_H_
