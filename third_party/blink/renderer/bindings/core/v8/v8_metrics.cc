// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"

#include <cstdint>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/bindings/core/v8/local_window_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleDecoded& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleDecoded(ukm->source_id)
      .SetStreamed(event.streamed ? 1 : 0)
      .SetSuccess(event.success ? 1 : 0)
      .SetModuleSize(
          ukm::GetExponentialBucketMinForBytes(event.module_size_in_bytes))
      .SetFunctionCount(event.function_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleCompiled& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleCompiled(ukm->source_id)
      .SetAsync(event.async ? 1 : 0)
      .SetCached(event.cached ? 1 : 0)
      .SetDeserialized(event.deserialized ? 1 : 0)
      .SetLazy(event.lazy ? 1 : 0)
      .SetStreamed(event.streamed ? 1 : 0)
      .SetSuccess(event.success ? 1 : 0)
      .SetCodeSize(
          ukm::GetExponentialBucketMinForBytes(event.code_size_in_bytes))
      .SetLiftoffBailoutCount(event.liftoff_bailout_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::WasmModuleInstantiated& event,
    v8::metrics::Recorder::ContextId context_id) {
  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (!ukm)
    return;
  ukm::builders::V8_Wasm_ModuleInstantiated(ukm->source_id)
      .SetSuccess(event.success ? 1 : 0)
      .SetImportedFunctionCount(event.imported_function_count)
      .SetWallClockDuration(event.wall_clock_duration_in_us)
      .Record(ukm->recorder);
}

namespace {

// Helper function to convert a byte count to a KB count, capping at
// INT_MAX if the number is larger than that.
constexpr int32_t CappedSizeInKB(int64_t size_in_bytes) {
  return base::saturated_cast<int32_t>(size_in_bytes / 1024);
}

// Helper function to convert a B/us count to a KB/ms count, capping at
// INT_MAX if the number is larger than that.
constexpr int32_t CappedEfficacyInKBPerMs(double efficacy_in_bytes_per_us) {
  return base::saturated_cast<int32_t>(efficacy_in_bytes_per_us * 1000 / 1024);
}

// Returns true if |event| contains valid cpp histogram values.
bool CheckCppEvents(const v8::metrics::GarbageCollectionFullCycle& event) {
  if (event.total_cpp.mark_wall_clock_duration_in_us == -1) {
    // If a cpp field in |event| is uninitialized, all cpp fields should be
    // uninitialized.
    DCHECK_EQ(-1, event.total_cpp.mark_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.total_cpp.weak_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.total_cpp.compact_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.total_cpp.sweep_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_cpp.mark_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_cpp.weak_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_cpp.compact_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_cpp.sweep_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_atomic_cpp.mark_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_atomic_cpp.weak_wall_clock_duration_in_us);
    DCHECK_EQ(-1,
              event.main_thread_atomic_cpp.compact_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.main_thread_atomic_cpp.sweep_wall_clock_duration_in_us);
    DCHECK_EQ(-1, event.objects_cpp.bytes_before);
    DCHECK_EQ(-1, event.objects_cpp.bytes_after);
    DCHECK_EQ(-1, event.objects_cpp.bytes_freed);
    DCHECK_EQ(-1, event.memory_cpp.bytes_freed);
    DCHECK_EQ(-1.0, event.efficiency_cpp_in_bytes_per_us);
    DCHECK_EQ(-1.0, event.main_thread_efficiency_cpp_in_bytes_per_us);
    DCHECK_EQ(-1.0, event.collection_rate_cpp_in_percent);
    DCHECK_EQ(-1.0, event.collection_weight_cpp_in_percent);
    DCHECK_EQ(-1.0, event.main_thread_collection_weight_cpp_in_percent);
    return false;
  }
  // Check that all used values have been initialized.
  DCHECK_LE(0, event.total_cpp.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total_cpp.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total_cpp.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total_cpp.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_cpp.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_cpp.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_cpp.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_cpp.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic_cpp.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic_cpp.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic_cpp.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic_cpp.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.objects_cpp.bytes_before);
  DCHECK_LE(0, event.objects_cpp.bytes_after);
  DCHECK_LE(0, event.objects_cpp.bytes_freed);
  DCHECK_LE(0, event.memory_cpp.bytes_freed);
  DCHECK_LE(0, event.efficiency_cpp_in_bytes_per_us);
  DCHECK_LE(0, event.main_thread_efficiency_cpp_in_bytes_per_us);
  DCHECK_LE(0, event.collection_rate_cpp_in_percent);
  DCHECK_LE(0.0, event.collection_weight_cpp_in_percent);
  DCHECK_LE(0.0, event.main_thread_collection_weight_cpp_in_percent);
  return true;
}

void CheckUnifiedEvents(const v8::metrics::GarbageCollectionFullCycle& event) {
  // Check that all used values have been initialized.
  DCHECK_LE(0, event.total.total_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.total.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread.total_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic.total_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic.weak_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_atomic.sweep_wall_clock_duration_in_us);
  // Incremental marking and sweeping may be uninitialized; the other values
  // must be uninitialized.
  DCHECK_EQ(-1, event.main_thread_incremental.total_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.main_thread_incremental.mark_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.incremental_marking_start_stop_wall_clock_duration_in_us);
  DCHECK_EQ(-1, event.main_thread_incremental.weak_wall_clock_duration_in_us);
  DCHECK_EQ(-1,
            event.main_thread_incremental.compact_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.main_thread_incremental.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, event.objects.bytes_before);
  DCHECK_LE(0, event.objects.bytes_after);
  DCHECK_LE(0, event.objects.bytes_freed);
  DCHECK_LE(0, event.memory.bytes_freed);
  DCHECK_LE(0, event.efficiency_in_bytes_per_us);
  DCHECK_LE(0, event.main_thread_efficiency_in_bytes_per_us);
  DCHECK_LE(0, event.collection_rate_in_percent);
  DCHECK_LE(0, event.collection_weight_in_percent);
  DCHECK_LE(0, event.main_thread_collection_weight_in_percent);
}

}  // namespace

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullCycle& event,
    ContextId context_id) {
#define UMA_HISTOGRAM_TIMES_ALL_GC_PHASES(prefix, suffix, statistics)    \
  UMA_HISTOGRAM_TIMES(                                                   \
      prefix suffix,                                                     \
      base::Microseconds(statistics.total_wall_clock_duration_in_us));   \
  UMA_HISTOGRAM_TIMES(                                                   \
      prefix ".Mark" suffix,                                             \
      base::Microseconds(statistics.mark_wall_clock_duration_in_us));    \
  UMA_HISTOGRAM_TIMES(                                                   \
      prefix ".Compact" suffix,                                          \
      base::Microseconds(statistics.compact_wall_clock_duration_in_us)); \
  UMA_HISTOGRAM_TIMES(                                                   \
      prefix ".Sweep" suffix,                                            \
      base::Microseconds(statistics.sweep_wall_clock_duration_in_us));   \
  UMA_HISTOGRAM_TIMES(                                                   \
      prefix ".Weak" suffix,                                             \
      base::Microseconds(statistics.weak_wall_clock_duration_in_us));

  static constexpr size_t kMinSize = 1;
  static constexpr size_t kMaxSize = 4 * 1024 * 1024;
  static constexpr size_t kNumBuckets = 50;

  CheckUnifiedEvents(event);

  DCHECK_LE(0, event.reason);
#define REPORT_V8_HISTOGRAMS(priority)                                         \
  {                                                                            \
    UMA_HISTOGRAM_ENUMERATION("V8.GC.Cycle" priority ".Reason.Full",           \
                              event.reason,                                    \
                              v8::internal::kGarbageCollectionReasonMaxValue); \
                                                                               \
    /* Report throughput metrics: */                                           \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES("V8.GC.Cycle" priority ".Full", "",      \
                                      event.total);                            \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES(                                         \
        "V8.GC.Cycle" priority ".MainThread.Full", "", event.main_thread);     \
                                                                               \
    /* Report atomic pause metrics: */                                         \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES("V8.GC.Cycle" priority                   \
                                      ".MainThread.Full.Atomic",               \
                                      "", event.main_thread_atomic);           \
                                                                               \
    /* Report incremental marking/sweeping metrics: */                         \
    if (event.main_thread_incremental.mark_wall_clock_duration_in_us >= 0) {   \
      UMA_HISTOGRAM_TIMES(                                                     \
          "V8.GC.Cycle" priority ".MainThread.Full.Incremental.Mark",          \
          base::Microseconds(                                                  \
              event.main_thread_incremental.mark_wall_clock_duration_in_us));  \
    }                                                                          \
    if (event.main_thread_incremental.sweep_wall_clock_duration_in_us >= 0) {  \
      UMA_HISTOGRAM_TIMES(                                                     \
          "V8.GC.Cycle" priority ".MainThread.Full.Incremental.Sweep",         \
          base::Microseconds(                                                  \
              event.main_thread_incremental.sweep_wall_clock_duration_in_us)); \
    }                                                                          \
    if (event.incremental_marking_start_stop_wall_clock_duration_in_us >= 0) { \
      UMA_HISTOGRAM_TIMES(                                                     \
          "V8.GC.Cycle" priority                                               \
          ".MainThread.Full.Incremental.Mark.StartStop",                       \
          base::Microseconds(                                                  \
              event                                                            \
                  .incremental_marking_start_stop_wall_clock_duration_in_us)); \
    }                                                                          \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_before_histogram,                    \
        ("V8.GC.Cycle" priority ".Objects.Before.Full", kMinSize, kMaxSize,    \
         kNumBuckets));                                                        \
    object_size_before_histogram.Count(                                        \
        CappedSizeInKB(event.objects.bytes_before));                           \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_after_histogram,                     \
        ("V8.GC.Cycle" priority ".Objects.After.Full", kMinSize, kMaxSize,     \
         kNumBuckets));                                                        \
    object_size_after_histogram.Count(                                         \
        CappedSizeInKB(event.objects.bytes_after));                            \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_freed_histogram,                     \
        ("V8.GC.Cycle" priority ".Objects.Freed.Full", kMinSize, kMaxSize,     \
         kNumBuckets));                                                        \
    object_size_freed_histogram.Count(                                         \
        CappedSizeInKB(event.objects.bytes_freed));                            \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, memory_size_freed_histogram,                     \
        ("V8.GC.Cycle" priority ".Memory.Freed.Full", kMinSize, kMaxSize,      \
         kNumBuckets));                                                        \
    memory_size_freed_histogram.Count(                                         \
        CappedSizeInKB(event.memory.bytes_freed));                             \
                                                                               \
    /* Report efficacy metrics: */                                             \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_histogram,                              \
        ("V8.GC.Cycle" priority ".Efficiency.Full", kMinSize, kMaxSize,        \
         kNumBuckets));                                                        \
    efficacy_histogram.Count(                                                  \
        CappedEfficacyInKBPerMs(event.efficiency_in_bytes_per_us));            \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_main_thread_histogram,                  \
        ("V8.GC.Cycle" priority ".Efficiency.MainThread.Full", kMinSize,       \
         kMaxSize, kNumBuckets));                                              \
    efficacy_main_thread_histogram.Count(CappedEfficacyInKBPerMs(              \
        event.main_thread_efficiency_in_bytes_per_us));                        \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, collection_rate_histogram,                       \
        ("V8.GC.Cycle" priority ".CollectionRate.Full", 1, 100, 20));          \
    collection_rate_histogram.Count(                                           \
        base::saturated_cast<base::Histogram::Sample>(                         \
            100 * event.collection_rate_in_percent));                          \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, collection_weight_histogram,                     \
        ("V8.GC.Cycle" priority ".CollectionWeight.Full", 1, 1000, 20));       \
    collection_weight_histogram.Count(                                         \
        base::saturated_cast<base::Histogram::Sample>(                         \
            1000 * event.collection_weight_in_percent));                       \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, main_thread_collection_weight_histogram,         \
        ("V8.GC.Cycle" priority ".CollectionWeight.MainThread.Full", 1, 1000,  \
         20));                                                                 \
    main_thread_collection_weight_histogram.Count(                             \
        base::saturated_cast<base::Histogram::Sample>(                         \
            1000 * event.main_thread_collection_weight_in_percent));           \
  }

  REPORT_V8_HISTOGRAMS("")
  if (event.priority.has_value()) {
    switch (event.priority.value()) {
      case v8::Isolate::Priority::kBestEffort:
        REPORT_V8_HISTOGRAMS(".BestEffort")
        break;
      case v8::Isolate::Priority::kUserVisible:
        REPORT_V8_HISTOGRAMS(".UserVisible")
        break;
      case v8::Isolate::Priority::kUserBlocking:
        REPORT_V8_HISTOGRAMS(".UserBlocking")
    }
  }
