// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_SYNC_MICROTASK_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_SYNC_MICROTASK_QUEUE_H_

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_queue_base.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue
    : public V0CustomElementMicrotaskQueueBase {
 public:
  V0CustomElementSyncMicrotaskQueue() = default;

  void Enqueue(V0CustomElementMicrotaskStep*);

 private:
  void DoDispatch() override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_SYNC_MICROTASK_QUEUE_H_
