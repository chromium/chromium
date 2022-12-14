// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

#include "base/record_replay.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace blink {

ContextLifecycleNotifier::~ContextLifecycleNotifier() {
#if DCHECK_IS_ON()
  // `NotifyContextDestroyed()` must be called prior to destruction.
  DCHECK(did_notify_observers_);
#endif
}

void ContextLifecycleNotifier::AddContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  observers_.AddObserver(observer);

  if (recordreplay::IsRecordingOrReplaying("values") && recordreplay::IsReplaying())
    replay_observers_.push_back(observer);
}

void ContextLifecycleNotifier::RemoveContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);

  for (size_t i = 0; i < replay_observers_.size(); i++) {
    if (replay_observers_[i] == observer) {
      replay_observers_.EraseAt(i);
      break;
    }
  }
}

void ContextLifecycleNotifier::NotifyContextDestroyed() {
  HeapVector<Member<ContextLifecycleObserver>> observers;
  observers_.ForEachObserver([&](ContextLifecycleObserver* observer) {
    observers.push_back(observer);
  });
  observers_.Clear();

  std::sort(observers.begin(), observers.end(),
            recordreplay::CompareMemberByPointerId<Member<ContextLifecycleObserver>>());

  if (recordreplay::IsRecordingOrReplaying("values") &&
      !recordreplay::AreEventsDisallowed()) {
    size_t num_observers = recordreplay::RecordReplayValue("NotifyContextDestroyed NumObservers", observers.size());
    int* observer_ids = new int[num_observers];

    if (recordreplay::IsRecording()) {
      for (size_t i = 0; i < observers.size(); i++) {
        int id = recordreplay::PointerId(observers[i]);
        CHECK(id);
        observer_ids[i] = id;
      }
    }

    recordreplay::RecordReplayBytes("ContextLifecycleNotifier::NotifyContextDestroyed ObserverIds",
                                    observer_ids, num_observers * sizeof(int));

    if (recordreplay::IsReplaying()) {
      HeapVector<Member<ContextLifecycleObserver>> new_observers;
      for (ContextLifecycleObserver* observer : observers) {
        int id = recordreplay::PointerId(observer);
        CHECK(id);
        bool found = false;
        for (size_t i = 0; i < num_observers; i++) {
          if (observer_ids[i] == id) {
            found = true;
            break;
          }
        }
        if (found)
          new_observers.push_back(observer);
      }

      observers = std::move(new_observers);
    }

    delete[] observer_ids;
  }

  for (ContextLifecycleObserver* observer : observers) {
    if (!recordreplay::AreEventsDisallowed()) {
      recordreplay::Assert("ContextLifecycleNotifier::NotifyContextDestroyed #1 %d",
                           recordreplay::PointerId(observer));
    }
    observer->NotifyContextDestroyed();
  }

  replay_observers_.clear();

#if DCHECK_IS_ON()
  did_notify_observers_ = true;
#endif
}

void ContextLifecycleNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
  visitor->Trace(replay_observers_);
}

}  // namespace blink
