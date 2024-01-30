// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_thread_hang_monitor.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
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
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        histograms_(),
        audio_thread_("Audio thread"),
        hang_monitor_({nullptr, base::OnTaskRunnerDeleter(nullptr)}) {
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
    audio_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::WaitableEvent::Wait, base::Unretained(&event_)));
  }

  MOCK_METHOD0(HangActionDump, void());
  MOCK_METHOD0(HangActionTerminate, void());

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

}  // namespace media
