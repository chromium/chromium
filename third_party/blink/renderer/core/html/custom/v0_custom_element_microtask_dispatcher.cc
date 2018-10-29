// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_dispatcher.h"

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_callback_queue.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_import_step.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_scheduler.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

static const V0CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

V0CustomElementMicrotaskDispatcher::V0CustomElementMicrotaskDispatcher()
    : has_scheduled_microtask_(false), phase_(kQuiescent) {}

V0CustomElementMicrotaskDispatcher&
V0CustomElementMicrotaskDispatcher::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<V0CustomElementMicrotaskDispatcher>, instance,
                      (new V0CustomElementMicrotaskDispatcher));
  return *instance;
}

void V0CustomElementMicrotaskDispatcher::Enqueue(
    V0CustomElementCallbackQueue* queue) {
  EnsureMicrotaskScheduledForElementQueue();
  queue->SetOwner(kMicrotaskQueueId);
  elements_.push_back(queue);
}

void V0CustomElementMicrotaskDispatcher::
    EnsureMicrotaskScheduledForElementQueue() {
  DCHECK(phase_ == kQuiescent || phase_ == kResolving);
  EnsureMicrotaskScheduled();
}

void V0CustomElementMicrotaskDispatcher::EnsureMicrotaskScheduled() {
  if (!has_scheduled_microtask_) {
    Microtask::EnqueueMicrotask(WTF::Bind(&Dispatch));
    has_scheduled_microtask_ = true;
  }
}

void V0CustomElementMicrotaskDispatcher::Dispatch() {
  Instance().DoDispatch();
}

void V0CustomElementMicrotaskDispatcher::DoDispatch() {
  DCHECK(IsMainThread());

  DCHECK(phase_ == kQuiescent);
  DCHECK(has_scheduled_microtask_);
  has_scheduled_microtask_ = false;

  // Finishing microtask work deletes all
  // V0CustomElementCallbackQueues. Being in a callback delivery scope
  // implies those queues could still be in use.
  SECURITY_DCHECK(!V0CustomElementProcessingStack::InCallbackDeliveryScope());

  phase_ = kResolving;

  phase_ = kDispatchingCallbacks;
  for (const auto& element : elements_) {
    // Created callback may enqueue an attached callback.
    V0CustomElementProcessingStack::CallbackDeliveryScope scope;
    element->ProcessInElementQueue(kMicrotaskQueueId);
  }

  elements_.clear();
  V0CustomElementScheduler::MicrotaskDispatcherDidFinish();
  phase_ = kQuiescent;
}

void V0CustomElementMicrotaskDispatcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(elements_);
}

}  // namespace blink
