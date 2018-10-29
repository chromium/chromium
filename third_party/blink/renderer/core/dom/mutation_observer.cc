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

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mutation_callback.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_init.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

class MutationObserver::V8DelegateImpl final
    : public MutationObserver::Delegate,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(V8DelegateImpl);

 public:
  static V8DelegateImpl* Create(V8MutationCallback* callback,
                                ExecutionContext* execution_context) {
    return new V8DelegateImpl(callback, execution_context);
  }

  ExecutionContext* GetExecutionContext() const override {
    return ContextClient::GetExecutionContext();
  }

  void Deliver(const MutationRecordVector& records,
               MutationObserver& observer) override {
    // https://dom.spec.whatwg.org/#notify-mutation-observers
    // step 5-4. specifies that the callback this value is a MutationObserver.
    callback_->InvokeAndReportException(&observer, records, &observer);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(callback_);
    MutationObserver::Delegate::Trace(visitor);
    ContextClient::Trace(visitor);
  }

 private:
  V8DelegateImpl(V8MutationCallback* callback,
                 ExecutionContext* execution_context)
      : ContextClient(execution_context), callback_(callback) {}

  TraceWrapperMember<V8MutationCallback> callback_;
};

static unsigned g_observer_priority = 0;

struct MutationObserver::ObserverLessThan {
  bool operator()(const Member<MutationObserver>& lhs,
                  const Member<MutationObserver>& rhs) {
    return lhs->priority_ < rhs->priority_;
  }
};

MutationObserver* MutationObserver::Create(Delegate* delegate) {
  DCHECK(IsMainThread());
  return new MutationObserver(delegate->GetExecutionContext(), delegate);
}

MutationObserver* MutationObserver::Create(ScriptState* script_state,
                                           V8MutationCallback* callback) {
  DCHECK(IsMainThread());
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  return new MutationObserver(
      execution_context, V8DelegateImpl::Create(callback, execution_context));
}

MutationObserver::MutationObserver(ExecutionContext* execution_context,
                                   Delegate* delegate)
    : ContextClient(execution_context),
      delegate_(delegate),
      priority_(g_observer_priority++) {}

MutationObserver::~MutationObserver() {
  CancelInspectorAsyncTasks();
}

