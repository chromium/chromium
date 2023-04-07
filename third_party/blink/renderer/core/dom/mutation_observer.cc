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

#include "third_party/blink/renderer/core/dom/mutation_observer.h"

#include <algorithm>

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

using SlotChangeList = HeapVector<Member<HTMLSlotElement>>;

static unsigned g_observer_priority = 0;
struct MutationObserver::ObserverLessThan {
  bool operator()(const Member<MutationObserver>& lhs,
                  const Member<MutationObserver>& rhs) {
    return lhs->priority_ < rhs->priority_;
  }
};

class MutationObserverAgentData
    : public GarbageCollected<MutationObserverAgentData>,
      public Supplement<Agent> {
 public:
  constexpr static const char kSupplementName[] = "MutationObserverAgentData";

  explicit MutationObserverAgentData(Agent& agent) : Supplement<Agent>(agent) {}

  static MutationObserverAgentData& From(Agent& agent) {
    MutationObserverAgentData* supplement =
        Supplement<Agent>::From<MutationObserverAgentData>(agent);
    if (!supplement) {
      supplement = MakeGarbageCollected<MutationObserverAgentData>(agent);
      ProvideTo(agent, supplement);
    }
    return *supplement;
  }

  void Trace(Visitor* visitor) const override {
    Supplement<Agent>::Trace(visitor);
    visitor->Trace(active_mutation_observers_);
    visitor->Trace(active_slot_change_list_);
  }

  void EnqueueSlotChange(HTMLSlotElement& slot) {
    EnsureEnqueueMicrotask();
    active_slot_change_list_.push_back(&slot);
  }

  void CleanSlotChangeList(Document& document) {
    SlotChangeList kept;
    kept.reserve(active_slot_change_list_.size());
    for (auto& slot : active_slot_change_list_) {
      if (slot->GetDocument() != document)
        kept.push_back(slot);
    }
    active_slot_change_list_.swap(kept);
  }

  void ActivateObserver(MutationObserver* observer) {
    EnsureEnqueueMicrotask();
    active_mutation_observers_.insert(observer);
  }

  void ClearActiveObserver(MutationObserver* observer) {
    active_mutation_observers_.erase(observer);
  }

 private:
  void EnsureEnqueueMicrotask() {
    if (active_mutation_observers_.empty() &&
        active_slot_change_list_.empty()) {
      GetSupplementable()->event_loop()->EnqueueMicrotask(
          WTF::BindOnce(&MutationObserverAgentData::DeliverMutations,
                        WrapWeakPersistent(this)));
    }
  }

  void DeliverMutations() {
    // These steps are defined in DOM Standard's "notify mutation observers".
    // https://dom.spec.whatwg.org/#notify-mutation-observers
    DCHECK(IsMainThread());
    MutationObserverVector observers(active_mutation_observers_);
    active_mutation_observers_.clear();
    SlotChangeList slots;
    slots.swap(active_slot_change_list_);
    for (const auto& slot : slots)
      slot->ClearSlotChangeEventEnqueued();
    std::sort(observers.begin(), observers.end(),
              MutationObserver::ObserverLessThan());
    for (const auto& observer : observers)
      observer->Deliver();
    for (const auto& slot : slots)
      slot->DispatchSlotChangeEvent();
  }

 private:
  // For MutationObserver.
  MutationObserverSet active_mutation_observers_;
  SlotChangeList active_slot_change_list_;
};

class MutationObserver::V8DelegateImpl final
    : public MutationObserver::Delegate,
      public ExecutionContextClient {
 public:
  static V8DelegateImpl* Create(V8MutationCallback* callback,
                                ExecutionContext* execution_context) {
    return MakeGarbageCollected<V8DelegateImpl>(callback, execution_context);
  }

  V8DelegateImpl(V8MutationCallback* callback,
                 ExecutionContext* execution_context)
      : ExecutionContextClient(execution_context), callback_(callback) {}

  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextClient::GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver& observer) override {
    // https://dom.spec.whatwg.org/#notify-mutation-observers
    // step 5-4. specifies that the callback this value is a MutationObserver.
    callback_->InvokeAndReportException(&observer, records, &observer);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(callback_);
    MutationObserver::Delegate::Trace(visitor);
    ExecutionContextClient::Trace(visitor);
  }

 private:
  Member<V8MutationCallback> callback_;
};

MutationObserver* MutationObserver::Create(Delegate* delegate) {
  DCHECK(IsMainThread());
  return MakeGarbageCollected<MutationObserver>(delegate->GetExecutionContext(),
                                                delegate);
}

MutationObserver* MutationObserver::Create(ScriptState* script_state,
                                           V8MutationCallback* callback) {
  DCHECK(IsMainThread());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return MakeGarbageCollected<MutationObserver>(
      execution_context, V8DelegateImpl::Create(callback, execution_context));
}

MutationObserver::MutationObserver(ExecutionContext* execution_context,
                                   Delegate* delegate)
    : ActiveScriptWrappable<MutationObserver>({}),
      ExecutionContextLifecycleStateObserver(execution_context),
      delegate_(delegate) {
  priority_ = g_observer_priority++;
  UpdateStateIfNeeded();
}

MutationObserver::~MutationObserver() = default;

