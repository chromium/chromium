// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_thread_hang_monitor.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/audio/audio_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::Test;
using HangAction = media::AudioThreadHangMonitor::HangAction;

namespace media {

namespace {

constexpr int kStarted =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kStarted);
constexpr int kHung =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kHung);
constexpr int kRecovered =
    static_cast<int>(AudioThreadHangMonitor::ThreadStatus::kRecovered);

constexpr base::TimeDelta kShortHangDeadline = base::Seconds(5);
constexpr base::TimeDelta kLongHangDeadline = base::Minutes(30);

}  // namespace

class AudioThreadHangMonitorTest : public Test {
 public:
  AudioThreadHangMonitorTest()
      : AudioThreadHangMonitorTest(/*rearm_after_late_check=*/true) {}

  // Sets kAudioThreadHangMonitorRearmAfterLateCheck before the audio thread
  // starts and before the monitor, which reads it in its constructor, is
  // created.
  explicit AudioThreadHangMonitorTest(bool rearm_after_late_check)
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        histograms_(),
        audio_thread_("Audio thread"),
        hang_monitor_({nullptr, base::OnTaskRunnerDeleter(nullptr)}) {
    feature_list_.InitWithFeatureState(
        features::kAudioThreadHangMonitorRearmAfterLateCheck,
        rearm_after_late_check);
    CHECK(audio_thread_.Start());
    // We must inject the main thread task runner as the hang monitor task
    // runner since TaskEnvironment::FastForwardBy only works for the main
    // thread.
    hang_monitor_ = AudioThreadHangMonitor::Create(
        HangAction::kDoNothing, std::nullopt, task_env_.GetMockTickClock(),
        audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  }

  ~AudioThreadHangMonitorTest() override {
    hang_monitor_.reset();
    task_env_.RunUntilIdle();
  }

  void SetHangActionCallbacksForTesting() {
    hang_monitor_->SetHangActionCallbacksForTesting(
        base::BindRepeating(&AudioThreadHangMonitorTest::HangActionDump,
                            base::Unretained(this)),
        base::BindRepeating(&AudioThreadHangMonitorTest::HangActionTerminate,
                            base::Unretained(this)));
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  // Advances the mock clock without running any tasks and then runs the
  // overdue hang monitor timer, which thereby fires late. Assumes the default
  // hang deadline (1 minute ping interval).
  void SimulateLateCheck() {
    task_env_.AdvanceClock(base::Minutes(5));
    RunUntilIdle();
  }

  // Simulates a late check where the audio thread had not yet responded to the
  // in-flight ping when the late check fired (e.g. system suspend/resume),
  // after which the audio thread resumes and processes tasks.
  void SimulateUnansweredLateCheckThenResume() {
    BlockAudioThreadUntilEvent();
    // Run one on-time check while |audio_thread_| is blocked so a ping task is
    // queued behind |event_| and |alive_flag_| is false.
    task_env_.FastForwardBy(base::Minutes(1));
    SimulateLateCheck();
    event_.Signal();
    FlushAudioThread();
  }

  void FlushAudioThread() {
    base::WaitableEvent ev;
    audio_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(&ev)));
    ev.Wait();
  }

  void BlockAudioThreadUntilEvent() {
    // We keep |event_| as a member of the test fixture to make sure that the
    // audio thread terminates before |event_| is destructed.
    event_.Reset();
    audio_thread_.task_runner()->PostTask(FROM_HERE,
                                          event_.GetWaitCallbackForTesting());
  }

  MOCK_METHOD0(HangActionDump, void());
  MOCK_METHOD0(HangActionTerminate, void());

  base::test::ScopedFeatureList feature_list_;
  base::WaitableEvent event_;
  base::test::TaskEnvironment task_env_;
  base::HistogramTester histograms_;
  base::Thread audio_thread_;
  AudioThreadHangMonitor::Ptr hang_monitor_;
};

