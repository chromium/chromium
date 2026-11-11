// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"

#include <cmath>
#include <iterator>
#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_marker.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_sample.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_trace.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/time_clamper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"
namespace blink {

class ProfilerTraceBuilderTest : public testing::Test {
 protected:
  void SetUp() override {
    task_environment_ = std::make_unique<test::TaskEnvironment>();
  }

  void TearDown() override { task_environment_.reset(); }

  std::unique_ptr<test::TaskEnvironment> task_environment_;
};

TEST_F(ProfilerTraceBuilderTest, AddVMStateMarkerCrossOriginIsolated) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, base::TimeTicks::Now());
  builder->is_cross_origin_isolated_ = true;

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::GC,
                     v8::EmbedderStateTag::EMPTY);

  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 1u);
  auto* sample = samples.at(0).Get();
  EXPECT_EQ(sample->marker(), V8ProfilerMarker::Enum::kGc);
}

TEST_F(ProfilerTraceBuilderTest, VMStateMarkerExposure) {
  const struct {
    v8::StateTag state;
    std::optional<V8ProfilerMarker::Enum> marker;
  } test_cases[] = {
      {v8::JS, V8ProfilerMarker::Enum::kScript},
      {v8::GC, V8ProfilerMarker::Enum::kGc},
      {v8::PARSER, V8ProfilerMarker::Enum::kOther},
      {v8::BYTECODE_COMPILER, V8ProfilerMarker::Enum::kOther},
      {v8::COMPILER, V8ProfilerMarker::Enum::kOther},
      {v8::OTHER, V8ProfilerMarker::Enum::kOther},
      {v8::EXTERNAL, V8ProfilerMarker::Enum::kOther},
      {v8::ATOMICS_WAIT, V8ProfilerMarker::Enum::kScript},
      {v8::IDLE, std::nullopt},
      {v8::LOGGING, std::nullopt},
      {v8::IDLE_EXTERNAL, std::nullopt},
  };

  for (bool markers_enabled : {false, true}) {
    ScopedExperimentalJSProfilerMarkersForTest enable_markers(markers_enabled);
    for (bool is_cross_origin_isolated : {false, true}) {
      V8TestingScope scope;
      auto* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
          scope.GetScriptState(), base::TimeTicks::Now());
      builder->is_cross_origin_isolated_ = is_cross_origin_isolated;

      for (const auto& test_case : test_cases) {
        SCOPED_TRACE(testing::Message()
                     << "state=" << test_case.state
                     << ", markers_enabled=" << markers_enabled
                     << ", cross_origin_isolated=" << is_cross_origin_isolated);
        builder->AddSample(nullptr, base::TimeTicks::Now(), test_case.state,
                           v8::EmbedderStateTag::EMPTY);
        const auto* sample = builder->GetTrace()->samples().back().Get();
        bool expect_marker = markers_enabled && is_cross_origin_isolated &&
                             test_case.marker.has_value();
        ASSERT_EQ(sample->hasMarker(), expect_marker);
        if (expect_marker) {
          EXPECT_EQ(sample->marker(), *test_case.marker);
        }
      }
      EXPECT_EQ(builder->GetTrace()->samples().size(), std::size(test_cases));
    }
  }

  const struct {
    BlinkState state;
    V8ProfilerMarker::Enum marker;
  } rendering_cases[] = {
      {BlinkState::LAYOUT, V8ProfilerMarker::Enum::kLayout},
      {BlinkState::STYLE, V8ProfilerMarker::Enum::kStyle},
      {BlinkState::PAINT, V8ProfilerMarker::Enum::kPaint},
  };
  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);
  for (const bool is_cross_origin_isolated : {false, true}) {
    V8TestingScope scope;
    auto* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
        scope.GetScriptState(), base::TimeTicks::Now());
    builder->is_cross_origin_isolated_ = is_cross_origin_isolated;

    for (const auto& test_case : rendering_cases) {
      SCOPED_TRACE(testing::Message()
                   << "rendering_marker=" << static_cast<int>(test_case.marker)
                   << ", cross_origin_isolated=" << is_cross_origin_isolated);
      builder->AddSample(nullptr, base::TimeTicks::Now(),
                         v8::StateTag::EXTERNAL,
                         static_cast<v8::EmbedderStateTag>(test_case.state));
      const auto* sample = builder->GetTrace()->samples().back().Get();
      const bool expect_marker =
          is_cross_origin_isolated ||
          test_case.marker != V8ProfilerMarker::Enum::kPaint;
      ASSERT_EQ(sample->hasMarker(), expect_marker);
      if (expect_marker) {
        EXPECT_EQ(sample->marker(), test_case.marker);
      }
    }
    EXPECT_EQ(builder->GetTrace()->samples().size(),
              std::size(rendering_cases));
  }
}

TEST_F(ProfilerTraceBuilderTest, AddEmbedderStateMarkerCrossOriginIsolated) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, base::TimeTicks::Now());
  builder->is_cross_origin_isolated_ = true;

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::LAYOUT));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::STYLE));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::PAINT));
  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 3u);
  EXPECT_EQ(samples.at(0).Get()->marker(), V8ProfilerMarker::Enum::kLayout);
  EXPECT_EQ(samples.at(1).Get()->marker(), V8ProfilerMarker::Enum::kStyle);
  EXPECT_EQ(samples.at(2).Get()->marker(), V8ProfilerMarker::Enum::kPaint);
}