void MutationObserver::observe(Node* node,
                               const MutationObserverInit* observer_init,
                               ExceptionState& exception_state) {
  DCHECK(node);

  MutationObserverOptions options = 0;

  if (observer_init->hasAttributeOldValue() &&
      observer_init->attributeOldValue())
    options |= kAttributeOldValue;

  HashSet<AtomicString> attribute_filter;
  if (observer_init->hasAttributeFilter()) {
    for (const auto& name : observer_init->attributeFilter())
      attribute_filter.insert(AtomicString(name));
    options |= kAttributeFilter;
  }

  bool attributes =
      observer_init->hasAttributes() && observer_init->attributes();
  if (attributes || (!observer_init->hasAttributes() &&
                     (observer_init->hasAttributeOldValue() ||
                      observer_init->hasAttributeFilter())))
    options |= kMutationTypeAttributes;

  if (observer_init->hasCharacterDataOldValue() &&
      observer_init->characterDataOldValue())
    options |= kCharacterDataOldValue;

  bool character_data =
      observer_init->hasCharacterData() && observer_init->characterData();
  if (character_data || (!observer_init->hasCharacterData() &&
                         observer_init->hasCharacterDataOldValue()))
    options |= kMutationTypeCharacterData;

  if (observer_init->childList())
    options |= kMutationTypeChildList;

  if (observer_init->subtree())
    options |= kSubtree;

  if (!(options & kMutationTypeAttributes)) {
    if (options & kAttributeOldValue) {
      exception_state.ThrowTypeError(
          "The options object may only set 'attributeOldValue' to true when "
          "'attributes' is true or not present.");
      return;
    }
    if (options & kAttributeFilter) {
      exception_state.ThrowTypeError(
          "The options object may only set 'attributeFilter' when 'attributes' "
          "is true or not present.");
      return;
    }
  }
  if (!((options & kMutationTypeCharacterData) ||
        !(options & kCharacterDataOldValue))) {
    exception_state.ThrowTypeError(
        "The options object may only set 'characterDataOldValue' to true when "
        "'characterData' is true or not present.");
    return;
  }

  if (!(options & kMutationTypeAll)) {
    exception_state.ThrowTypeError(
        "The options object must set at least one of 'attributes', "
        "'characterData', or 'childList' to true.");
    return;
  }

  node->RegisterMutationObserver(*this, options, attribute_filter);
}

MutationRecordVector MutationObserver::takeRecords() {
  MutationRecordVector records;
  CancelInspectorAsyncTasks();
  swap(records_, records);
  return records;
}

void MutationObserver::disconnect() {
  CancelInspectorAsyncTasks();
  records_.clear();
  MutationObserverRegistrationSet registrations(registrations_);
  for (auto& registration : registrations) {
    // The registration may be already unregistered while iteration.
    // Only call unregister if it is still in the original set.
    if (registrations_.Contains(registration))
      registration->Unregister();
  }
  DCHECK(registrations_.empty());
}

void MutationObserver::ObservationStarted(
    MutationObserverRegistration* registration) {
  DCHECK(!registrations_.Contains(registration));
  registrations_.insert(registration);
}

void MutationObserver::ObservationEnded(
    MutationObserverRegistration* registration) {
  DCHECK(registrations_.Contains(registration));
  registrations_.erase(registration);
}

// static
void MutationObserver::EnqueueSlotChange(HTMLSlotElement& slot) {
  DCHECK(IsMainThread());
  MutationObserverAgentData::From(slot.GetDocument().GetAgent())
      .EnqueueSlotChange(slot);
}

// static
void MutationObserver::CleanSlotChangeList(Document& document) {
  MutationObserverAgentData::From(document.GetAgent())
      .CleanSlotChangeList(document);
}

static void ActivateObserver(MutationObserver* observer) {
  if (!observer->GetExecutionContext())
    return;
  MutationObserverAgentData::From(*observer->GetExecutionContext()->GetAgent())
      .ActivateObserver(observer);
}

void MutationObserver::EnqueueMutationRecord(MutationRecord* mutation) {
  DCHECK(IsMainThread());
  records_.push_back(mutation);
  ActivateObserver(this);
  mutation->async_task_context()->Schedule(delegate_->GetExecutionContext(),
                                           mutation->type());
}

void MutationObserver::SetHasTransientRegistration() {
  DCHECK(IsMainThread());
  ActivateObserver(this);
}

HeapHashSet<Member<Node>> MutationObserver::GetObservedNodes() const {
  HeapHashSet<Member<Node>> observed_nodes;
  for (const auto& registration : registrations_)
    registration->AddRegistrationNodesToSet(observed_nodes);
  return observed_nodes;
}

void MutationObserver::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning)
    ActivateObserver(this);
}

void MutationObserver::ContextDestroyed() {
  // The 'DeliverMutations' micro task is *not* guaranteed to run.
  // It's necessary to clear out this observer from the list of active observers
  // in case the MutationObserverAgentData is reused across navigations.
  // Otherwise no MutationObserver for the agent can fire again.
  DCHECK(GetExecutionContext());
  MutationObserverAgentData::From(*GetExecutionContext()->GetAgent())
      .ClearActiveObserver(this);
}

void MutationObserver::CancelInspectorAsyncTasks() {
  for (auto& record : records_) {
    record->async_task_context()->Cancel();
  }
}

void MutationObserver::Deliver() {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextPaused())
    return;

  // Calling ClearTransientRegistrations() can modify registrations_, so it's
  // necessary to make a copy of the transient registrations before operating on
  // them.
  HeapVector<Member<MutationObserverRegistration>, 1> transient_registrations;
  for (auto& registration : registrations_) {
    if (registration->HasTransientRegistrations())
      transient_registrations.push_back(registration);
  }
  for (const auto& registration : transient_registrations)
    registration->ClearTransientRegistrations();

  if (records_.empty())
    return;

  MutationRecordVector records;
  swap(records_, records);

  // Report the first (earliest) stack as the async cause.
  probe::AsyncTask async_task(delegate_->GetExecutionContext(),
                              records.front()->async_task_context());
  delegate_->Deliver(records, *this);
}

void MutationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  visitor->Trace(records_);
  visitor->Trace(registrations_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
