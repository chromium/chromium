// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_audio_worker.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const size_t kTestCallbacks = 5;

using testing::Eq;
using testing::SizeIs;

class FakeAudioWorkerTest : public testing::Test {
 public:
  FakeAudioWorkerTest()
      : params_(AudioParameters::AUDIO_FAKE,
                ChannelLayoutConfig::Stereo(),
                44100,
                128),
        fake_worker_(task_environment_.GetMainThreadTaskRunner(), params_) {
    time_between_callbacks_ = base::Microseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
  }

  FakeAudioWorkerTest(const FakeAudioWorkerTest&) = delete;
  FakeAudioWorkerTest& operator=(const FakeAudioWorkerTest&) = delete;

  ~FakeAudioWorkerTest() override = default;

  void CalledByFakeWorker(base::TimeTicks ideal_time, base::TimeTicks now) {
    callbacks_.push_back(base::TimeTicks::Now());
  }

  void RunOnAudioThread() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Start(base::BindRepeating(
        &FakeAudioWorkerTest::CalledByFakeWorker, base::Unretained(this)));
  }

  void StopStartOnAudioThread() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Stop();
    RunOnAudioThread();
  }

  void TimeCallbacksOnAudioThread(size_t callbacks) {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());

    if (callbacks_.size() == 0) {
      RunOnAudioThread();
    }

    // Keep going until we've seen the requested number of callbacks.
    if (callbacks_.size() < callbacks) {
      TaskRunner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                         base::Unretained(this), callbacks),
          time_between_callbacks_ / 2);
    } else {
      EndTest();
    }
  }

  void EndTest() {
    ASSERT_TRUE(TaskRunner()->BelongsToCurrentThread());
    fake_worker_.Stop();
  }

  scoped_refptr<base::SingleThreadTaskRunner> TaskRunner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  AudioParameters params_;
  FakeAudioWorker fake_worker_;
  base::TimeDelta time_between_callbacks_;
  std::vector<base::TimeTicks> callbacks_;
};

TEST_F(FakeAudioWorkerTest, FakeBasicCallback) {
  base::OnceClosure run_on_audio_thread = base::BindOnce(
      &FakeAudioWorkerTest::RunOnAudioThread, base::Unretained(this));
  base::OnceClosure end_test =
      base::BindOnce(&FakeAudioWorkerTest::EndTest, base::Unretained(this));

  // Start() should immediately post a task to run the callback, so we
  // should end up with only a single callback being run.
  //
  // PostTaskAndReply because we want to end_test after run_on_audio_thread is
  // finished. This is because RunOnAudioThread may post other tasks which
  // should run before we end_test.
  task_environment_.GetMainThreadTaskRunner()->PostTaskAndReply(
      FROM_HERE, std::move(run_on_audio_thread), std::move(end_test));

  task_environment_.RunUntilIdle();

  EXPECT_THAT(callbacks_, SizeIs(1));
}

// Ensure the time between callbacks is correct.
TEST_F(FakeAudioWorkerTest, TimeBetweenCallbacks) {
  TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::TimeCallbacksOnAudioThread,
                     base::Unretained(this), kTestCallbacks));
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(callbacks_, SizeIs(Eq(kTestCallbacks)));

  // There are only (kTestCallbacks - 1) intervals between kTestCallbacks.
  base::TimeTicks start_time = callbacks_.front();
  std::vector<base::TimeTicks> expected_callback_times;
  for (size_t i = 0; i < callbacks_.size(); i++) {
    base::TimeTicks expected = start_time + time_between_callbacks_ * i;
    expected_callback_times.push_back(expected);
  }

  std::vector<int64_t> time_between_callbacks;
  for (size_t i = 0; i < callbacks_.size() - 1; i++) {
    time_between_callbacks.push_back(
        (callbacks_.at(i + 1) - callbacks_.at(i)).InMilliseconds());
  }

  EXPECT_THAT(time_between_callbacks,
              testing::Each(time_between_callbacks_.InMilliseconds()));
}

