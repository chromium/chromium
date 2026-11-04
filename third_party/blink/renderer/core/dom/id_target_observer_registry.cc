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
}

void IdTargetObserverRegistry::AddObserver(const AtomicString& id,
                                           IdTargetObserver* observer) {
  if (id.empty())
    return;

  IdToObserverSetMap::AddResult result = registry_.insert(id.Impl(), nullptr);
  if (result.is_new_entry)
    result.stored_value->value = MakeGarbageCollected<ObserverSet>();

  auto insert_result = result.stored_value->value->insert(observer);
  CHECK(insert_result.is_new_entry);
}

void IdTargetObserverRegistry::RemoveObserver(const AtomicString& id,
                                              IdTargetObserver* observer) {
  if (id.empty()) {
    return;
  }

  IdToObserverSetMap::iterator iter = registry_.find(id.Impl());
  CHECK(iter != registry_.end());

  ObserverSet* set = iter->value.Get();
  set->erase(observer);
  if (set->empty()) {
    registry_.erase(iter);
  }
}

void IdTargetObserverRegistry::NotifyObserversInternal(const AtomicString& id) {
  DCHECK(!id.empty());
  DCHECK(!registry_.empty());

  // Note that `id` may be a reference into an element's AttributeCollection
  // buffer, which an observer's IdTargetChanged() callback can reallocate and
  // free, so `id` must not be accessed after any observer has been notified.
  auto it_reg = registry_.find(id.Impl());
  if (it_reg == registry_.end()) {
    return;
  }

  // `set` may be erased from `registry_` during the loop, but stays alive on
  // the stack. Observers are never re-added, so checking `set` is enough to
  // skip the ones unregistered by earlier callbacks.
  ObserverSet* set = it_reg->value.Get();
  HeapVector<Member<IdTargetObserver>> copy(*set);
  for (const auto& observer : copy) {
    if (set->Contains(observer)) {
      observer->IdTargetChanged();
    }
  }
}

bool IdTargetObserverRegistry::HasObservers(const AtomicString& id) const {
  if (id.empty() || registry_.empty())
    return false;
  auto it = registry_.find(id.Impl());
  return it != registry_.end() && !it->value->empty();
}

}  // namespace blink
