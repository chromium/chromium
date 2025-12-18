// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

#include "base/trace_event/trace_event.h"

namespace blink {
namespace trace_event {

void EnableTracing(const String& category_filter) {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(category_filter.Utf8(), ""));
}

void DisableTracing() {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

void AddTraceSessionObserver(TraceSessionObserver* observer) {
  base::trace_event::TraceSessionObserverList::AddObserver(observer);
}

void RemoveTraceSessionObserver(TraceSessionObserver* observer) {
  base::trace_event::TraceSessionObserverList::RemoveObserver(observer);
}

}  // namespace trace_event
}  // namespace blink
