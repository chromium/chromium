// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/profiler_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_profiler_init_options.h"
#include "third_party/blink/renderer/core/timing/profiler.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

static constexpr int kLargeProfilerCount = 128;
static constexpr int kMaxConcurrentProfilerCount = 100;

}  // namespace

class ProfilerGroupTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(ProfilerGroupTest, StopProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

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
TEST_F(ProfilerGroupTest, StopProfilerOnGroupDeallocate) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

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

TEST_F(ProfilerGroupTest, CreateProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(10);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // clean up
  profiler->stop(scope.GetScriptState());
}

TEST_F(ProfilerGroupTest, ClampedSamplingIntervalZero) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

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

  // clean up
  profiler->stop(scope.GetScriptState());
}

TEST_F(ProfilerGroupTest, ClampedSamplingIntervalNext) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(
      (ProfilerGroup::GetBaseSampleInterval() + base::Milliseconds(1))
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

  // clean up
  profiler->stop(scope.GetScriptState());
}

TEST_F(ProfilerGroupTest,
       V8ProfileLimitThrowsExceptionWhenMaxConcurrentReached) {
  V8TestingScope scope;

  HeapVector<Member<Profiler>> profilers;
  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());
  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();

  for (auto i = 0; i < kMaxConcurrentProfilerCount; i++) {
    init_options->setSampleInterval(i);
    profilers.push_back(profiler_group->CreateProfiler(
        scope.GetScriptState(), *init_options, base::TimeTicks(),
        scope.GetExceptionState()));
    EXPECT_FALSE(scope.GetExceptionState().HadException());
  }

  // check kErrorTooManyProfilers
  ProfilerGroup* extra_profiler_group = ProfilerGroup::From(scope.GetIsolate());
  ProfilerInitOptions* extra_init_options = ProfilerInitOptions::Create();
  extra_init_options->setSampleInterval(100);
  for (auto i = kMaxConcurrentProfilerCount; i < kLargeProfilerCount; i++) {
    extra_profiler_group->CreateProfiler(scope.GetScriptState(),
                                         *extra_init_options, base::TimeTicks(),
                                         scope.GetExceptionState());
    EXPECT_TRUE(scope.GetExceptionState().HadException());
    EXPECT_EQ(scope.GetExceptionState().Message(),
              "Reached maximum concurrent amount of profilers");
  }

  for (auto profiler : profilers) {
    profiler->stop(scope.GetScriptState());
  }
}

TEST_F(ProfilerGroupTest, NegativeSamplingInterval) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(-10);
  profiler_group->CreateProfiler(scope.GetScriptState(), *init_options,
                                 base::TimeTicks(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST_F(ProfilerGroupTest, OverflowSamplingInterval) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval((double)std::numeric_limits<int>::max() +
                                  1.f);
  profiler_group->CreateProfiler(scope.GetScriptState(), *init_options,
                                 base::TimeTicks(), scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST_F(ProfilerGroupTest, Bug1119865) {
  class ExpectNoCallFunction : public ScriptFunction::Callable {
   public:
    ScriptValue Call(ScriptState*, ScriptValue) override {
      EXPECT_FALSE(true)
          << "Promise should not resolve without dispatching a task";
      return ScriptValue();
    }
  };

  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);

  auto* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  auto* function = MakeGarbageCollected<ScriptFunction>(
      scope.GetScriptState(), MakeGarbageCollected<ExpectNoCallFunction>());
  profiler->stop(scope.GetScriptState()).Then(function);
}

/*
 *  LEAK TESTS - SHOULD RUN LAST
 */

// Tests that a leaked profiler doesn't crash the isolate on heap teardown.
// These should run last
TEST_F(ProfilerGroupTest, LeakProfiler) {
  V8TestingScope scope;

  ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
  profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

  ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
  init_options->setSampleInterval(0);
  init_options->setMaxBufferSize(0);
  Profiler* profiler = profiler_group->CreateProfiler(
      scope.GetScriptState(), *init_options, base::TimeTicks(),
      scope.GetExceptionState());

  EXPECT_FALSE(profiler->stopped());
}

// Tests that a leaked profiler doesn't crash when disposed alongside its
// context.
TEST_F(ProfilerGroupTest, LeakProfilerWithContext) {
  Profiler* profiler;
  {
    V8TestingScope scope;
    ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
    profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

    ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
    init_options->setSampleInterval(0);
    init_options->setMaxBufferSize(0);
    profiler = profiler_group->CreateProfiler(scope.GetScriptState(),
                                              *init_options, base::TimeTicks(),
                                              scope.GetExceptionState());

    EXPECT_FALSE(profiler->stopped());
  }

  // Force a collection of the underlying Profiler and v8::Context, and ensure
  // a crash doesn't occur.
  profiler = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  test::RunPendingTasks();
}

// Tests that a ProfilerGroup doesn't crash if the ProfilerGroup is destroyed
// before a Profiler::Dispose is ran.
TEST_F(ProfilerGroupTest, Bug1297283) {
  {
    V8TestingScope scope;
    ProfilerGroup* profiler_group = ProfilerGroup::From(scope.GetIsolate());
    profiler_group->OnProfilingContextAdded(scope.GetExecutionContext());

    ProfilerInitOptions* init_options = ProfilerInitOptions::Create();
    init_options->setSampleInterval(0);
    init_options->setMaxBufferSize(0);
    Profiler* profiler = profiler_group->CreateProfiler(
        scope.GetScriptState(), *init_options, base::TimeTicks(),
        scope.GetExceptionState());
    EXPECT_FALSE(profiler->stopped());

    // Force a collection of the underlying Profiler
    profiler = nullptr;
    ThreadState::Current()->CollectAllGarbageForTesting();
    // Exit Scope deallocating Context triggering ProfilerGroup::WillBeDestroyed
    // Ensure doesn't crash.
  }
  test::RunPendingTasks();
}

}  // namespace blink
