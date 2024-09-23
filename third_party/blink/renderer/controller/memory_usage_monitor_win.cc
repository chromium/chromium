// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_win.h"

#include <tchar.h>
#include <windows.h>

#include <psapi.h>

#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

static MemoryUsageMonitor* g_instance_for_testing = nullptr;

}  // namespace

// static
MemoryUsageMonitor& MemoryUsageMonitor::Instance() {
  DEFINE_STATIC_LOCAL(MemoryUsageMonitorWin, monitor, ());
  return g_instance_for_testing ? *g_instance_for_testing : monitor;
}

// static
void MemoryUsageMonitor::SetInstanceForTesting(MemoryUsageMonitor* instance) {
  g_instance_for_testing = instance;
}

// CalculateProcessMemoryFootprint is generated from:
// - CalculatePrivateFootprintKb defined in
//   //services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.cc
// - OSMetrics::FillOSMemoryDump defined in
//   //services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics_win.cc
bool MemoryUsageMonitorWin::CalculateProcessMemoryFootprint(
    uint64_t* private_footprint) {
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (!::GetProcessMemoryInfo(::GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                              sizeof(pmc)))
    return false;
  *private_footprint = pmc.PrivateUsage;
  return true;
}

void MemoryUsageMonitorWin::GetProcessMemoryUsage(MemoryUsage& usage) {
  uint64_t private_footprint;
  if (CalculateProcessMemoryFootprint(&private_footprint))
    usage.private_footprint_bytes = static_cast<double>(private_footprint);
}

}  // namespace blink
