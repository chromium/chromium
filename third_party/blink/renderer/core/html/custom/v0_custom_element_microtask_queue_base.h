// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_QUEUE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_QUEUE_BASE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_step.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class V0CustomElementMicrotaskQueueBase
    : public GarbageCollected<V0CustomElementMicrotaskQueueBase> {
 public:
  virtual ~V0CustomElementMicrotaskQueueBase() = default;

  bool IsEmpty() const { return queue_.IsEmpty(); }
  void Dispatch();

  void Trace(Visitor*);

#if !defined(NDEBUG)
  void Show(unsigned indent);
#endif

 protected:
  V0CustomElementMicrotaskQueueBase() : in_dispatch_(false) {}
  virtual void DoDispatch() = 0;

  HeapVector<Member<V0CustomElementMicrotaskStep>> queue_;
  bool in_dispatch_;

  DISALLOW_COPY_AND_ASSIGN(V0CustomElementMicrotaskQueueBase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_QUEUE_BASE_H_