#undef REPORT_V8_HISTOGRAMS

  if (CheckCppEvents(event)) {
#define REPORT_CPP_HISTOGRAMS(priority)                                        \
  {                                                                            \
    /* Report throughput metrics: */                                           \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES("V8.GC.Cycle" priority ".Full", ".Cpp",  \
                                      event.total_cpp);                        \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES("V8.GC.Cycle" priority                   \
                                      ".MainThread.Full",                      \
                                      ".Cpp", event.main_thread_cpp);          \
                                                                               \
    /* Report atomic pause metrics: */                                         \
    UMA_HISTOGRAM_TIMES_ALL_GC_PHASES("V8.GC.Cycle" priority                   \
                                      ".MainThread.Full.Atomic",               \
                                      ".Cpp", event.main_thread_atomic_cpp);   \
                                                                               \
    /* Report incremental marking/sweeping metrics: */                         \
    if (event.main_thread_incremental_cpp.mark_wall_clock_duration_in_us >=    \
        0) {                                                                   \
      UMA_HISTOGRAM_TIMES(                                                     \
          "V8.GC.Cycle" priority ".MainThread.Full.Incremental.Mark.Cpp",      \
          base::Microseconds(event.main_thread_incremental_cpp                 \
                                 .mark_wall_clock_duration_in_us));            \
    }                                                                          \
    if (event.main_thread_incremental_cpp.sweep_wall_clock_duration_in_us >=   \
        0) {                                                                   \
      UMA_HISTOGRAM_TIMES(                                                     \
          "V8.GC.Cycle" priority ".MainThread.Full.Incremental.Sweep.Cpp",     \
          base::Microseconds(event.main_thread_incremental_cpp                 \
                                 .sweep_wall_clock_duration_in_us));           \
    }                                                                          \
                                                                               \
    /* Report size metrics: */                                                 \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_before_cpp_histogram,                \
        ("V8.GC.Cycle" priority ".Objects.Before.Full.Cpp", kMinSize,          \
         kMaxSize, kNumBuckets));                                              \
    object_size_before_cpp_histogram.Count(                                    \
        CappedSizeInKB(event.objects_cpp.bytes_before));                       \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_after_cpp_histogram,                 \
        ("V8.GC.Cycle" priority ".Objects.After.Full.Cpp", kMinSize, kMaxSize, \
         kNumBuckets));                                                        \
    object_size_after_cpp_histogram.Count(                                     \
        CappedSizeInKB(event.objects_cpp.bytes_after));                        \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, object_size_freed_cpp_histogram,                 \
        ("V8.GC.Cycle" priority ".Objects.Freed.Full.Cpp", kMinSize, kMaxSize, \
         kNumBuckets));                                                        \
    object_size_freed_cpp_histogram.Count(                                     \
        CappedSizeInKB(event.objects_cpp.bytes_freed));                        \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, memory_size_freed_cpp_histogram,                 \
        ("V8.GC.Cycle" priority ".Memory.Freed.Full.Cpp", kMinSize, kMaxSize,  \
         kNumBuckets));                                                        \
    memory_size_freed_cpp_histogram.Count(                                     \
        CappedSizeInKB(event.memory_cpp.bytes_freed));                         \
                                                                               \
    /* Report efficacy metrics: */                                             \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_cpp_histogram,                          \
        ("V8.GC.Cycle" priority ".Efficiency.Full.Cpp", kMinSize, kMaxSize,    \
         kNumBuckets));                                                        \
    efficacy_cpp_histogram.Count(                                              \
        CappedEfficacyInKBPerMs(event.efficiency_cpp_in_bytes_per_us));        \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_main_thread_cpp_histogram,              \
        ("V8.GC.Cycle" priority ".Efficiency.MainThread.Full.Cpp", kMinSize,   \
         kMaxSize, kNumBuckets));                                              \
    efficacy_main_thread_cpp_histogram.Count(CappedEfficacyInKBPerMs(          \
        event.main_thread_efficiency_cpp_in_bytes_per_us));                    \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, collection_rate_cpp_histogram,                   \
        ("V8.GC.Cycle" priority ".CollectionRate.Full.Cpp", 1, 100, 20));      \
    collection_rate_cpp_histogram.Count(                                       \
        base::saturated_cast<base::Histogram::Sample>(                         \
            100 * event.collection_rate_cpp_in_percent));                      \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, collection_weight_cpp_histogram,                 \
        ("V8.GC.Cycle" priority ".CollectionWeight.Full.Cpp", 1, 1000, 20));   \
    collection_weight_cpp_histogram.Count(                                     \
        base::saturated_cast<base::Histogram::Sample>(                         \
            1000 * event.collection_weight_cpp_in_percent));                   \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, main_thread_collection_weight_cpp_histogram,     \
        ("V8.GC.Cycle" priority ".CollectionWeight.MainThread.Full.Cpp", 1,    \
         1000, 20));                                                           \
    main_thread_collection_weight_cpp_histogram.Count(                         \
        base::saturated_cast<base::Histogram::Sample>(                         \
            1000 * event.main_thread_collection_weight_cpp_in_percent));       \
  }

    REPORT_CPP_HISTOGRAMS("")
    if (event.priority.has_value()) {
      switch (event.priority.value()) {
        case v8::Isolate::Priority::kBestEffort:
          REPORT_CPP_HISTOGRAMS(".BestEffort")
          break;
        case v8::Isolate::Priority::kUserVisible:
          REPORT_CPP_HISTOGRAMS(".UserVisible")
          break;
        case v8::Isolate::Priority::kUserBlocking:
          REPORT_CPP_HISTOGRAMS(".UserBlocking")
      }
    }
