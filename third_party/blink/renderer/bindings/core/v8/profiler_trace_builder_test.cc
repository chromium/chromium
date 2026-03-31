// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/profiler_trace_builder.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_marker.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_sample.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_trace.h"
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
      script_state, nullptr, base::TimeTicks::Now());
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

TEST_F(ProfilerTraceBuilderTest, AddEmbedderStateMarkerCrossOriginIsolated) {
  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  ScopedExperimentalJSProfilerMarkersForTest enable_markers(true);

  ProfilerTraceBuilder* builder = MakeGarbageCollected<ProfilerTraceBuilder>(
      script_state, nullptr, base::TimeTicks::Now());
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
      script_state, nullptr, base::TimeTicks::Now());

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
      script_state, nullptr, base::TimeTicks::Now());

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
      script_state, nullptr, base::TimeTicks::Now());

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

}  // namespace blink
