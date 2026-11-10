// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

ContextLifecycleNotifier::~ContextLifecycleNotifier() {
  // `NotifyContextDestroyed()` must be called prior to destruction.
  DCHECK(context_destroyed_);
}

bool ContextLifecycleNotifier::IsContextDestroyed() const {
  return context_destroyed_;
}

void ContextLifecycleNotifier::AddContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  observers_.AddObserver(observer);
}

void ContextLifecycleNotifier::RemoveContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void ContextLifecycleNotifier::NotifyContextDestroyed() {
  // `NotifyContextDestroyed()` must always be executed from a context that can
  // allocate (e.g. never from a pre-finalizer or
  // `DisallowGarbageCollectionScope`).
  CHECK(HeapAllocator::IsAllocationAllowed());
  ScriptForbiddenScope forbid_script;

  // Keep live observers strongly rooted across `NotifyContextDestroyed()`. Once
  // `context_destroyed_` becomes true, mechanisms such as
  // `ActiveScriptWrappableManager` stop keeping active objects alive, so a GC
  // triggered during one observer's `ContextDestroyed()` could otherwise
  // collect remaining observers and clear their entries in `observers_` before
  // they are notified.
  HeapVector<Member<ContextLifecycleObserver>> keep_alive;
  observers_.ForEachObserver([&](ContextLifecycleObserver* observer) {
    keep_alive.push_back(observer);
  });

  context_destroyed_ = true;
  observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
    observer->NotifyContextDestroyed();
  });
  observers_.Clear();
  // Explicitly clear `keep_alive` after notification finishes so the compiler
  // cannot dead-store-eliminate its trivially-destructible stack slot before
  // `ForEachObserver` completes.
  keep_alive.clear();
}

void ContextLifecycleNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink
