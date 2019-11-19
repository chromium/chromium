// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_run_queue.h"

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_async_import_microtask_queue.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_sync_microtask_queue.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

V0CustomElementMicrotaskRunQueue::V0CustomElementMicrotaskRunQueue()
    : sync_queue_(MakeGarbageCollected<V0CustomElementSyncMicrotaskQueue>()),
      async_queue_(
          MakeGarbageCollected<V0CustomElementAsyncImportMicrotaskQueue>()),
      dispatch_is_pending_(false) {}

void V0CustomElementMicrotaskRunQueue::Enqueue(
    HTMLImportLoader* parent_loader,
    V0CustomElementMicrotaskStep* step,
    bool import_is_sync) {
  if (import_is_sync) {
    if (parent_loader)
      parent_loader->MicrotaskQueue()->Enqueue(step);
    else
      sync_queue_->Enqueue(step);
  } else {
    async_queue_->Enqueue(step);
  }

  RequestDispatchIfNeeded();
}

void V0CustomElementMicrotaskRunQueue::RequestDispatchIfNeeded() {
  if (dispatch_is_pending_ || IsEmpty())
    return;
  Microtask::EnqueueMicrotask(WTF::Bind(
      &V0CustomElementMicrotaskRunQueue::Dispatch, WrapWeakPersistent(this)));
  dispatch_is_pending_ = true;
}

void V0CustomElementMicrotaskRunQueue::Trace(Visitor* visitor) {
  visitor->Trace(sync_queue_);
  visitor->Trace(async_queue_);
}

void V0CustomElementMicrotaskRunQueue::Dispatch() {
  dispatch_is_pending_ = false;
  sync_queue_->Dispatch();
  if (sync_queue_->IsEmpty())
    async_queue_->Dispatch();
}

bool V0CustomElementMicrotaskRunQueue::IsEmpty() const {
  return sync_queue_->IsEmpty() && async_queue_->IsEmpty();
}

}  // namespace blink