#undef REPORT_CPP_HISTOGRAMS
  }

#undef UMA_HISTOGRAM_TIMES_ALL_GC_PHASES
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullMainThreadIncrementalMark& event,
    ContextId context_id) {
  if (event.wall_clock_duration_in_us >= 0) {
    UMA_HISTOGRAM_TIMES("V8.GC.Event.MainThread.Full.Incremental.Mark",
                        base::Microseconds(event.wall_clock_duration_in_us));
  }
  if (event.cpp_wall_clock_duration_in_us >= 0) {
    // This is only a latency event.
    UMA_HISTOGRAM_TIMES(
        "V8.GC.Event.MainThread.Full.Incremental.Mark.Cpp",
        base::Microseconds(event.cpp_wall_clock_duration_in_us));
  }
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullMainThreadIncrementalSweep& event,
    ContextId context_id) {
  if (event.wall_clock_duration_in_us >= 0) {
    UMA_HISTOGRAM_TIMES("V8.GC.Event.MainThread.Full.Incremental.Sweep",
                        base::Microseconds(event.wall_clock_duration_in_us));
  }
  if (event.cpp_wall_clock_duration_in_us >= 0) {
    // This is only a latency event.
    UMA_HISTOGRAM_TIMES(
        "V8.GC.Event.MainThread.Full.Incremental.Sweep.Cpp",
        base::Microseconds(event.cpp_wall_clock_duration_in_us));
  }
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalMark&
        event,
    ContextId context_id) {
  AddMainThreadBatchedEvents(event, context_id);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullMainThreadBatchedIncrementalSweep&
        event,
    ContextId context_id) {
  AddMainThreadBatchedEvents(event, context_id);
}

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionYoungCycle& event,
    ContextId context_id) {
  // Check that all used values have been initialized.
  DCHECK_LE(0, event.reason);
  DCHECK_LE(0, event.total_wall_clock_duration_in_us);
  DCHECK_LE(0, event.main_thread_wall_clock_duration_in_us);
  DCHECK_LE(0, event.collection_rate_in_percent);
  DCHECK_LE(0, event.efficiency_in_bytes_per_us);
  DCHECK_LE(0, event.main_thread_efficiency_in_bytes_per_us);

  static constexpr size_t kMinSize = 1;
  static constexpr size_t kMaxSize = 4 * 1024 * 1024;
  static constexpr size_t kNumBuckets = 50;

#define REPORT_V8_HISTOGRAMS(priority)                                         \
  {                                                                            \
    UMA_HISTOGRAM_ENUMERATION("V8.GC.Cycle" priority ".Reason.Young",          \
                              event.reason,                                    \
                              v8::internal::kGarbageCollectionReasonMaxValue); \
                                                                               \
    UMA_HISTOGRAM_TIMES(                                                       \
        "V8.GC.Cycle" priority ".Young",                                       \
        base::Microseconds(event.total_wall_clock_duration_in_us));            \
    UMA_HISTOGRAM_TIMES(                                                       \
        "V8.GC.Cycle" priority ".MainThread.Young",                            \
        base::Microseconds(event.main_thread_wall_clock_duration_in_us));      \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_histogram,                              \
        ("V8.GC.Cycle" priority ".Efficiency.Young", kMinSize, kMaxSize,       \
         kNumBuckets));                                                        \
    efficacy_histogram.Count(                                                  \
        CappedEfficacyInKBPerMs(event.efficiency_in_bytes_per_us));            \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, efficacy_main_thread_histogram,                  \
        ("V8.GC.Cycle" priority ".Efficiency.MainThread.Young", kMinSize,      \
         kMaxSize, kNumBuckets));                                              \
    efficacy_main_thread_histogram.Count(CappedEfficacyInKBPerMs(              \
        event.main_thread_efficiency_in_bytes_per_us));                        \
                                                                               \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                           \
        CustomCountHistogram, collection_rate_histogram,                       \
        ("V8.GC.Cycle" priority ".CollectionRate.Young", 1, 100, 20));         \
    collection_rate_histogram.Count(                                           \
        base::saturated_cast<base::Histogram::Sample>(                         \
            100 * event.collection_rate_in_percent));                          \
  }

  REPORT_V8_HISTOGRAMS("")
  if (event.priority.has_value()) {
    switch (event.priority.value()) {
      case v8::Isolate::Priority::kBestEffort:
        REPORT_V8_HISTOGRAMS(".BestEffort")
        break;
      case v8::Isolate::Priority::kUserVisible:
        REPORT_V8_HISTOGRAMS(".UserVisible")
        break;
      case v8::Isolate::Priority::kUserBlocking:
        REPORT_V8_HISTOGRAMS(".UserBlocking")
    }
  }
