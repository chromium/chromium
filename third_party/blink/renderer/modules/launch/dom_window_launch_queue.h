// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_QUEUE_H_

#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/modules/launch/launch_queue.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocalDOMWindow;

class DOMWindowLaunchQueue final
    : public GarbageCollected<DOMWindowLaunchQueue>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit DOMWindowLaunchQueue();

  // IDL Interface.
  static Member<LaunchQueue> launchQueue(LocalDOMWindow&);

  static void UpdateLaunchFiles(LocalDOMWindow*,
                                HeapVector<Member<FileSystemHandle>>);

  void Trace(Visitor*) const override;

 private:
  static DOMWindowLaunchQueue* FromState(LocalDOMWindow* window);

  Member<LaunchQueue> launch_queue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_LAUNCH_DOM_WINDOW_LAUNCH_QUEUE_H_
