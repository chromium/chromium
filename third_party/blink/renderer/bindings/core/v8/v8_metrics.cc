// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"

#include <cstdint>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
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

void CheckGarbageCollectionPhasesInitialized(
    const v8::metrics::GarbageCollectionPhases& phases) {
  DCHECK_LE(0, phases.total_wall_clock_duration_in_us);
  DCHECK_LE(0, phases.compact_wall_clock_duration_in_us);
  DCHECK_LE(0, phases.mark_wall_clock_duration_in_us);
  DCHECK_LE(0, phases.sweep_wall_clock_duration_in_us);
  DCHECK_LE(0, phases.weak_wall_clock_duration_in_us);
}

void CheckGarbageCollectionPhasesUninitialized(
    const v8::metrics::GarbageCollectionPhases& phases) {
  DCHECK_EQ(-1, phases.total_wall_clock_duration_in_us);
  DCHECK_EQ(-1, phases.compact_wall_clock_duration_in_us);
  DCHECK_EQ(-1, phases.mark_wall_clock_duration_in_us);
  DCHECK_EQ(-1, phases.sweep_wall_clock_duration_in_us);
  DCHECK_EQ(-1, phases.weak_wall_clock_duration_in_us);
}

void CheckGarbageCollectionSizesInitialized(
    const v8::metrics::GarbageCollectionSizes& sizes) {
  DCHECK_LE(0, sizes.bytes_before);
  DCHECK_LE(0, sizes.bytes_after);
  DCHECK_LE(0, sizes.bytes_freed);
}

void CheckGarbageCollectionSizesUninitialized(
    const v8::metrics::GarbageCollectionSizes& sizes) {
  DCHECK_EQ(-1, sizes.bytes_before);
  DCHECK_EQ(-1, sizes.bytes_after);
  DCHECK_EQ(-1, sizes.bytes_freed);
}

void CheckGarbageCollectionLimitsInitialized(
    const v8::metrics::GarbageCollectionLimits& limits) {
  DCHECK_LE(0, limits.bytes_baseline);
  DCHECK_LE(0, limits.bytes_limit);
  DCHECK_LE(0, limits.bytes_current);
  DCHECK_LE(0, limits.bytes_max);
}

// Returns true if |event| contains valid cpp histogram values.
bool CheckCppEvents(const v8::metrics::GarbageCollectionFullCycle& event) {
  if (event.total_cpp.mark_wall_clock_duration_in_us == -1) {
    // If a cpp field in |event| is uninitialized, all cpp fields should be
    // uninitialized.
    CheckGarbageCollectionPhasesUninitialized(event.total_cpp);
    CheckGarbageCollectionPhasesUninitialized(event.main_thread_cpp);
    CheckGarbageCollectionPhasesUninitialized(event.main_thread_atomic_cpp);
    CheckGarbageCollectionSizesUninitialized(event.objects_cpp);
    CheckGarbageCollectionSizesUninitialized(event.memory_cpp);
    DCHECK_EQ(-1.0, event.efficiency_cpp_in_bytes_per_us);
    DCHECK_EQ(-1.0, event.main_thread_efficiency_cpp_in_bytes_per_us);
    DCHECK_EQ(-1.0, event.collection_rate_cpp_in_percent);
    DCHECK_EQ(-1.0, event.collection_weight_cpp_in_percent);
    DCHECK_EQ(-1.0, event.main_thread_collection_weight_cpp_in_percent);
    return false;
  }
  // Check that all used values have been initialized.
  CheckGarbageCollectionPhasesInitialized(event.total_cpp);
  CheckGarbageCollectionPhasesInitialized(event.main_thread_cpp);
  CheckGarbageCollectionPhasesInitialized(event.main_thread_atomic_cpp);
  CheckGarbageCollectionSizesInitialized(event.objects_cpp);
  CheckGarbageCollectionSizesInitialized(event.memory_cpp);
  DCHECK_LE(0, event.efficiency_cpp_in_bytes_per_us);
  DCHECK_LE(0, event.main_thread_efficiency_cpp_in_bytes_per_us);
  DCHECK_LE(0, event.collection_rate_cpp_in_percent);
  DCHECK_LE(0.0, event.collection_weight_cpp_in_percent);
  DCHECK_LE(0.0, event.main_thread_collection_weight_cpp_in_percent);
  return true;
}

