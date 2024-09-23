// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_resource_tracker.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "v8/include/v8.h"

namespace blink {

CanvasResourceTracker* CanvasResourceTracker::For(v8::Isolate* isolate) {
  auto* isolate_data = V8PerIsolateData::From(isolate);
  auto* canvas_resource_tracker = static_cast<CanvasResourceTracker*>(
      isolate_data->GetUserData(UserData::Key::kCanvasResourceTracker));
  if (!canvas_resource_tracker) {
    canvas_resource_tracker = MakeGarbageCollected<CanvasResourceTracker>();
    isolate_data->SetUserData(UserData::Key::kCanvasResourceTracker,
                              canvas_resource_tracker);
  }
  return canvas_resource_tracker;
}

void CanvasResourceTracker::Add(CanvasRenderingContextHost* resource,
                                ExecutionContext* execution_context) {
  resource_map_.insert(resource, execution_context);
}

const CanvasResourceTracker::ResourceMap&
CanvasResourceTracker::GetResourceMap() const {
  return resource_map_;
}

void CanvasResourceTracker::Trace(Visitor* visitor) const {
  V8PerIsolateData::UserData::Trace(visitor);
  visitor->Trace(resource_map_);
}

}  // namespace blink
