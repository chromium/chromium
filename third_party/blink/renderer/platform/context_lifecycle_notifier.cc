// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

<<<<<<< HEAD
#include "base/record_replay.h"
||||||| 80c960997e61f
=======
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace blink {

ContextLifecycleNotifier::ContextLifecycleNotifier() {
  // https://linear.app/replay/issue/RUN-806
  recordreplay::RegisterPointer("ContextLifecycleNotifier", this);
}

ContextLifecycleNotifier::~ContextLifecycleNotifier() {
<<<<<<< HEAD
  // https://linear.app/replay/issue/RUN-806
  recordreplay::UnregisterPointer(this);

#if DCHECK_IS_ON()
||||||| 80c960997e61f
#if DCHECK_IS_ON()
=======
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  // `NotifyContextDestroyed()` must be called prior to destruction.
  DCHECK(context_destroyed_);
}

bool ContextLifecycleNotifier::IsContextDestroyed() const {
  return context_destroyed_;
}

void ContextLifecycleNotifier::AddContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  // https://linear.app/replay/issue/RUN-806
  recordreplay::Assert("ContextLifecycleNotifier::AddContextLifecycleObserver %d %d",
                       recordreplay::PointerId(this),
                       recordreplay::PointerId(observer));

  observers_.AddObserver(observer);
}

void ContextLifecycleNotifier::RemoveContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  // https://linear.app/replay/issue/RUN-806
  recordreplay::Assert("ContextLifecycleNotifier::RemoveContextLifecycleObserver %d %d",
                       recordreplay::PointerId(this),
                       recordreplay::PointerId(observer));

  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void ContextLifecycleNotifier::NotifyContextDestroyed() {
<<<<<<< HEAD
  // https://linear.app/replay/issue/RUN-806
  recordreplay::Assert("ContextLifecycleNotifier::NotifyContextDestroyed %d",
                       recordreplay::PointerId(this));

  // Manually ensure we notify observers in a consistent order when recording
  // vs. replaying. It would be better to ensure the observers_ set is iterated
  // deterministically, but this is easier for now.
  std::vector<ContextLifecycleObserver*> observers;
  observers_.ForEachObserver([&](ContextLifecycleObserver* observer) {
    observers.push_back(observer);
||||||| 80c960997e61f
  observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
    observer->NotifyContextDestroyed();
=======
  context_destroyed_ = true;

  ScriptForbiddenScope forbid_script;
  observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
    observer->NotifyContextDestroyed();
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
  });
  observers_.Clear();
<<<<<<< HEAD

  std::sort(observers.begin(), observers.end(), recordreplay::CompareByPointerId());
  for (ContextLifecycleObserver* observer : observers) {
    // https://linear.app/replay/issue/RUN-806
    recordreplay::Assert("ContextLifecycleNotifier::NotifyContextDestroyed #1 %d",
                         recordreplay::PointerId(observer));

    observer->NotifyContextDestroyed();
  }

#if DCHECK_IS_ON()
  did_notify_observers_ = true;
#endif
||||||| 80c960997e61f

#if DCHECK_IS_ON()
  did_notify_observers_ = true;
#endif
=======
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
}

void ContextLifecycleNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink
