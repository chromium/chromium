// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_TRACER_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_TRACER_H_

#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

class CONTROLLER_EXPORT MemoryTracer : public MemoryUsageMonitor::Observer {
  USING_FAST_MALLOC(MemoryTracer);

 public:
  // Initializes the shared instance. Has no effect if called more than once.
  static void Initialize();

 private:
  MemoryTracer();

  // MemoryUsageMonitor::Observer:
  void OnMemoryPing(MemoryUsage) override;

  perfetto::CounterTrack v8_track_{"v8 bytes"};
  perfetto::CounterTrack blink_track_{"Blink bytes"};
  perfetto::CounterTrack pmf_track_{"PrivateMemoryFootprint"};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_TRACER_H_
