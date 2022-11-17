/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/node_rare_data.h"

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsNodeRareData : NodeData {
  Member<void*> member_[5];
};

ASSERT_SIZE(NodeRareData, SameSizeAsNodeRareData);

void NodeMutationObserverData::Trace(Visitor* visitor) const {
  visitor->Trace(registry_);
  visitor->Trace(transient_registry_);
}

void NodeMutationObserverData::AddTransientRegistration(
    MutationObserverRegistration* registration) {
  transient_registry_.insert(registration);
}

void NodeMutationObserverData::RemoveTransientRegistration(
    MutationObserverRegistration* registration) {
  DCHECK(transient_registry_.Contains(registration));
  transient_registry_.erase(registration);
}

void NodeMutationObserverData::AddRegistration(
    MutationObserverRegistration* registration) {
  registry_.push_back(registration);
}

void NodeMutationObserverData::RemoveRegistration(
    MutationObserverRegistration* registration) {
  DCHECK(registry_.Contains(registration));
  registry_.EraseAt(registry_.Find(registration));
}

void NodeData::Trace(Visitor* visitor) const {}

NodeRenderingData::NodeRenderingData(
    LayoutObject* layout_object,
    scoped_refptr<const ComputedStyle> computed_style)
    : NodeData(ClassType::kNodeRenderingData),
      layout_object_(layout_object),
      computed_style_(computed_style) {}

void NodeRenderingData::SetComputedStyle(
    scoped_refptr<const ComputedStyle> computed_style) {
  DCHECK_NE(&SharedEmptyData(), this);
  computed_style_ = computed_style;
}

NodeRenderingData& NodeRenderingData::SharedEmptyData() {
  DEFINE_STATIC_LOCAL(
      Persistent<NodeRenderingData>, shared_empty_data,
      (MakeGarbageCollected<NodeRenderingData>(nullptr, nullptr)));
  return *shared_empty_data;
}
void NodeRenderingData::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  NodeData::Trace(visitor);
}

void NodeRareData::RegisterScrollTimeline(ScrollTimeline* timeline) {
  if (!scroll_timelines_) {
    scroll_timelines_ =
        MakeGarbageCollected<HeapHashSet<Member<ScrollTimeline>>>();
  }
  scroll_timelines_->insert(timeline);
}
void NodeRareData::UnregisterScrollTimeline(ScrollTimeline* timeline) {
  scroll_timelines_->erase(timeline);
}

void NodeRareData::InvalidateAssociatedAnimationEffects() {
  if (!scroll_timelines_)
    return;

  for (ScrollTimeline* scroll_timeline : *scroll_timelines_) {
    scroll_timeline->InvalidateEffectTargetStyle();
  }
}

void NodeRareData::Trace(blink::Visitor* visitor) const {
  visitor->Trace(mutation_observer_data_);
  visitor->Trace(flat_tree_node_data_);
  visitor->Trace(node_layout_data_);
  visitor->Trace(node_lists_);
  visitor->Trace(scroll_timelines_);
  NodeData::Trace(visitor);
}

void NodeRareData::IncrementConnectedSubframeCount() {
  SECURITY_CHECK((connected_frame_count_ + 1) <= Page::MaxNumberOfFrames());
  ++connected_frame_count_;
}

NodeListsNodeData& NodeRareData::CreateNodeLists() {
  node_lists_ = MakeGarbageCollected<NodeListsNodeData>();
  return *node_lists_;
}

FlatTreeNodeData& NodeRareData::EnsureFlatTreeNodeData() {
  if (!flat_tree_node_data_)
    flat_tree_node_data_ = MakeGarbageCollected<FlatTreeNodeData>();
  return *flat_tree_node_data_;
}

static_assert(static_cast<int>(NodeRareData::kNumberOfElementFlags) ==
                  static_cast<int>(ElementFlags::kNumberOfElementFlags),
              "kNumberOfElementFlags must match.");
static_assert(
    static_cast<int>(NodeRareData::kNumberOfDynamicRestyleFlags) ==
        static_cast<int>(DynamicRestyleFlags::kNumberOfDynamicRestyleFlags),
    "kNumberOfDynamicRestyleFlags must match.");

}  // namespace blink
