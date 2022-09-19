/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"

#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// The accumulator map is used to make sure that there is only one mutation
// accumulator for a given node even if there are multiple
// ChildListMutationScopes on the stack. The map is always empty when there are
// no ChildListMutationScopes on the stack.
typedef HeapHashMap<Member<Node>, Member<ChildListMutationAccumulator>>
    AccumulatorMap;

static AccumulatorMap& GetAccumulatorMap() {
  DEFINE_STATIC_LOCAL(Persistent<AccumulatorMap>, map,
                      (MakeGarbageCollected<AccumulatorMap>()));
  return *map;
}

ChildListMutationAccumulator::ChildListMutationAccumulator(
    Node* target,
    MutationObserverInterestGroup* observers)
    : target_(target),
      last_added_(nullptr),
      observers_(observers),
      mutation_scopes_(0) {}

void ChildListMutationAccumulator::LeaveMutationScope() {
  DCHECK_GT(mutation_scopes_, 0u);
  if (!--mutation_scopes_) {
    if (!IsEmpty())
      EnqueueMutationRecord();
    GetAccumulatorMap().erase(target_.Get());
  }
}

ChildListMutationAccumulator* ChildListMutationAccumulator::GetOrCreate(
    Node& target) {
  AccumulatorMap::AddResult result =
      GetAccumulatorMap().insert(&target, nullptr);
  ChildListMutationAccumulator* accumulator;
  if (!result.is_new_entry) {
    accumulator = result.stored_value->value;
  } else {
    accumulator = MakeGarbageCollected<ChildListMutationAccumulator>(
        &target,
        MutationObserverInterestGroup::CreateForChildListMutation(target));
    result.stored_value->value = accumulator;
  }
  return accumulator;
}

inline bool ChildListMutationAccumulator::IsAddedNodeInOrder(Node& child) {
  return IsEmpty() || (last_added_ == child.previousSibling() &&
                       next_sibling_ == child.nextSibling());
}

void ChildListMutationAccumulator::ChildAdded(Node& child) {
  DCHECK(HasObservers());

  if (!IsAddedNodeInOrder(child))
    EnqueueMutationRecord();

  if (IsEmpty()) {
    previous_sibling_ = child.previousSibling();
    next_sibling_ = child.nextSibling();
  }

  last_added_ = &child;
  added_nodes_.push_back(&child);
}

inline bool ChildListMutationAccumulator::IsRemovedNodeInOrder(Node& child) {
  return IsEmpty() || next_sibling_ == &child;
}

void ChildListMutationAccumulator::WillRemoveChild(Node& child) {
  DCHECK(HasObservers());

  if (!added_nodes_.empty() || !IsRemovedNodeInOrder(child))
    EnqueueMutationRecord();

  if (IsEmpty()) {
    previous_sibling_ = child.previousSibling();
    next_sibling_ = child.nextSibling();
    last_added_ = child.previousSibling();
  } else {
    next_sibling_ = child.nextSibling();
  }

  removed_nodes_.push_back(&child);
}

void ChildListMutationAccumulator::EnqueueMutationRecord() {
  DCHECK(HasObservers());
  DCHECK(!IsEmpty());

  StaticNodeList* added_nodes = StaticNodeList::Adopt(added_nodes_);
  StaticNodeList* removed_nodes = StaticNodeList::Adopt(removed_nodes_);
  MutationRecord* record = MutationRecord::CreateChildList(
      target_, added_nodes, removed_nodes, previous_sibling_.Release(),
      next_sibling_.Release());
  observers_->EnqueueMutationRecord(record);
  last_added_ = nullptr;
  DCHECK(IsEmpty());
}

bool ChildListMutationAccumulator::IsEmpty() {
  bool result = removed_nodes_.empty() && added_nodes_.empty();
#if DCHECK_IS_ON()
  if (result) {
    DCHECK(!previous_sibling_);
    DCHECK(!next_sibling_);
    DCHECK(!last_added_);
  }
#endif
  return result;
}

void ChildListMutationAccumulator::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(removed_nodes_);
  visitor->Trace(added_nodes_);
  visitor->Trace(previous_sibling_);
  visitor->Trace(next_sibling_);
  visitor->Trace(last_added_);
  visitor->Trace(observers_);
}

}  // namespace blink
