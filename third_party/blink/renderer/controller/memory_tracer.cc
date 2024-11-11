// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_tracer.h"

#include "base/trace_event/base_tracing.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

void MemoryTracer::Initialize() {
  DEFINE_STATIC_LOCAL(MemoryTracer, provider, {});
  (void)provider;
}

MemoryTracer::MemoryTracer() {
  auto& monitor = MemoryUsageMonitor::Instance();
  monitor.AddObserver(this);
}

void MemoryTracer::OnMemoryPing(MemoryUsage usage) {
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"), v8_track_,
                usage.v8_bytes);
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"), blink_track_,
                usage.blink_gc_bytes);
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"), pmf_track_,
                usage.private_footprint_bytes);
}

}  // namespace blink