void MutationObserver::observe(Node* node,
                               const MutationObserverInit& observer_init,
                               ExceptionState& exception_state) {
  DCHECK(node);

  MutationObserverOptions options = 0;

  if (observer_init.hasAttributeOldValue() && observer_init.attributeOldValue())
    options |= kAttributeOldValue;

  HashSet<AtomicString> attribute_filter;
  if (observer_init.hasAttributeFilter()) {
    for (const auto& name : observer_init.attributeFilter())
      attribute_filter.insert(AtomicString(name));
    options |= kAttributeFilter;
  }

  bool attributes = observer_init.hasAttributes() && observer_init.attributes();
  if (attributes || (!observer_init.hasAttributes() &&
                     (observer_init.hasAttributeOldValue() ||
                      observer_init.hasAttributeFilter())))
    options |= kMutationTypeAttributes;

  if (observer_init.hasCharacterDataOldValue() &&
      observer_init.characterDataOldValue())
    options |= kCharacterDataOldValue;

  bool character_data =
      observer_init.hasCharacterData() && observer_init.characterData();
  if (character_data || (!observer_init.hasCharacterData() &&
                         observer_init.hasCharacterDataOldValue()))
    options |= kMutationTypeCharacterData;

  if (observer_init.childList())
    options |= kMutationTypeChildList;

  if (observer_init.subtree())
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
  DCHECK(registrations_.IsEmpty());
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

static MutationObserverSet& ActiveMutationObservers() {
  DEFINE_STATIC_LOCAL(Persistent<MutationObserverSet>, active_observers,
                      (new MutationObserverSet));
  return *active_observers;
}

using SlotChangeList = HeapVector<Member<HTMLSlotElement>>;

// TODO(hayato): We should have a SlotChangeList for each unit of related
// similar-origin browsing context.
// https://html.spec.whatwg.org/multipage/browsers.html#unit-of-related-similar-origin-browsing-contexts
static SlotChangeList& ActiveSlotChangeList() {
  DEFINE_STATIC_LOCAL(Persistent<SlotChangeList>, slot_change_list,
                      (new SlotChangeList));
  return *slot_change_list;
}

static MutationObserverSet& SuspendedMutationObservers() {
  DEFINE_STATIC_LOCAL(Persistent<MutationObserverSet>, suspended_observers,
                      (new MutationObserverSet));
  return *suspended_observers;
}

static void EnsureEnqueueMicrotask() {
  if (ActiveMutationObservers().IsEmpty() && ActiveSlotChangeList().IsEmpty())
    Microtask::EnqueueMicrotask(WTF::Bind(&MutationObserver::DeliverMutations));
}

void MutationObserver::EnqueueSlotChange(HTMLSlotElement& slot) {
  DCHECK(IsMainThread());
  EnsureEnqueueMicrotask();
  ActiveSlotChangeList().push_back(&slot);
}

void MutationObserver::CleanSlotChangeList(Document& document) {
  SlotChangeList kept;
  kept.ReserveCapacity(ActiveSlotChangeList().size());
  for (auto& slot : ActiveSlotChangeList()) {
    if (slot->GetDocument() != document)
      kept.push_back(slot);
  }
  ActiveSlotChangeList().swap(kept);
}

static void ActivateObserver(MutationObserver* observer) {
  EnsureEnqueueMicrotask();
  ActiveMutationObservers().insert(observer);
}

void MutationObserver::EnqueueMutationRecord(MutationRecord* mutation) {
  DCHECK(IsMainThread());
  records_.push_back(mutation);
  ActivateObserver(this);
  probe::AsyncTaskScheduled(delegate_->GetExecutionContext(), mutation->type(),
                            mutation);
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

bool MutationObserver::ShouldBeSuspended() const {
  const ExecutionContext* execution_context = delegate_->GetExecutionContext();
  return execution_context && execution_context->IsContextPaused();
}

void MutationObserver::CancelInspectorAsyncTasks() {
  for (auto& record : records_)
    probe::AsyncTaskCanceled(delegate_->GetExecutionContext(), record);
}

void MutationObserver::Deliver() {
  DCHECK(!ShouldBeSuspended());

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

  if (records_.IsEmpty())
    return;

  MutationRecordVector records;
  swap(records_, records);

  // Report the first (earliest) stack as the async cause.
  probe::AsyncTask async_task(delegate_->GetExecutionContext(),
                              records.front());
  delegate_->Deliver(records, *this);
}

void MutationObserver::ResumeSuspendedObservers() {
  DCHECK(IsMainThread());
  if (SuspendedMutationObservers().IsEmpty())
    return;

  MutationObserverVector suspended;
  CopyToVector(SuspendedMutationObservers(), suspended);
  for (const auto& observer : suspended) {
    if (!observer->ShouldBeSuspended()) {
      SuspendedMutationObservers().erase(observer);
      ActivateObserver(observer);
    }
  }
}

void MutationObserver::DeliverMutations() {
  // These steps are defined in DOM Standard's "notify mutation observers".
  // https://dom.spec.whatwg.org/#notify-mutation-observers
  DCHECK(IsMainThread());

  MutationObserverVector observers;
  CopyToVector(ActiveMutationObservers(), observers);
  ActiveMutationObservers().clear();

  SlotChangeList slots;
  slots.swap(ActiveSlotChangeList());
  for (const auto& slot : slots)
    slot->ClearSlotChangeEventEnqueued();

  std::sort(observers.begin(), observers.end(), ObserverLessThan());
  for (const auto& observer : observers) {
    if (!observer->GetExecutionContext()) {
      // The observer's execution context is already gone, as active observers
      // intentionally do not hold their execution context. Do nothing then.
      continue;
    }

    if (observer->ShouldBeSuspended())
      SuspendedMutationObservers().insert(observer);
    else
      observer->Deliver();
  }
  for (const auto& slot : slots)
    slot->DispatchSlotChangeEvent();
}

void MutationObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(delegate_);
  visitor->Trace(records_);
  visitor->Trace(registrations_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