// Ensure Start()/Stop() on the worker doesn't generate too many callbacks. See
// http://crbug.com/159049.
TEST_F(FakeAudioWorkerTest, StartStopClearsCallbacks) {
  TaskRunner()->PostTask(FROM_HERE,
                         base::BindOnce(&FakeAudioWorkerTest::RunOnAudioThread,
                                        base::Unretained(this)));

  // Issuing a Stop() / Start() in the middle of the callback period should not
  // trigger a callback.
  task_environment_.FastForwardBy(time_between_callbacks_ / 2);
  EXPECT_THAT(callbacks_, SizeIs(1));
  TaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeAudioWorkerTest::StopStartOnAudioThread,
                                base::Unretained(this)));

  task_environment_.FastForwardBy(time_between_callbacks_);
  // We expect 3 callbacks: First Start(), Second Start(), and one for the
  // period. If the first callback was not cancelled, we would get 4 callbacks,
  // two on the first period.
  TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeAudioWorkerTest::EndTest, base::Unretained(this)));

  // EndTest() will ensure the proper number of callbacks have occurred.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_THAT(callbacks_, SizeIs(3));
}

class FakeAudioWorkerMockTaskTest : public testing::Test {
 public:
  FakeAudioWorkerMockTaskTest()
      : params_(AudioParameters::AUDIO_FAKE,
                ChannelLayoutConfig::Stereo(),
                44100,
                128),
        fake_worker_(task_runner_, params_) {
    DCHECK(!global_clock_);
    global_clock_ = task_runner_->GetMockTickClock();
    time_between_callbacks_ = base::Microseconds(
        params_.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
        static_cast<float>(params_.sample_rate()));
    clock_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        nullptr, TimeTicksOverride, nullptr);
  }

  FakeAudioWorkerMockTaskTest(const FakeAudioWorkerMockTaskTest&) = delete;
  FakeAudioWorkerMockTaskTest& operator=(const FakeAudioWorkerMockTaskTest&) =
      delete;

  ~FakeAudioWorkerMockTaskTest() override { global_clock_ = nullptr; }

  void CalledByFakeWorker(base::TimeTicks ideal_time, base::TimeTicks now) {
    callbacks_.push_back(base::TimeTicks::Now());
  }

  void SetUp() override {
    {
      base::TestMockTimeTaskRunner::ScopedContext ctx(task_runner_);
      fake_worker_.Start(
          base::BindRepeating(&FakeAudioWorkerMockTaskTest::CalledByFakeWorker,
                              base::Unretained(this)));
    }
  }

  void TearDown() override {
    {
      base::TestMockTimeTaskRunner::ScopedContext ctx(task_runner_);
      fake_worker_.Stop();
    }
    task_runner_->RunUntilIdle();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      new base::TestMockTimeTaskRunner();
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> clock_overrides_;
  AudioParameters params_;
  FakeAudioWorker fake_worker_;
  base::TimeDelta time_between_callbacks_;
  std::vector<base::TimeTicks> callbacks_;

  static const base::TickClock* global_clock_;
  static base::TimeTicks TimeTicksOverride() {
    DCHECK(global_clock_);
    return global_clock_->NowTicks();
  }
};

const base::TickClock* FakeAudioWorkerMockTaskTest::global_clock_ = nullptr;

// This test is disabled because when late we skip reading to maintain
// compatibility for input and output streams.
// This test is designed to be manually executed.
TEST_F(FakeAudioWorkerMockTaskTest, DISABLED_LateCallbackProducesCallback) {
  task_runner_->RunUntilIdle();
  EXPECT_THAT(callbacks_, SizeIs(1));

  // Advancing 2 periods will trigger the late logic. It should result in one
  // callback, And one queued item which will run in 0.5 callback periods.
  task_runner_->AdvanceMockTickClock(time_between_callbacks_ * 2.5);
  task_runner_->RunUntilIdle();
  EXPECT_THAT(callbacks_, SizeIs(2));
  // Fast-forward to trigger the next time. Note that 0.5 does not work due to
  // rounding in the next frame logic, since 128 does not divide 44100.
  task_runner_->FastForwardBy(time_between_callbacks_ * 0.501);
  EXPECT_THAT(callbacks_, SizeIs(3));
}

TEST_F(FakeAudioWorkerMockTaskTest, CallbackDelay) {
  // Initial call only
  task_runner_->RunUntilIdle();

  // Run the clock forward 1.5 periods and then trigger the callback.
  // This means we are not behind, but the next callback should occur
  // in 0.5 periods.
  task_runner_->AdvanceMockTickClock(time_between_callbacks_ * 1.5);
  task_runner_->RunUntilIdle();
  EXPECT_THAT(callbacks_, SizeIs(2));
  EXPECT_EQ(task_runner_->NextPendingTaskDelay(), time_between_callbacks_ / 2);
}
}  // namespace media
