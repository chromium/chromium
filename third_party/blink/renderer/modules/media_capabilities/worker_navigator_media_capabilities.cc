// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/worker_navigator_media_capabilities.h"

#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

// static
const char WorkerNavigatorMediaCapabilities::kSupplementName[] =
    "WorkerNavigatorMediaCapabilities";

MediaCapabilities* WorkerNavigatorMediaCapabilities::mediaCapabilities(
    WorkerNavigator& navigator) {
  WorkerNavigatorMediaCapabilities& self =
      WorkerNavigatorMediaCapabilities::From(navigator);
  if (!self.capabilities_)
    self.capabilities_ = MakeGarbageCollected<MediaCapabilities>();
  return self.capabilities_.Get();
}

void WorkerNavigatorMediaCapabilities::Trace(blink::Visitor* visitor) {
  visitor->Trace(capabilities_);
  Supplement<WorkerNavigator>::Trace(visitor);
}

WorkerNavigatorMediaCapabilities::WorkerNavigatorMediaCapabilities(
    WorkerNavigator& navigator)
    : Supplement<WorkerNavigator>(navigator) {}

WorkerNavigatorMediaCapabilities& WorkerNavigatorMediaCapabilities::From(
    WorkerNavigator& navigator) {
  WorkerNavigatorMediaCapabilities* supplement =
      Supplement<WorkerNavigator>::From<WorkerNavigatorMediaCapabilities>(
          navigator);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<WorkerNavigatorMediaCapabilities>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

}  // namespace blink