void CheckUnifiedEvents(const v8::metrics::GarbageCollectionFullCycle& event) {
  // Check that all used values have been initialized.
  CheckGarbageCollectionPhasesInitialized(event.total);
  CheckGarbageCollectionPhasesInitialized(event.main_thread);
  CheckGarbageCollectionPhasesInitialized(event.main_thread_atomic);
  // Incremental marking and sweeping may be uninitialized; the other values
  // must be uninitialized.
  DCHECK_EQ(-1, event.main_thread_incremental.total_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.main_thread_incremental.mark_wall_clock_duration_in_us);
  DCHECK_EQ(-1, event.main_thread_incremental.weak_wall_clock_duration_in_us);
  DCHECK_EQ(-1,
            event.main_thread_incremental.compact_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.main_thread_incremental.sweep_wall_clock_duration_in_us);
  DCHECK_LE(-1, event.incremental_marking_start_stop_wall_clock_duration_in_us);
  CheckGarbageCollectionSizesInitialized(event.objects_cpp);
  CheckGarbageCollectionSizesInitialized(event.memory_cpp);
  CheckGarbageCollectionLimitsInitialized(event.old_generation_consumed);
  CheckGarbageCollectionLimitsInitialized(event.global_consumed);
  DCHECK_LE(0, event.efficiency_in_bytes_per_us);
  DCHECK_LE(0, event.main_thread_efficiency_in_bytes_per_us);
  DCHECK_LE(0, event.collection_rate_in_percent);
  DCHECK_LE(0, event.collection_weight_in_percent);
  DCHECK_LE(0, event.main_thread_collection_weight_in_percent);
}

constexpr size_t kMinSize = 1;
constexpr size_t kMaxSize = 4 * 1024 * 1024;
constexpr size_t kMaxSizeLarge = 32 * 1024 * 1024;
constexpr size_t kNumBuckets = 50;

void ReportHistogramTimesAllGcPhases(
    std::string_view prefix,
    std::string_view suffix,
    const v8::metrics::GarbageCollectionPhases& statistics) {
  base::UmaHistogramTimes(
      base::StrCat({prefix, suffix}),
      base::Microseconds(statistics.total_wall_clock_duration_in_us));
  base::UmaHistogramTimes(
      base::StrCat({prefix, ".Mark", suffix}),
      base::Microseconds(statistics.mark_wall_clock_duration_in_us));
  base::UmaHistogramTimes(
      base::StrCat({prefix, ".Compact", suffix}),
      base::Microseconds(statistics.compact_wall_clock_duration_in_us));
  base::UmaHistogramTimes(
      base::StrCat({prefix, ".Sweep", suffix}),
      base::Microseconds(statistics.sweep_wall_clock_duration_in_us));
  base::UmaHistogramTimes(
      base::StrCat({prefix, ".Weak", suffix}),
      base::Microseconds(statistics.weak_wall_clock_duration_in_us));
}

void ReportHistogramCollectionSizes(
    std::string_view prefix,
    std::string_view suffix,
    const v8::metrics::GarbageCollectionSizes& statistics) {
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".Before", suffix}),
                                 CappedSizeInKB(statistics.bytes_before),
                                 kMinSize, kMaxSize, kNumBuckets);
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".After", suffix}),
                                 CappedSizeInKB(statistics.bytes_after),
                                 kMinSize, kMaxSize, kNumBuckets);
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".Freed", suffix}),
                                 CappedSizeInKB(statistics.bytes_freed),
                                 kMinSize, kMaxSize, kNumBuckets);
}

