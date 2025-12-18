// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACE_EVENT_H_

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/trace_session_observer.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace trace_event {

using base::trace_event::TraceScopedTrackableObject;

inline base::TimeTicks ToTraceTimestamp(double seconds) {
  return base::TimeTicks() + base::Seconds(seconds);
}

// This is to avoid error of passing a chromium time internal value.
void ToTraceTimestamp(int64_t);

PLATFORM_EXPORT void EnableTracing(const String& category_filter);
PLATFORM_EXPORT void DisableTracing();

using TraceSessionObserver = base::trace_event::TraceSessionObserver;
PLATFORM_EXPORT void AddTraceSessionObserver(TraceSessionObserver*);
PLATFORM_EXPORT void RemoveTraceSessionObserver(TraceSessionObserver*);

}  // namespace trace_event
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_TRACING_TRACE_EVENT_H_