#undef REPORT_V8_HISTOGRAMS
}

void V8MetricsRecorder::NotifyIsolateDisposal() {
  v8::metrics::Recorder::NotifyIsolateDisposal();
  isolate_ = nullptr;
}

std::optional<V8MetricsRecorder::UkmRecorderAndSourceId>
V8MetricsRecorder::GetUkmRecorderAndSourceId(
    v8::metrics::Recorder::ContextId context_id) {
  if (!isolate_)
    return std::optional<UkmRecorderAndSourceId>();
  v8::HandleScope handle_scope(isolate_);
  v8::MaybeLocal<v8::Context> maybe_context =
      v8::metrics::Recorder::GetContext(isolate_, context_id);
  if (maybe_context.IsEmpty())
    return std::optional<UkmRecorderAndSourceId>();
  ExecutionContext* context =
      ExecutionContext::From(maybe_context.ToLocalChecked());
  if (!context)
    return std::optional<UkmRecorderAndSourceId>();
  ukm::UkmRecorder* ukm_recorder = context->UkmRecorder();
  if (!ukm_recorder)
    return std::optional<UkmRecorderAndSourceId>();
  return std::optional<UkmRecorderAndSourceId>(std::in_place, ukm_recorder,
                                               context->UkmSourceID());
}

}  // namespace blink