void ReportHistogramCollectionLimits(
    std::string_view prefix,
    const v8::metrics::GarbageCollectionLimits& statistics) {
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".Limit2.Full"}),
                                 CappedSizeInKB(statistics.bytes_limit),
                                 kMinSize, kMaxSizeLarge, kNumBuckets);
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".Current2.Full"}),
                                 CappedSizeInKB(statistics.bytes_current),
                                 kMinSize, kMaxSizeLarge, kNumBuckets);
  base::UmaHistogramCustomCounts(base::StrCat({prefix, ".Max.Full"}),
                                 CappedSizeInKB(statistics.bytes_max), kMinSize,
                                 kMaxSizeLarge, kNumBuckets);
}

void ReportV8FullHistograms(
    std::string_view priority,
    const v8::metrics::GarbageCollectionFullCycle& event) {
  base::UmaHistogramExactLinear(
      base::StrCat({"V8.GC.Cycle", priority, ".Reason.Full"}), event.reason,
      v8::internal::kGarbageCollectionReasonMaxValue);

  // Interval between full cycles can go over 10s. UmaHistogramMediumTimes() can
  // record intervals up to 3m, as opposed to 10s for UmaHistogramMedium().
  base::UmaHistogramMediumTimes(
      base::StrCat({"V8.GC.Cycle", priority, ".TimeSinceLastCycle.Full"}),
      base::Microseconds(event.total_duration_since_last_mark_compact));

  /* Report throughput metrics: */
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".Full"}), "", event.total);
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".MainThread.Full"}), "",
      event.main_thread);

  /* Report atomic pause metrics: */
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".MainThread.Full.Atomic"}), "",
      event.main_thread_atomic);

  /* Report incremental marking/sweeping metrics: */
  if (event.main_thread_incremental.mark_wall_clock_duration_in_us >= 0) {
    base::UmaHistogramTimes(
        base::StrCat(
            {"V8.GC.Cycle", priority, ".MainThread.Full.Incremental.Mark"}),
        base::Microseconds(
            event.main_thread_incremental.mark_wall_clock_duration_in_us));
  }
  if (event.main_thread_incremental.sweep_wall_clock_duration_in_us >= 0) {
    base::UmaHistogramTimes(
        base::StrCat(
            {"V8.GC.Cycle", priority, ".MainThread.Full.Incremental.Sweep"}),
        base::Microseconds(
            event.main_thread_incremental.sweep_wall_clock_duration_in_us));
  }
  if (event.incremental_marking_start_stop_wall_clock_duration_in_us >= 0) {
    base::UmaHistogramTimes(
        base::StrCat({"V8.GC.Cycle", priority,
                      ".MainThread.Full.Incremental.Mark.StartStop"}),
        base::Microseconds(
            event.incremental_marking_start_stop_wall_clock_duration_in_us));
  }

  ReportHistogramCollectionSizes(
      base::StrCat({"V8.GC.Cycle", priority, ".Objects"}), ".Full",
      event.objects);
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Memory", ".Freed.Full"}),
      CappedSizeInKB(event.memory.bytes_freed), kMinSize, kMaxSize,
      kNumBuckets);
  ReportHistogramCollectionLimits(
      base::StrCat({"V8.GC.Cycle", priority, ".Consumed.OldGeneration"}),
      event.old_generation_consumed);
  ReportHistogramCollectionLimits(
      base::StrCat({"V8.GC.Cycle", priority, ".Consumed.Global"}),
      event.global_consumed);
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".ExternalMemory.Full"}),
      CappedSizeInKB(event.external_memory_bytes), kMinSize, kMaxSizeLarge,
      kNumBuckets);

  /* Report efficacy metrics: */
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Efficiency.Full"}),
      CappedEfficacyInKBPerMs(event.efficiency_in_bytes_per_us), kMinSize,
      kMaxSize, kNumBuckets);

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Efficiency.MainThread.Full"}),
      CappedEfficacyInKBPerMs(event.main_thread_efficiency_in_bytes_per_us),
      kMinSize, kMaxSize, kNumBuckets);

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".CollectionRate.Full"}),
      base::saturated_cast<base::Histogram::Sample32>(
          100 * event.collection_rate_in_percent),
      1, 100, 20);

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".CollectionWeight.Full"}),
      base::saturated_cast<base::Histogram::Sample32>(
          1000 * event.collection_weight_in_percent),
      1, 1000, 20);
  base::UmaHistogramCustomCounts(
      base::StrCat(
          {"V8.GC.Cycle", priority, ".CollectionWeight.MainThread.Full"}),
      base::saturated_cast<base::Histogram::Sample32>(
          1000 * event.main_thread_collection_weight_in_percent),
      1, 1000, 20);
}

