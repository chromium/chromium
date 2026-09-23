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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_LIST_MUTATION_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_LIST_MUTATION_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MutationObserverInterestGroup;

// ChildListMutationAccumulator is not meant to be used directly;
// ChildListMutationScope is the public interface.
//
// One ChildListMutationAccumulator for a given Node is shared between all the
// active ChildListMutationScopes for that Node. Once the last
// ChildListMutationScope is destructed the accumulator enqueues a mutation
// record for the recorded mutations and the accumulator can be garbage
// collected.
class CORE_EXPORT ChildListMutationAccumulator final
    : public GarbageCollected<ChildListMutationAccumulator> {
 public:
  // Returns the accumulator shared by all active scopes for the node, creating
  // one if needed, or nullptr if no MutationObserver is interested in child
  // list mutations of the node.
  static ChildListMutationAccumulator* GetOrCreate(Node&);

  ChildListMutationAccumulator(Node*, MutationObserverInterestGroup*);

  void ChildAdded(Node&);
  void WillRemoveChild(Node&);

  // Register and unregister mutation scopes that are using this mutation
  // accumulator.
  void EnterMutationScope() { mutation_scopes_++; }
  void LeaveMutationScope();

  void Trace(Visitor*) const;

 private:
  void EnqueueMutationRecord();
  bool IsEmpty();
  bool IsAddedNodeInOrder(Node&);
  bool IsRemovedNodeInOrder(Node&);

  HeapVector<Member<Node>> removed_nodes_;
  HeapVector<Member<Node>> added_nodes_;
  Member<Node> target_;
  Member<Node> previous_sibling_;
  Member<Node> next_sibling_;
  Member<Node> last_added_;

  Member<MutationObserverInterestGroup> observers_;

  unsigned mutation_scopes_;
};

// Put one of these on the stack around a compound child list change of
// |target| (e.g. inserting a DocumentFragment) so that MutationObservers get
// one record for it. Nested scopes for the same target join the outermost one.
class ChildListMutationScope final {
  STACK_ALLOCATED();

 public:
  explicit ChildListMutationScope(Node& target) : target_(&target) {
    Document& document = target.GetDocument();
    if (!document.MayHaveMutationObserversOfType(kMutationTypeChildList) ||
        document.UnobservedChildListMutationTarget() == &target) {
      return;
    }
    accumulator_ = ChildListMutationAccumulator::GetOrCreate(target);
    if (accumulator_) {
      // Register another user of the accumulator.
      accumulator_->EnterMutationScope();
      return;
    }
    // Nobody observes |target|. Note that on the document while this scope
    // is active, so that nested scopes for |target| skip the observer lookup,
    // the way they would share this scope's accumulator if there were one (so
    // an observer registered before this scope ends gets no record for this
    // operation, as before). There is one slot: a scope for another
    // unobserved target nested in this one does the lookup each time.
    if (!document.UnobservedChildListMutationTarget()) {
      cached_unobserved_document_ = &document;
      document.SetUnobservedChildListMutationTarget(&target);
    }
  }
  ChildListMutationScope(const ChildListMutationScope&) = delete;
  ChildListMutationScope& operator=(const ChildListMutationScope&) = delete;

  ~ChildListMutationScope() {
    if (accumulator_) {
      // Unregister a user of the accumulator. If this is the last user the
      // accumulator will enqueue a mutation record for the mutations.
      accumulator_->LeaveMutationScope();
    } else if (cached_unobserved_document_) {
      DCHECK_EQ(
          cached_unobserved_document_->UnobservedChildListMutationTarget(),
          target_);
      cached_unobserved_document_->SetUnobservedChildListMutationTarget(
          nullptr);
    }
  }

  void ChildAdded(Node& child) {
    if (accumulator_) {
      accumulator_->ChildAdded(child);
    }
  }

  void WillRemoveChild(Node& child) {
    if (accumulator_) {
      accumulator_->WillRemoveChild(child);
    }
  }

 private:
  ChildListMutationAccumulator* accumulator_ = nullptr;
  Node* target_;
  // The document whose UnobservedChildListMutationTarget() this scope set to
  // |target_|, if it did; the destructor resets it.
  Document* cached_unobserved_document_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_LIST_MUTATION_SCOPE_H_
