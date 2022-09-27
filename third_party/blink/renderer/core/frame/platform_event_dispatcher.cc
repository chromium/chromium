// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/platform_event_dispatcher.h"

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/frame/platform_event_controller.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

PlatformEventDispatcher::PlatformEventDispatcher()
    : is_dispatching_(false), is_listening_(false) {}

void PlatformEventDispatcher::AddController(PlatformEventController* controller,
                                            LocalDOMWindow* window) {
  DCHECK(controller);
  // TODO: If we can avoid to register a same controller twice, we can change
  // this 'if' to ASSERT.
  if (controllers_.Contains(controller))
    return;

  controllers_.insert(controller);

  if (!is_listening_) {
    StartListening(window);
    is_listening_ = true;
  }
}

void PlatformEventDispatcher::RemoveController(
    PlatformEventController* controller) {
  DCHECK(controllers_.Contains(controller));

  controllers_.erase(controller);
  if (!is_dispatching_ && controllers_.empty()) {
    StopListening();
    is_listening_ = false;
  }
}

void PlatformEventDispatcher::NotifyControllers() {
  if (controllers_.empty())
    return;

  {
    base::AutoReset<bool> change_is_dispatching(&is_dispatching_, true);
    // HashSet |controllers_| can be updated during an iteration, and it stops
    // the iteration.  Thus we store it into a Vector to access all elements.
    HeapVector<Member<PlatformEventController>> snapshot_vector(controllers_);
    for (PlatformEventController* controller : snapshot_vector) {
      if (controllers_.Contains(controller))
        controller->DidUpdateData();
    }
  }

  if (controllers_.empty()) {
    StopListening();
    is_listening_ = false;
  }
}

void PlatformEventDispatcher::Trace(Visitor* visitor) const {
  visitor->Trace(controllers_);
}

}  // namespace blink
