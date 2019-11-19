// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_RUN_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_RUN_QUEUE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue;
class V0CustomElementAsyncImportMicrotaskQueue;
class V0CustomElementMicrotaskStep;
class HTMLImportLoader;

class V0CustomElementMicrotaskRunQueue
    : public GarbageCollected<V0CustomElementMicrotaskRunQueue> {
 public:
  V0CustomElementMicrotaskRunQueue();

  void Enqueue(HTMLImportLoader* parent_loader,
               V0CustomElementMicrotaskStep*,
               bool import_is_sync);
  void RequestDispatchIfNeeded();
  bool IsEmpty() const;

  void Trace(Visitor*);

 private:
  void Dispatch();

  Member<V0CustomElementSyncMicrotaskQueue> sync_queue_;
  Member<V0CustomElementAsyncImportMicrotaskQueue> async_queue_;
  bool dispatch_is_pending_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_RUN_QUEUE_H_
