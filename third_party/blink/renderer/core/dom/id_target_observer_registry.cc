/*
 * Copyright (C) 2012 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"

#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

void IdTargetObserverRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(registry_);
  visitor->Trace(notifying_observers_in_set_);
}

void IdTargetObserverRegistry::AddObserver(const AtomicString& id,
                                           IdTargetObserver* observer) {
  if (id.empty())
    return;

  IdToObserverSetMap::AddResult result = registry_.insert(id.Impl(), nullptr);
  if (result.is_new_entry)
    result.stored_value->value = MakeGarbageCollected<ObserverSet>();

  result.stored_value->value->insert(observer);
}

void IdTargetObserverRegistry::RemoveObserver(const AtomicString& id,
                                              IdTargetObserver* observer) {
  if (id.empty() || registry_.empty())
    return;

  IdToObserverSetMap::iterator iter = registry_.find(id.Impl());

  ObserverSet* set = iter->value.Get();
  set->erase(observer);
  if (set->empty() && set != notifying_observers_in_set_)
    registry_.erase(iter);
}

void IdTargetObserverRegistry::NotifyObserversInternal(const AtomicString& id) {
  DCHECK(!id.empty());
  DCHECK(!registry_.empty());

  auto it_reg = registry_.find(id.Impl());
  if (it_reg != registry_.end())
    notifying_observers_in_set_ = it_reg->value;
  if (!notifying_observers_in_set_)
    return;

  HeapVector<Member<IdTargetObserver>> copy(*notifying_observers_in_set_);
  for (const auto& observer : copy) {
    if (notifying_observers_in_set_->Contains(observer))
      observer->IdTargetChanged();
  }

  if (notifying_observers_in_set_->empty())
    registry_.erase(id.Impl());

  notifying_observers_in_set_ = nullptr;
}

bool IdTargetObserverRegistry::HasObservers(const AtomicString& id) const {
  if (id.empty() || registry_.empty())
    return false;
  auto it = registry_.find(id.Impl());
  return it != registry_.end() ? !it->value->empty() : false;
}

}  // namespace blink