void ReportCppHistograms(std::string_view priority,
                         const v8::metrics::GarbageCollectionFullCycle& event) {
  /* Report throughput metrics: */
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".Full"}), ".Cpp",
      event.total_cpp);
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".MainThread.Full"}), ".Cpp",
      event.main_thread_cpp);

  /* Report atomic pause metrics: */
  ReportHistogramTimesAllGcPhases(
      base::StrCat({"V8.GC.Cycle", priority, ".MainThread.Full.Atomic"}),
      ".Cpp", event.main_thread_atomic_cpp);

  /* Report incremental marking/sweeping metrics: */
  if (event.main_thread_incremental_cpp.mark_wall_clock_duration_in_us >= 0) {
    base::UmaHistogramTimes(
        base::StrCat(
            {"V8.GC.Cycle", priority, ".MainThread.Full.Incremental.Mark.Cpp"}),
        base::Microseconds(
            event.main_thread_incremental_cpp.mark_wall_clock_duration_in_us));
  }
  if (event.main_thread_incremental_cpp.sweep_wall_clock_duration_in_us >= 0) {
    base::UmaHistogramTimes(
        base::StrCat({"V8.GC.Cycle", priority,
                      ".MainThread.Full.Incremental.Sweep.Cpp"}),
        base::Microseconds(
            event.main_thread_incremental_cpp.sweep_wall_clock_duration_in_us));
  }

  /* Report size metrics: */
  ReportHistogramCollectionSizes(
      base::StrCat({"V8.GC.Cycle", priority, ".Objects"}), ".Full.Cpp",
      event.objects);
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Memory", ".Freed.Full.Cpp"}),
      CappedSizeInKB(event.memory.bytes_freed), kMinSize, kMaxSize,
      kNumBuckets);

  /* Report efficacy metrics: */
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Efficiency.Full.Cpp"}),
      CappedEfficacyInKBPerMs(event.efficiency_cpp_in_bytes_per_us), kMinSize,
      kMaxSize, kNumBuckets);

  base::UmaHistogramCustomCounts(
      base::StrCat(
          {"V8.GC.Cycle", priority, ".Efficiency.MainThread.Full.Cpp"}),
      CappedEfficacyInKBPerMs(event.main_thread_efficiency_cpp_in_bytes_per_us),
      kMinSize, kMaxSize, kNumBuckets);

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".CollectionRate.Full.Cpp"}),
      base::saturated_cast<base::Histogram::Sample32>(
          100 * event.collection_rate_cpp_in_percent),
      1, 100, 20);

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".CollectionWeight.Full.Cpp"}),
      base::saturated_cast<base::Histogram::Sample32>(
          1000 * event.collection_weight_cpp_in_percent),
      1, 1000, 20);
  base::UmaHistogramCustomCounts(
      base::StrCat(
          {"V8.GC.Cycle", priority, ".CollectionWeight.MainThread.Full.Cpp"}),
      base::saturated_cast<base::Histogram::Sample32>(
          1000 * event.main_thread_collection_weight_cpp_in_percent),
      1, 1000, 20);
}

void ReportV8YoungHistograms(
    std::string_view priority,
    const v8::metrics::GarbageCollectionYoungCycle& event) {
  base::UmaHistogramExactLinear(
      base::StrCat({"V8.GC.Cycle", priority, ".Reason.Young"}), event.reason,
      v8::internal::kGarbageCollectionReasonMaxValue);

  base::UmaHistogramTimes(
      base::StrCat({"V8.GC.Cycle", priority, ".Young"}),
      base::Microseconds(event.total_wall_clock_duration_in_us));
  base::UmaHistogramTimes(
      base::StrCat({"V8.GC.Cycle", priority, ".MainThread.Young"}),
      base::Microseconds(event.main_thread_wall_clock_duration_in_us));

  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Efficiency.Young"}),
      CappedEfficacyInKBPerMs(event.efficiency_in_bytes_per_us), kMinSize,
      kMaxSize, kNumBuckets);
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".Efficiency.MainThread.Young"}),
      CappedEfficacyInKBPerMs(event.main_thread_efficiency_in_bytes_per_us),
      kMinSize, kMaxSize, kNumBuckets);
  base::UmaHistogramCustomCounts(
      base::StrCat({"V8.GC.Cycle", priority, ".CollectionRate.Young"}),
      base::saturated_cast<base::Histogram::Sample32>(
          100 * event.collection_rate_in_percent),
      1, 100, 20);
}

}  // namespace

void V8MetricsRecorder::AddMainThreadEvent(
    const v8::metrics::GarbageCollectionFullCycle& event,
    ContextId context_id) {
  CheckUnifiedEvents(event);

  DCHECK_LE(0, event.reason);

  auto ukm = GetUkmRecorderAndSourceId(context_id);
  if (ukm) {
    auto builder = ukm::builders::V8_GC_FullCycle(ukm->source_id);
    if (event.priority) {
      builder.SetPriority(static_cast<int64_t>(*event.priority));
    }
    builder.SetReason(event.reason)
        .SetReduceMemory(event.reduce_memory)
        .SetIsLoading(event.is_loading)
        .SetReason_IncrementalMarking(event.incremental_marking_reason)
        .SetDuration_SinceLastMarkCompact(
            ukm::GetExponentialBucketMinForFineUserTiming(
                event.total_duration_since_last_mark_compact))
        .SetDuration_Total(ukm::GetExponentialBucketMinForFineUserTiming(
            event.total.total_wall_clock_duration_in_us))
        .SetDuration_MainThread(ukm::GetExponentialBucketMinForFineUserTiming(
            event.main_thread.total_wall_clock_duration_in_us))
        .SetDuration_MainThread_Atomic(
            ukm::GetExponentialBucketMinForFineUserTiming(
                event.main_thread_atomic.total_wall_clock_duration_in_us))
        .SetMemory_BytesFreed(
            ukm::GetExponentialBucketMinForBytes(event.memory.bytes_freed))
        .SetObjects_BytesBefore(
            ukm::GetExponentialBucketMinForBytes(event.objects.bytes_before))
        .SetObjects_BytesAfter(
            ukm::GetExponentialBucketMinForBytes(event.objects.bytes_after))
        .SetObjects_BytesFreed(
            ukm::GetExponentialBucketMinForBytes(event.objects.bytes_freed))
        .SetGlobal_ConsumedBytes_Baseline(ukm::GetExponentialBucketMinForBytes(
            event.global_consumed.bytes_baseline))
        .SetGlobal_ConsumedBytes_Current(ukm::GetExponentialBucketMinForBytes(
            event.global_consumed.bytes_current))
        .SetGlobal_ConsumedBytes_Limit(ukm::GetExponentialBucketMinForBytes(
            event.global_consumed.bytes_limit))
        .SetGlobal_ConsumedBytes_DeltaLimit(
            ukm::GetExponentialBucketMinForBytes(
                event.global_consumed.bytes_limit -
                event.global_consumed.bytes_baseline))
        .SetGlobal_ConsumedBytes_DeltaCurrent(
            ukm::GetExponentialBucketMinForBytes(
                event.global_consumed.bytes_current -
                event.global_consumed.bytes_baseline))
        .SetOldGeneration_ConsumedBytes_Baseline(
            ukm::GetExponentialBucketMinForBytes(
                event.old_generation_consumed.bytes_baseline))
        .SetOldGeneration_ConsumedBytes_Current(
            ukm::GetExponentialBucketMinForBytes(
                event.old_generation_consumed.bytes_current))
        .SetOldGeneration_ConsumedBytes_Limit(
            ukm::GetExponentialBucketMinForBytes(
                event.old_generation_consumed.bytes_limit))
        .SetOldGeneration_ConsumedBytes_DeltaLimit(
            ukm::GetExponentialBucketMinForBytes(
                event.old_generation_consumed.bytes_limit -
                event.old_generation_consumed.bytes_baseline))
        .SetOldGeneration_ConsumedBytes_DeltaCurrent(
            ukm::GetExponentialBucketMinForBytes(
                event.old_generation_consumed.bytes_current -
                event.old_generation_consumed.bytes_baseline));
    if (CheckCppEvents(event)) {
      builder
          .SetDuration_Total_Cpp(ukm::GetExponentialBucketMinForFineUserTiming(
              event.total_cpp.total_wall_clock_duration_in_us))
          .SetDuration_MainThread_Cpp(
              ukm::GetExponentialBucketMinForFineUserTiming(
                  event.main_thread_cpp.total_wall_clock_duration_in_us))
          .SetDuration_MainThread_Atomic_Cpp(
              ukm::GetExponentialBucketMinForFineUserTiming(
                  event.main_thread_atomic_cpp.total_wall_clock_duration_in_us))
          .SetMemory_BytesFreed_Cpp(ukm::GetExponentialBucketMinForBytes(
              event.memory_cpp.bytes_freed))
          .SetObjects_BytesBefore_Cpp(ukm::GetExponentialBucketMinForBytes(
              event.objects_cpp.bytes_before))
          .SetObjects_BytesAfter_Cpp(ukm::GetExponentialBucketMinForBytes(
              event.objects_cpp.bytes_after))
          .SetObjects_BytesFreed_Cpp(ukm::GetExponentialBucketMinForBytes(
              event.objects_cpp.bytes_freed));
    }
    builder.Record(ukm->recorder);
  }

  ReportV8FullHistograms("", event);
  if (event.priority.has_value()) {
    switch (event.priority.value()) {
      case v8::Isolate::Priority::kBestEffort:
        ReportV8FullHistograms(".BestEffort", event);
        break;
      case v8::Isolate::Priority::kUserVisible:
        ReportV8FullHistograms(".UserVisible", event);
        break;
      case v8::Isolate::Priority::kUserBlocking:
        ReportV8FullHistograms(".UserBlocking", event);
    }
  }

  if (CheckCppEvents(event)) {
    ReportCppHistograms("", event);
    if (event.priority.has_value()) {
      switch (event.priority.value()) {
        case v8::Isolate::Priority::kBestEffort:
          ReportCppHistograms(".BestEffort", event);
          break;
        case v8::Isolate::Priority::kUserVisible:
          ReportCppHistograms(".UserVisible", event);
          break;
        case v8::Isolate::Priority::kUserBlocking:
          ReportCppHistograms(".UserBlocking", event);
      }
    }
  }
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

  ReportV8YoungHistograms("", event);
  if (event.priority.has_value()) {
    switch (event.priority.value()) {
      case v8::Isolate::Priority::kBestEffort:
        ReportV8YoungHistograms(".BestEffort", event);
        break;
      case v8::Isolate::Priority::kUserVisible:
        ReportV8YoungHistograms(".UserVisible", event);
        break;
      case v8::Isolate::Priority::kUserBlocking:
        ReportV8YoungHistograms(".UserBlocking", event);
    }
  }
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