TEST_F(ProfilerTraceBuilderTest, AddVMStateMarker) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, base::TimeTicks::Now());

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::GC,
                     v8::EmbedderStateTag::EMPTY);

  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 1u);
  auto* sample = samples.at(0).Get();
  EXPECT_FALSE(sample->hasMarker());
}

TEST_F(ProfilerTraceBuilderTest, AddEmbedderStateMarker) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, base::TimeTicks::Now());

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::LAYOUT));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::STYLE));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::PAINT));
  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 3u);
  EXPECT_EQ(samples.at(0).Get()->marker(), V8ProfilerMarker::Enum::kLayout);
  EXPECT_EQ(samples.at(1).Get()->marker(), V8ProfilerMarker::Enum::kStyle);
  EXPECT_FALSE(samples.at(2).Get()->hasMarker());
}

TEST_F(ProfilerTraceBuilderTest, AddEmbedderStateMarkerFeatureDisabled) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest disable_markers(false);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, base::TimeTicks::Now());

  base::TimeTicks sample_ticks = base::TimeTicks::Now();
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::LAYOUT));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::STYLE));
  builder->AddSample(nullptr, sample_ticks, v8::StateTag::IDLE,
                     static_cast<v8::EmbedderStateTag>(BlinkState::PAINT));
  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 3u);
  EXPECT_FALSE(samples.at(0).Get()->hasMarker());
  EXPECT_FALSE(samples.at(1).Get()->hasMarker());
  EXPECT_FALSE(samples.at(2).Get()->hasMarker());
}

TEST_F(ProfilerTraceBuilderTest,
       SampleTimestampClampingNonCrossOriginIsolated) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  base::TimeTicks time_origin = base::TimeTicks::Now();
  ProfilerTraceBuilder* builder =
      MakeGarbageCollected<ProfilerTraceBuilder>(script_state, time_origin);

  // In non-cross-origin-isolated contexts, timestamps should be coarsened
  // to kCoarseResolutionMicroseconds (100us).
  for (int i = 1; i <= 100; ++i) {
    base::TimeTicks sample_ticks = time_origin + base::Microseconds(i * 13);
    builder->AddSample(nullptr, sample_ticks, v8::StateTag::JS,
                       v8::EmbedderStateTag::EMPTY);
  }

  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 100u);
  for (wtf_size_t i = 0; i < samples.size(); ++i) {
    base::TimeTicks sample_ticks =
        time_origin + base::Microseconds((i + 1) * 13);
    DOMHighResTimeStamp expected_timestamp =
        Performance::MonotonicTimeToDOMHighResTimeStamp(
            time_origin, sample_ticks, /*allow_negative_value=*/true,
            /*cross_origin_isolated_capability=*/false);
    EXPECT_EQ(samples.at(i).Get()->timestamp(), expected_timestamp);
    int64_t timestamp_us = static_cast<int64_t>(
        std::round(samples.at(i).Get()->timestamp() *
                   base::Time::kMicrosecondsPerMillisecond));
    EXPECT_EQ(timestamp_us % TimeClamper::kCoarseResolutionMicroseconds, 0);
  }
}

TEST_F(ProfilerTraceBuilderTest, SampleTimestampClampingCrossOriginIsolated) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  base::TimeTicks time_origin = base::TimeTicks::Now();
  ProfilerTraceBuilder* builder =
      MakeGarbageCollected<ProfilerTraceBuilder>(script_state, time_origin);
  builder->is_cross_origin_isolated_ = true;

  // In cross-origin-isolated contexts, timestamps should be coarsened
  // to kFineResolutionMicroseconds (5us).
  for (int i = 1; i <= 100; ++i) {
    base::TimeTicks sample_ticks = time_origin + base::Microseconds(i * 13);
    builder->AddSample(nullptr, sample_ticks, v8::StateTag::JS,
                       v8::EmbedderStateTag::EMPTY);
  }

  auto* profiler_trace = builder->GetTrace();
  const auto& samples = profiler_trace->samples();
  EXPECT_EQ(samples.size(), 100u);
  int non_coarse_count = 0;
  for (wtf_size_t i = 0; i < samples.size(); ++i) {
    base::TimeTicks sample_ticks =
        time_origin + base::Microseconds((i + 1) * 13);
    DOMHighResTimeStamp expected_timestamp =
        Performance::MonotonicTimeToDOMHighResTimeStamp(
            time_origin, sample_ticks, /*allow_negative_value=*/true,
            /*cross_origin_isolated_capability=*/true);
    EXPECT_EQ(samples.at(i).Get()->timestamp(), expected_timestamp);
    int64_t timestamp_us = static_cast<int64_t>(
        std::round(samples.at(i).Get()->timestamp() *
                   base::Time::kMicrosecondsPerMillisecond));
    EXPECT_EQ(timestamp_us % TimeClamper::kFineResolutionMicroseconds, 0);
    if (timestamp_us % TimeClamper::kCoarseResolutionMicroseconds != 0) {
      non_coarse_count++;
    }
  }
  EXPECT_GT(non_coarse_count, 0);
}

}  // namespace blink
