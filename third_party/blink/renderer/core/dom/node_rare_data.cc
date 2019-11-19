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

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

struct SameSizeAsNodeRareData {
  void* pointer_;
  Member<void*> willbe_member_[3];
  unsigned bitfields_;
};

static_assert(sizeof(NodeRareData) == sizeof(SameSizeAsNodeRareData),
              "NodeRareData should stay small");

void NodeMutationObserverData::Trace(Visitor* visitor) {
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

void NodeRareData::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(mutation_observer_data_);
  visitor->Trace(flat_tree_node_data_);
  // Do not keep empty NodeListsNodeData objects around.
  if (node_lists_ && node_lists_->IsEmpty())
    node_lists_.Clear();
  else
    visitor->Trace(node_lists_);
}

void NodeRareData::Trace(Visitor* visitor) {
  if (is_element_rare_data_)
    static_cast<ElementRareData*>(this)->TraceAfterDispatch(visitor);
  else
    TraceAfterDispatch(visitor);
}

void NodeRareData::FinalizeGarbageCollectedObject() {
  if (is_element_rare_data_)
    static_cast<ElementRareData*>(this)->~ElementRareData();
  else
    this->~NodeRareData();
}

void NodeRareData::IncrementConnectedSubframeCount() {
  SECURITY_CHECK((connected_frame_count_ + 1) <= Page::kMaxNumberOfFrames);
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

// Ensure the 10 bits reserved for the connected_frame_count_ cannot overflow.
static_assert(Page::kMaxNumberOfFrames <
                  (1 << NodeRareData::kConnectedFrameCountBits),
              "Frame limit should fit in rare data count");
static_assert(static_cast<int>(NodeRareData::kNumberOfElementFlags) ==
                  static_cast<int>(ElementFlags::kNumberOfElementFlags),
              "kNumberOfElementFlags must match.");
static_assert(
    static_cast<int>(NodeRareData::kNumberOfDynamicRestyleFlags) ==
        static_cast<int>(DynamicRestyleFlags::kNumberOfDynamicRestyleFlags),
    "kNumberOfDynamicRestyleFlags must match.");

}  // namespace blink
