// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"

namespace blink {
namespace {

using ::testing::Invoke;
using ::testing::MockFunction;

void EnsureTickAboutToElapse(base::test::TaskEnvironment& environment) {
  auto now = base::TimeTicks::Now();
  auto next_tick = MetronomeSource::TimeSnappedToNextTick(now);
  environment.FastForwardBy(next_tick - now);
}

void EnsureTickJustElapsed(base::test::TaskEnvironment& environment) {
  EnsureTickAboutToElapse(environment);
  environment.FastForwardBy(base::Microseconds(1));
}

TEST(MetronomeSourceTest, IdleMetronomePostsNoTasks) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto now = base::TimeTicks::Now();
  auto metronome = source.CreateWebRtcMetronome();
  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(now, base::TimeTicks::Now());
}

TEST(MetronomeSourceTest, SupportsCallsBeyondSourceLifetime) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  auto source =
      std::make_unique<MetronomeSource>(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source->CreateWebRtcMetronome();

  metronome->RequestCallOnNextTick([] {});
  source = nullptr;

  // This just makes use of the metronome after the source is gone.
  metronome->RequestCallOnNextTick([] {});
  metronome->TickPeriod();
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest, InvokesRequestedCallbackOnTick) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  EnsureTickAboutToElapse(task_environment);
  auto start_time = base::TimeTicks::Now();
  auto expected_run_time = MetronomeSource::TimeSnappedToNextTick(start_time);
  EXPECT_EQ(expected_run_time, start_time);
  EXPECT_CALL(callback, Call).WillOnce(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest, InvokesRequestedCallbackAfterTickElapsed) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  EnsureTickJustElapsed(task_environment);
  auto start_time = base::TimeTicks::Now();
  auto expected_run_time = MetronomeSource::TimeSnappedToNextTick(start_time);
  EXPECT_GT(expected_run_time, start_time);
  EXPECT_CALL(callback, Call).WillOnce(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest, InvokesTwoCallbacksOnSameTick) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  EnsureTickJustElapsed(task_environment);
  auto expected_run_time =
      MetronomeSource::TimeSnappedToNextTick(base::TimeTicks::Now());
  EXPECT_CALL(callback, Call).Times(2).WillRepeatedly(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardBy(MetronomeSource::Tick() / 2);
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest, InvokesRequestedCallbackOnTickFromCallbackOnTick) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  auto expected_run_time =
      MetronomeSource::TimeSnappedToNextTick(base::TimeTicks::Now());
  bool first_callback_invoke = true;
  EXPECT_CALL(callback, Call).Times(2).WillRepeatedly(Invoke([&] {
    if (first_callback_invoke)
      metronome->RequestCallOnNextTick(callback.AsStdFunction());
    first_callback_invoke = false;
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest,
     InvokesRequestedCallbackOnNextTickFromCallbackOnTick) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  auto expected_run_time = MetronomeSource::TimeSnappedToNextTick(
      MetronomeSource::TimeSnappedToNextTick(base::TimeTicks::Now()) +
      base::Microseconds(1));

  testing::InSequence s;
  EXPECT_CALL(callback, Call).WillOnce(Invoke([&] {
    task_environment.AdvanceClock(base::Microseconds(1));
    metronome->RequestCallOnNextTick(callback.AsStdFunction());
  }));
  EXPECT_CALL(callback, Call).WillOnce(Invoke(([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  })));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardUntilNoTasksRemain();
}

TEST(MetronomeSourceTest, WebRtcMetronomeAdapterTickPeriod) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  EXPECT_EQ(MetronomeSource::Tick().InMicroseconds(),
            source.CreateWebRtcMetronome()->TickPeriod().us());
}

TEST(MetronomeSourceTest, MultipleMetronomesAreAligned) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  MetronomeSource source(base::SequencedTaskRunnerHandle::Get());
  auto metronome1 = source.CreateWebRtcMetronome();
  auto metronome2 = source.CreateWebRtcMetronome();
  MockFunction<void()> callback;
  absl::optional<base::TimeTicks> callback_time;

  // Request 2 callbacks that should be called on the same tick instant.
  // Nudge time between the requests to guard against too simplistic
  // implementations.
  EnsureTickJustElapsed(task_environment);
  metronome1->RequestCallOnNextTick(callback.AsStdFunction());
  task_environment.FastForwardBy(base::Microseconds(1));
  metronome2->RequestCallOnNextTick(callback.AsStdFunction());
  EXPECT_CALL(callback, Call).Times(2).WillRepeatedly(Invoke([&] {
    if (!callback_time.has_value())
      callback_time = base::TimeTicks::Now();
    else
      EXPECT_EQ(*callback_time, base::TimeTicks::Now());
  }));
  task_environment.FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace blink
