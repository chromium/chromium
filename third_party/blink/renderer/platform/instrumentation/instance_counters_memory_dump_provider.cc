// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/instance_counters_memory_dump_provider.h"

#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

InstanceCountersMemoryDumpProvider*
InstanceCountersMemoryDumpProvider::Instance() {
  DEFINE_STATIC_LOCAL(InstanceCountersMemoryDumpProvider, instance, ());
  return &instance;
}

bool InstanceCountersMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* memory_dump) {
  using base::trace_event::MemoryAllocatorDump;
#define DUMP_COUNTER(CounterType)                                     \
  memory_dump->CreateAllocatorDump("blink_objects/" #CounterType)     \
      ->AddScalar("object_count", MemoryAllocatorDump::kUnitsObjects, \
                  InstanceCounters::CounterValue(                     \
                      InstanceCounters::k##CounterType##Counter));
  INSTANCE_COUNTERS_LIST(DUMP_COUNTER)
  return true;
}

}  // namespace blink
