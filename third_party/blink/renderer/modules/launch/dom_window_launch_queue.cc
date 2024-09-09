// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_handle.h"
#include "third_party/blink/renderer/modules/launch/launch_params.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const char DOMWindowLaunchQueue::kSupplementName[] = "DOMWindowLaunchQueue";

DOMWindowLaunchQueue::DOMWindowLaunchQueue(LocalDOMWindow& window)
    : Supplement(window), launch_queue_(MakeGarbageCollected<LaunchQueue>()) {}

LaunchQueue* DOMWindowLaunchQueue::launchQueue(LocalDOMWindow& window) {
  return FromState(&window)->launch_queue_.Get();
}

void DOMWindowLaunchQueue::UpdateLaunchFiles(
    LocalDOMWindow* window,
    HeapVector<Member<FileSystemHandle>> files) {
  FromState(window)->launch_queue_->Enqueue(
      MakeGarbageCollected<LaunchParams>(std::move(files)));
}

void DOMWindowLaunchQueue::EnqueueLaunchParams(LocalDOMWindow* window,
                                               const KURL& launch_url) {
  FromState(window)->launch_queue_->Enqueue(
      MakeGarbageCollected<LaunchParams>(launch_url));
}

void DOMWindowLaunchQueue::Trace(Visitor* visitor) const {
  visitor->Trace(launch_queue_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
DOMWindowLaunchQueue* DOMWindowLaunchQueue::FromState(LocalDOMWindow* window) {
  DOMWindowLaunchQueue* supplement =
      Supplement<LocalDOMWindow>::From<DOMWindowLaunchQueue>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<DOMWindowLaunchQueue>(*window);
    ProvideTo(*window, supplement);
  }
  return supplement;
}

}  // namespace blink
