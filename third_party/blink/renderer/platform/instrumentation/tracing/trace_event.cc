// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

#include "base/trace_event/trace_event.h"

namespace blink {
namespace trace_event {

void EnableTracing(const String& category_filter) {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(category_filter.Utf8(), ""),
      base::trace_event::TraceLog::RECORDING_MODE);
}

void DisableTracing() {
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
}

void AddAsyncEnabledStateObserver(
    base::WeakPtr<AsyncEnabledStateObserver> observer) {
  base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
      observer);
}

void RemoveAsyncEnabledStateObserver(AsyncEnabledStateObserver* observer) {
  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      observer);
}

void AddEnabledStateObserver(EnabledStateObserver* observer) {
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(observer);
}

void RemoveEnabledStateObserver(EnabledStateObserver* observer) {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(
      observer);
}

}  // namespace trace_event
}  // namespace blink