TEST_F(AudioThreadHangMonitorTest, LogsThreadStarted) {
  RunUntilIdle();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(AudioThreadHangMonitorTest, DoesNotLogThreadHungWhenOk) {
  RunUntilIdle();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::Minutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(AudioThreadHangMonitorTest, LogsHungWhenAudioThreadIsBlocked) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest, DoesNotLogThreadHungWithShortDeadline) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, kShortHangDeadline, task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(kShortHangDeadline / 2);
  event_.Signal();

  // Two started events, one for the originally created hang monitor and one for
  // the new created here.
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2)));
}

TEST_F(AudioThreadHangMonitorTest, LogsThreadHungWithShortDeadline) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, kShortHangDeadline, task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(kShortHangDeadline * 2);
  event_.Signal();

  // Two started events, one for the originally created hang monitor and one for
  // the new created here.
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest, DoesNotLogThreadHungWithLongDeadline) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, kLongHangDeadline, task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(kLongHangDeadline / 2);
  event_.Signal();

  // Two started events, one for the originally created hang monitor and one for
  // the new created here.
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2)));
}

TEST_F(AudioThreadHangMonitorTest, LogsThreadHungWithLongDeadline) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, kLongHangDeadline, task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(kLongHangDeadline * 2);
  event_.Signal();

  // Two started events, one for the originally created hang monitor and one for
  // the new created here.
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

// Zero deadline means that the default deadline should be used.
TEST_F(AudioThreadHangMonitorTest, ZeroDeadlineMeansDefaultDeadline) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, base::TimeDelta(), task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::Minutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2)));

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsRecoveredWhenAudioThreadIsBlockedThenRecovers) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::Minutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1),
                          base::Bucket(kRecovered, 1)));
}

TEST_F(AudioThreadHangMonitorTest, NoHangActionWhenOk) {
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  for (int i = 0; i < 10; ++i) {
    // Flush the audio thread, then advance the clock. The audio thread should
    // register as "alive" every time.
    FlushAudioThread();
    task_env_.FastForwardBy(base::Minutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(AudioThreadHangMonitorTest, DumpsWhenAudioThreadIsBlocked) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDump, std::nullopt, task_env_.GetMockTickClock(),
      audio_thread_.task_runner(), task_env_.GetMainThreadTaskRunner());
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  EXPECT_CALL(*this, HangActionDump).Times(1);

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest, TerminatesProcessWhenAudioThreadIsBlocked) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kTerminateCurrentProcess, std::nullopt,
      task_env_.GetMockTickClock(), audio_thread_.task_runner(),
      task_env_.GetMainThreadTaskRunner());
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  EXPECT_CALL(*this, HangActionTerminate).Times(1);

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       DumpsAndTerminatesProcessWhenAudioThreadIsBlocked) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDumpAndTerminateCurrentProcess, std::nullopt,
      task_env_.GetMockTickClock(), audio_thread_.task_runner(),
      task_env_.GetMainThreadTaskRunner());
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  EXPECT_CALL(*this, HangActionDump).Times(1);
  EXPECT_CALL(*this, HangActionTerminate).Times(1);

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsHungWhenAudioThreadIsBlockedAfterLateCheck) {
  RunUntilIdle();

  SimulateUnansweredLateCheckThenResume();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsHungWhenAudioThreadIsBlockedAfterConsecutiveLateChecks) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();

  // The first late check succeeds since the audio thread is considered alive
  // at start. Late checks without a response are skipped, so the following
  // ones don't count as failed pings even though the audio thread is blocked.
  for (int i = 0; i < 5; ++i) {
    SimulateLateCheck();
  }
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));

  // Exactly three checks that run on time detect the hang.
  task_env_.FastForwardBy(base::Minutes(2));
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));

  task_env_.FastForwardBy(base::Minutes(1));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsHungWhenAudioThreadIsBlockedBeforeAndAfterLateCheck) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();

  // The first check succeeds since the audio thread is considered alive at
  // start. The next two checks fail.
  task_env_.FastForwardBy(base::Minutes(3));

  // A late check without a response is not counted as a failed ping, and
  // doesn't reset the count of failed pings either.
  SimulateLateCheck();
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));

  // Hence, a single additional failed ping is enough to detect the hang.
  task_env_.FastForwardBy(base::Minutes(1));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       CountsResponseBeforeLateCheckAsSuccessfulPing) {
  RunUntilIdle();

  // The first check succeeds. The next two checks fail.
  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(3));

  // The audio thread responds before the late check, then blocks again.
  event_.Signal();
  FlushAudioThread();
  BlockAudioThreadUntilEvent();
  SimulateLateCheck();

  // The response counts as a successful ping, so one failed ping after the
  // late check is not a hang...
  task_env_.FastForwardBy(base::Minutes(1));
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));

  // ...but three are.
  task_env_.FastForwardBy(base::Minutes(2));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       LogsRecoveredAfterLateCheckWhenAudioThreadRecovers) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(4));
  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));

  SimulateLateCheck();
  event_.Signal();

  for (int i = 0; i < 3; ++i) {
    FlushAudioThread();
    task_env_.FastForwardBy(base::Minutes(1));
  }

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1),
                          base::Bucket(kRecovered, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       TerminatesProcessWhenAudioThreadIsBlockedAfterLateCheck) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kTerminateCurrentProcess, std::nullopt,
      task_env_.GetMockTickClock(), audio_thread_.task_runner(),
      task_env_.GetMainThreadTaskRunner());
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  EXPECT_CALL(*this, HangActionTerminate).Times(1);

  SimulateUnansweredLateCheckThenResume();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorTest,
       DoesNotPingAudioThreadAgainWhilePingIsPending) {
  // |audio_task_runner| never runs its tasks, so all pings stay pending.
  auto audio_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, std::nullopt, task_env_.GetMockTickClock(),
      audio_task_runner, task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  task_env_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(audio_task_runner->NumPendingTasks(), 1u);
}

