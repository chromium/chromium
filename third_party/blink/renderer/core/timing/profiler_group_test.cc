// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/core/timing/profiler_init_options.h"

namespace blink {

// Tests that a leaked profiler doesn't crash the isolate on heap teardown.
TEST(ProfilerGroupTest, LeakProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
}

TEST(ProfilerGroupTest, StopProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  profiler->stop(scope.GetScriptState());
  EXPECT_TRUE(profiler->stopped());
}

// Tests that attached profilers are stopped on ProfilerGroup deallocation.
TEST(ProfilerGroupTest, StopProfilerOnGroupDeallocate) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  profiler_group->WillBeDestroyed();
  EXPECT_TRUE(profiler->stopped());
}

TEST(ProfilerGroupTest, CreateProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(10);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(ProfilerGroupTest, ClampedSamplingIntervalZero) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  // Verify that the sample interval clamped to the next non-zero supported
  // interval.
  EXPECT_EQ(profiler->sampleInterval(),
            ProfilerGroup::GetBaseSampleInterval().InMilliseconds());
}

TEST(ProfilerGroupTest, ClampedSamplingIntervalNext) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval((ProfilerGroup::GetBaseSampleInterval() +
                                   base::TimeDelta::FromMilliseconds(1))
                                      .InMilliseconds());
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  // Verify that the sample interval clamped to the next highest supported
  // interval.
  EXPECT_EQ(profiler->sampleInterval(),
            (ProfilerGroup::GetBaseSampleInterval() * 2).InMilliseconds());
}

TEST(ProfilerGroupTest, NegativeSamplingInterval) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(-10);
  profiler_group->CreateProfiler(scope.GetScriptState(), *init_options,
                                 base::TimeTicks(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(ProfilerGroupTest, OverflowSamplingInterval) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval((double)std::numeric_limits<int>::max() +
                                  1.f);
  profiler_group->CreateProfiler(scope.GetScriptState(), *init_options,
                                 base::TimeTicks(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

}  // namespace blink
