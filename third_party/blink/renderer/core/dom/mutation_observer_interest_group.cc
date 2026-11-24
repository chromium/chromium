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

#include "third_party/blink/renderer/core/dom/mutation_observer_interest_group.h"

#include "third_party/blink/renderer/core/dom/active_template_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/child_list_mutation_scope.h"
#include "third_party/blink/renderer/core/dom/mutation_record.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"

namespace blink {

MutationObserverInterestGroup* MutationObserverInterestGroup::CreateIfNeeded(
    Node& target,
    MutationType type,
    MutationRecordDeliveryOptions old_value_flag,
    const QualifiedName* attribute_name) {
  DCHECK((type == kMutationTypeAttributes && attribute_name) ||
         !attribute_name);
  HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>
      observers;
  target.GetRegisteredMutationObserversOfType(observers, type, attribute_name);
  if (observers.empty())
    return nullptr;

  return MakeGarbageCollected<MutationObserverInterestGroup>(observers,
                                                             old_value_flag);
}

MutationObserverInterestGroup::MutationObserverInterestGroup(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    MutationRecordDeliveryOptions old_value_flag)
    : old_value_flag_(old_value_flag) {
  DCHECK(!observers.empty());
  observers_.swap(observers);
}

bool MutationObserverInterestGroup::IsOldValueRequested() {
  for (auto& observer : observers_) {
    if (HasOldValue(observer.value))
      return true;
  }
  return false;
}

void MutationObserverInterestGroup::EnqueueMutationRecord(
    MutationRecord* mutation) {
  if (ActiveTemplateMutationScope* active_scope =
          ActiveTemplateMutationScope::Current()) {
    EnqueueMutationRecord(mutation, active_scope->status(),
                          active_scope->invocation());
    return;
  }
  EnqueueMutationRecord(mutation, std::nullopt, nullptr);
}

void MutationObserverInterestGroup::EnqueueMutationRecord(
    MutationRecord* mutation,
    std::optional<V8MutationRecordActiveTemplateStatus::Enum>
        active_template_status,
    ActiveTemplateInvocation* active_template_invocation) {
  MutationRecord* mutation_with_null_old_value = nullptr;

  for (auto& iter : observers_) {
    MutationObserver* observer = iter.key.Get();
    MutationRecordDeliveryOptions delivery_options = iter.value;

    MutationRecord* base_record = nullptr;
    if (HasOldValue(delivery_options)) {
      base_record = mutation;
    } else {
      if (!mutation_with_null_old_value) {
        if (mutation->oldValue().IsNull()) {
          mutation_with_null_old_value = mutation;
        } else {
          mutation_with_null_old_value =
              MutationRecord::CreateWithNullOldValue(mutation);
        }
      }
      base_record = mutation_with_null_old_value;
    }

    if (active_template_status && active_template_invocation &&
        (delivery_options & MutationObserver::kActiveTemplateAttributeFilter)) {
      ActiveTemplateFilterEntry* entry =
          active_template_invocation->GetOrCreateEntry(*observer,
                                                       *mutation->target());
      observer->EnqueueMutationRecord(MutationRecord::CreateWithActiveTemplate(
          base_record, *active_template_status, entry));
    } else {
      observer->EnqueueMutationRecord(base_record);
    }
  }
}

// static
void MutationObserverInterestGroup::EnqueueActiveTemplateRecord(
    Node& target,
    V8MutationRecordActiveTemplateStatus::Enum status,
    ActiveTemplateInvocation* invocation,
    Node* previous_sibling,
    Node* next_sibling) {
  if (!invocation ||
      !target.GetDocument().MayHaveActiveTemplateMutationObservers()) {
    return;
  }
  ChildListMutationAccumulator::FlushIfNeeded(target);
  MutationObserverInterestGroup* group = CreateForChildListMutation(target);
  if (!group) {
    return;
  }
  MutationRecord* empty_record = nullptr;
  for (auto& iter : group->observers_) {
    if (!(iter.value & MutationObserver::kActiveTemplateAttributeFilter)) {
      continue;
    }
    if (!empty_record) {
      empty_record = MutationRecord::CreateChildList(
          &target, MakeGarbageCollected<StaticNodeList>(),
          MakeGarbageCollected<StaticNodeList>(), previous_sibling,
          next_sibling);
    }
    MutationObserver* observer = iter.key.Get();
    ActiveTemplateFilterEntry* entry =
        invocation->GetOrCreateEntry(*observer, target);
    observer->EnqueueMutationRecord(
        MutationRecord::CreateWithActiveTemplate(empty_record, status, entry));
  }
}

void MutationObserverInterestGroup::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink
