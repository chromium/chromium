// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

#include "base/record_replay.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace blink {

ContextLifecycleNotifier::ContextLifecycleNotifier() {
  // https://linear.app/replay/issue/RUN-806
  recordreplay::RegisterPointer("ContextLifecycleNotifier", this);
}

ContextLifecycleNotifier::~ContextLifecycleNotifier() {
  // https://linear.app/replay/issue/RUN-806
  recordreplay::UnregisterPointer(this);

#if DCHECK_IS_ON()
  // `NotifyContextDestroyed()` must be called prior to destruction.
  DCHECK(did_notify_observers_);
#endif
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
  // https://linear.app/replay/issue/RUN-806
  recordreplay::Assert("ContextLifecycleNotifier::NotifyContextDestroyed %d",
                       recordreplay::PointerId(this));

  // Manually ensure we notify observers in a consistent order when recording
  // vs. replaying. It would be better to ensure the observers_ set is iterated
  // deterministically, but this is easier for now.
  std::vector<ContextLifecycleObserver*> observers;
  observers_.ForEachObserver([&](ContextLifecycleObserver* observer) {
    observers.push_back(observer);
  });
  observers_.Clear();

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
}

void ContextLifecycleNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink
