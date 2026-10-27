// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"

#include <cstdint>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-metrics.h"

namespace blink {
namespace {

constexpr int64_t kKB = 1024;

v8::metrics::GarbageCollectionPhases MakeInitializedPhases() {
  return {
      .total_wall_clock_duration_in_us = 1000,
      .compact_wall_clock_duration_in_us = 100,
      .mark_wall_clock_duration_in_us = 500,
      .sweep_wall_clock_duration_in_us = 300,
      .weak_wall_clock_duration_in_us = 100,
  };
}

TEST(V8MetricsRecorderTest, FullCycleSizeHistograms) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  V8MetricsRecorder recorder(scope.GetIsolate());

  v8::metrics::GarbageCollectionFullCycle event;
  event.reason = 0;
  event.priority = v8::Isolate::Priority::kUserVisible;

  event.total = MakeInitializedPhases();
  event.main_thread = MakeInitializedPhases();
  event.main_thread_atomic = MakeInitializedPhases();
  event.total_cpp = MakeInitializedPhases();
  event.main_thread_cpp = MakeInitializedPhases();
  event.main_thread_atomic_cpp = MakeInitializedPhases();

  event.objects = {
      .bytes_before = 40000 * kKB,
      .bytes_after = 25000 * kKB,
      .bytes_freed = 15000 * kKB,
  };
  event.memory = {
      .bytes_before = 64000 * kKB,
      .bytes_after = 44000 * kKB,
      .bytes_freed = 20000 * kKB,
  };
  event.objects_cpp = {
      .bytes_before = 400 * kKB,
      .bytes_after = 250 * kKB,
      .bytes_freed = 150 * kKB,
  };
  event.memory_cpp = {
      .bytes_before = 800 * kKB,
      .bytes_after = 500 * kKB,
      .bytes_freed = 300 * kKB,
  };

  event.old_generation_consumed = {
      .bytes_baseline = 10000 * kKB,
      .bytes_limit = 50000 * kKB,
      .bytes_current = 40000 * kKB,
      .bytes_max = 100000 * kKB,
  };
  event.global_consumed = {
      .bytes_baseline = 12000 * kKB,
      .bytes_limit = 60000 * kKB,
      .bytes_current = 45000 * kKB,
      .bytes_max = 120000 * kKB,
  };

  event.external_memory_bytes = 1000 * kKB;
  event.found_js_global_proxies = 1;
  event.collection_rate_in_percent = 0.375;
  event.collection_rate_cpp_in_percent = 0.375;
  event.efficiency_in_bytes_per_us = 15.0;
  event.efficiency_cpp_in_bytes_per_us = 0.15;
  event.main_thread_efficiency_in_bytes_per_us = 15.0;
  event.main_thread_efficiency_cpp_in_bytes_per_us = 0.15;
  event.collection_weight_in_percent = 0.01;
  event.collection_weight_cpp_in_percent = 0.001;
  event.main_thread_collection_weight_in_percent = 0.01;
  event.main_thread_collection_weight_cpp_in_percent = 0.001;
  event.total_duration_since_last_mark_compact = 100000;

  base::HistogramTester histogram_tester;
  recorder.AddMainThreadEvent(event, v8::metrics::Recorder::ContextId::Empty());

  const auto expect_size_kb = [&](std::string_view name, int32_t expected_kb) {
    histogram_tester.ExpectUniqueSample(name, expected_kb, 1);
    EXPECT_EQ(histogram_tester.GetTotalSum(name), expected_kb) << name;
  };

  for (std::string_view priority :
       {std::string_view(""), std::string_view(".UserVisible")}) {
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.Before.Full"}), 40000);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.After.Full"}), 25000);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.Freed.Full"}), 15000);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Memory.Freed.Full"}), 20000);

    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.Before.Full.Cpp"}),
        400);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.After.Full.Cpp"}),
        250);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Objects.Freed.Full.Cpp"}),
        150);
    expect_size_kb(
        base::StrCat({"V8.GC.Cycle", priority, ".Memory.Freed.Full.Cpp"}), 300);
  }
}

}  // namespace
}  // namespace blink
