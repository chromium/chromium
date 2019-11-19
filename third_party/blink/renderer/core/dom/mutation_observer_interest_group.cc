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

#include "third_party/blink/renderer/core/dom/mutation_record.h"

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
  if (observers.IsEmpty())
    return nullptr;

  return MakeGarbageCollected<MutationObserverInterestGroup>(observers,
                                                             old_value_flag);
}

MutationObserverInterestGroup::MutationObserverInterestGroup(
    HeapHashMap<Member<MutationObserver>, MutationRecordDeliveryOptions>&
        observers,
    MutationRecordDeliveryOptions old_value_flag)
    : old_value_flag_(old_value_flag) {
  DCHECK(!observers.IsEmpty());
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
  MutationRecord* mutation_with_null_old_value = nullptr;

  // For investigation of crbug.com/1003733.
  // If the crashes stop it means there is a GC related issue.
  ThreadState::GCForbiddenScope gc_forbidden(ThreadState::Current());

  for (auto& iter : observers_) {
    MutationObserver* observer = iter.key.Get();
    if (HasOldValue(iter.value)) {
      observer->EnqueueMutationRecord(mutation);
      continue;
    }
    if (!mutation_with_null_old_value) {
      if (mutation->oldValue().IsNull())
        mutation_with_null_old_value = mutation;
      else
        mutation_with_null_old_value =
            MutationRecord::CreateWithNullOldValue(mutation);
    }
    observer->EnqueueMutationRecord(mutation_with_null_old_value);
  }
}

void MutationObserverInterestGroup::Trace(Visitor* visitor) {
  visitor->Trace(observers_);
}

}  // namespace blink