// Runs with kAudioThreadHangMonitorRearmAfterLateCheck disabled, i.e. with the
// behavior from before crbug.com/572682743 was fixed.
class AudioThreadHangMonitorRearmDisabledTest
    : public AudioThreadHangMonitorTest {
 public:
  AudioThreadHangMonitorRearmDisabledTest()
      : AudioThreadHangMonitorTest(/*rearm_after_late_check=*/false) {}
};

TEST_F(AudioThreadHangMonitorRearmDisabledTest,
       LogsHungWhenAudioThreadIsBlocked) {
  RunUntilIdle();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1), base::Bucket(kHung, 1)));
}

TEST_F(AudioThreadHangMonitorRearmDisabledTest,
       DoesNotLogHungWhenAudioThreadIsBlockedAfterAnsweredLateCheck) {
  RunUntilIdle();

  // The audio thread is considered alive at start, so this late check is
  // answered. With the feature disabled, it's skipped anyway, so all
  // following checks are late too and the hang isn't detected.
  SimulateLateCheck();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 1)));
}

TEST_F(
    AudioThreadHangMonitorRearmDisabledTest,
    DoesNotTerminateProcessWhenAudioThreadIsBlockedAfterUnansweredLateCheck) {
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kTerminateCurrentProcess, std::nullopt,
      task_env_.GetMockTickClock(), audio_thread_.task_runner(),
      task_env_.GetMainThreadTaskRunner());
  SetHangActionCallbacksForTesting();
  RunUntilIdle();

  // The late check doesn't update the last check time, so all following
  // checks are late too and the hang isn't detected.
  EXPECT_CALL(*this, HangActionTerminate).Times(0);

  SimulateUnansweredLateCheckThenResume();

  BlockAudioThreadUntilEvent();
  task_env_.FastForwardBy(base::Minutes(10));
  event_.Signal();

  EXPECT_THAT(histograms_.GetAllSamples("Media.AudioThreadStatus"),
              ElementsAre(base::Bucket(kStarted, 2)));
}

TEST_F(AudioThreadHangMonitorRearmDisabledTest,
       PingsAudioThreadAgainWhilePingIsPending) {
  // |audio_task_runner| never runs its tasks, so all pings stay pending.
  auto audio_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  hang_monitor_ = AudioThreadHangMonitor::Create(
      HangAction::kDoNothing, std::nullopt, task_env_.GetMockTickClock(),
      audio_task_runner, task_env_.GetMainThreadTaskRunner());
  RunUntilIdle();

  task_env_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(audio_task_runner->NumPendingTasks(), 3u);
}

}  // namespace media
