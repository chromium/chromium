// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_INITIATOR_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_INITIATOR_HELPER_H_

#include "third_party/blink/renderer/core/timing/resource_timing_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8-isolate.h"

namespace blink {

class ResourceInitiatorHelper final {
  STATIC_ONLY(ResourceInitiatorHelper);

 public:
  static v8::Isolate* GetIsolateIfRunningScriptOnMainThread() {
    CHECK(IsMainThread());
    v8::Isolate* isolate = v8::Isolate::TryGetCurrent();
    // We are assuming that |isolate->InContext()| indicates that JavaScript
    // is running, though this isn't guaranteed. There is no v8 API that is
    // truly definitive for this. This matches existing |InspectorNetworkAgent|
    // usage.
    if (!isolate || !isolate->InContext()) {
      return nullptr;
    }
    return isolate;
  }

  // When the running JavaScript code is not task-attributable, a nullptr
  // is returned.
  static ResourceTimingContext* GetResourceTimingContext(v8::Isolate& isolate) {
    auto* tracker = scheduler::TaskAttributionTracker::From(&isolate);
    scheduler::TaskAttributionInfo* task_state =
        tracker ? tracker->CurrentTaskState() : nullptr;
    return task_state ? task_state->GetResourceTimingContext() : nullptr;
  }

  // When the running JavaScript code is not task-attributable, an empty URL
  // is returned.
  static KURL GetScriptInitiatorUrl(v8::Isolate& isolate) {
    ResourceTimingContext* resource_timing_context =
        GetResourceTimingContext(isolate);
    return resource_timing_context ? resource_timing_context->InitiatorUrl()
                                   : KURL();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_INITIATOR_HELPER_H_
