// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/modules/native_file_system/native_file_system_handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const char DOMWindowLaunchQueue::kSupplementName[] = "DOMWindowLaunchQueue";

DOMWindowLaunchQueue::DOMWindowLaunchQueue()
    : launch_queue_(MakeGarbageCollected<LaunchQueue>()) {}
DOMWindowLaunchQueue::~DOMWindowLaunchQueue() = default;

Member<LaunchQueue> DOMWindowLaunchQueue::launchQueue(LocalDOMWindow& window) {
  return FromState(&window)->launch_queue_;
}

void DOMWindowLaunchQueue::UpdateLaunchFiles(
    LocalDOMWindow* window,
    HeapVector<Member<NativeFileSystemHandle>> files) {
  FromState(window)->launch_queue_->Enqueue(
      MakeGarbageCollected<LaunchParams>(std::move(files)));
}

void DOMWindowLaunchQueue::Trace(blink::Visitor* visitor) {
  visitor->Trace(launch_queue_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowLaunchQueue* DOMWindowLaunchQueue::FromState(LocalDOMWindow* window) {
  DOMWindowLaunchQueue* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowLaunchQueue>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowLaunchQueue>();
    ProvideTo(*window, supplement);
  }
  return supplement;
}

}  // namespace blink
