/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"

#include "base/process/process_metrics.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

#if BUILDFLAG(IS_ANDROID)
size_t GetMemoryUsage(v8::Isolate* isolate) {
  size_t usage =
      base::ProcessMetrics::CreateCurrentProcessMetrics()->GetMallocUsage() +
      WTF::Partitions::TotalActiveBytes() +
      ProcessHeap::TotalAllocatedObjectSize();
  v8::HeapStatistics v8_heap_statistics;
  isolate->GetHeapStatistics(&v8_heap_statistics);
  usage += v8_heap_statistics.total_heap_size();
  return usage;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

// static
V8GCForContextDispose& V8GCForContextDispose::Instance() {
  DEFINE_STATIC_LOCAL(V8GCForContextDispose, static_instance, ());
  return static_instance;
}

void V8GCForContextDispose::NotifyContextDisposed(
    v8::Isolate* isolate,
    bool is_main_frame,
    WindowProxy::FrameReuseStatus frame_reuse_status) {
#if BUILDFLAG(IS_ANDROID)
  // When a low end device is in a low memory situation we should prioritize
  // memory use and trigger a V8+Blink GC. However, on Android, if the frame
  // will not be reused, the process will likely to be killed soon so skip this.
  if (is_main_frame && frame_reuse_status == WindowProxy::kFrameWillBeReused &&
      ((MemoryPressureListenerRegistry::
            IsLowEndDeviceOrPartialLowEndModeEnabled() &&
        MemoryPressureListenerRegistry::IsCurrentlyLowMemory()) ||
       force_page_navigation_gc_)) {
    const size_t pre_gc_memory_usage = GetMemoryUsage(isolate);
    isolate->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical);
    const size_t post_gc_memory_usage = GetMemoryUsage(isolate);
    const int reduction = static_cast<int>(pre_gc_memory_usage) -
                          static_cast<int>(post_gc_memory_usage);
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, reduction_histogram,
        ("BlinkGC.LowMemoryPageNavigationGC.Reduction", 1, 512, 50));
    reduction_histogram.Count(reduction / 1024 / 1024);

    force_page_navigation_gc_ = false;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  isolate->ContextDisposedNotification(!is_main_frame);
}

void V8GCForContextDispose::SetForcePageNavigationGC() {
  force_page_navigation_gc_ = true;
}

}  // namespace blink
