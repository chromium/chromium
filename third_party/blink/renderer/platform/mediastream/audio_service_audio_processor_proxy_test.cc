// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/audio_service_audio_processor_proxy.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_processor_controls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/media_stream_interface.h"

using ::testing::_;
using ::testing::StrictMock;

namespace blink {

namespace {
void VerifyStats(const media::AudioProcessingStats& expected,
                 scoped_refptr<AudioServiceAudioProcessorProxy> proxy) {
  webrtc::AudioProcessorInterface::AudioProcessorStatistics received =
      proxy->GetStats(false);
  EXPECT_FALSE(received.typing_noise_detected);
  EXPECT_EQ(received.apm_statistics.echo_return_loss,
            expected.echo_return_loss);
  EXPECT_EQ(received.apm_statistics.echo_return_loss_enhancement,
            expected.echo_return_loss_enhancement);
  EXPECT_FALSE(received.apm_statistics.voice_detected);
  EXPECT_FALSE(received.apm_statistics.divergent_filter_fraction);
  EXPECT_FALSE(received.apm_statistics.delay_median_ms);
  EXPECT_FALSE(received.apm_statistics.delay_standard_deviation_ms);
  EXPECT_FALSE(received.apm_statistics.residual_echo_likelihood);
  EXPECT_FALSE(received.apm_statistics.residual_echo_likelihood_recent_max);
  EXPECT_FALSE(received.apm_statistics.delay_ms);
}

void VerifyStatsFromAnotherThread(
    const media::AudioProcessingStats& expected,
    scoped_refptr<AudioServiceAudioProcessorProxy> proxy) {
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {}, base::BindOnce(&VerifyStats, expected, proxy),
      run_loop.QuitClosure());
  run_loop.Run();
}

void MaybeSetNumChannelsOnAnotherThread(
    scoped_refptr<AudioServiceAudioProcessorProxy> proxy,
    uint32_t num_channels) {
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {},
      base::BindOnce(&AudioServiceAudioProcessorProxy::
                         MaybeUpdateNumPreferredCaptureChannels,
                     proxy, num_channels),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class MockAudioProcessorControls : public media::AudioProcessorControls {
 public:
  void SetStats(const media::AudioProcessingStats& stats) {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    stats_ = stats;
  }

  void GetStats(GetStatsCB callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    std::move(callback).Run(stats_);
  }

  // Set preferred number of microphone channels.
  void SetPreferredNumCaptureChannels(int32_t num_preferred_channels) override {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    SetPreferredNumCaptureChannelsCalled(num_preferred_channels);
  }

  MOCK_METHOD1(SetPreferredNumCaptureChannelsCalled, void(int32_t));

 private:
  media::AudioProcessingStats stats_;
  THREAD_CHECKER(main_thread_checker_);
};

class AudioServiceAudioProcessorProxyTest : public testing::Test {
 protected:
  void AdvanceUntilStatsUpdate() {
    task_environment_.FastForwardBy(
        AudioServiceAudioProcessorProxy::kStatsUpdateInterval +
        base::Seconds(1));
  }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AudioServiceAudioProcessorProxyTest, SafeIfNoControls) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();
  VerifyStats(media::AudioProcessingStats(), proxy);
  proxy->MaybeUpdateNumPreferredCaptureChannels(2);
}

TEST_F(AudioServiceAudioProcessorProxyTest, StopDetachesFromControls) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();

  StrictMock<MockAudioProcessorControls> controls;

  proxy->SetControls(&controls);
  proxy->Stop();

  // |proxy| should not poll |controls|.
  AdvanceUntilStatsUpdate();
}

TEST_F(AudioServiceAudioProcessorProxyTest, StatsUpdatedOnTimer) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();
  StrictMock<MockAudioProcessorControls> controls;
  media::AudioProcessingStats stats1{4, 5};
  controls.SetStats(stats1);

  proxy->SetControls(&controls);

  VerifyStatsFromAnotherThread(media::AudioProcessingStats(), proxy);

  AdvanceUntilStatsUpdate();
  VerifyStatsFromAnotherThread(stats1, proxy);

  media::AudioProcessingStats stats2{7, 8};
  controls.SetStats(stats2);
  AdvanceUntilStatsUpdate();
  VerifyStatsFromAnotherThread(stats2, proxy);
}

TEST_F(AudioServiceAudioProcessorProxyTest, SetNumChannelsIfIncreases) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();
  StrictMock<MockAudioProcessorControls> controls;
  EXPECT_CALL(controls, SetPreferredNumCaptureChannelsCalled(2));
  EXPECT_CALL(controls, SetPreferredNumCaptureChannelsCalled(3));

  proxy->SetControls(&controls);

  MaybeSetNumChannelsOnAnotherThread(proxy, 2);
  MaybeSetNumChannelsOnAnotherThread(proxy, 3);
  task_environment_.RunUntilIdle();
}

TEST_F(AudioServiceAudioProcessorProxyTest,
       DoesNotSetNumChannelsIfDoesNotChange) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();
  StrictMock<MockAudioProcessorControls> controls;
  EXPECT_CALL(controls, SetPreferredNumCaptureChannelsCalled(2)).Times(1);

  proxy->SetControls(&controls);

  MaybeSetNumChannelsOnAnotherThread(proxy, 2);
  MaybeSetNumChannelsOnAnotherThread(proxy, 2);
  task_environment_.RunUntilIdle();
}

TEST_F(AudioServiceAudioProcessorProxyTest, DoesNotSetNumChannelsIfDecreases) {
  scoped_refptr<AudioServiceAudioProcessorProxy> proxy =
      new rtc::RefCountedObject<AudioServiceAudioProcessorProxy>();
  StrictMock<MockAudioProcessorControls> controls;
  EXPECT_CALL(controls, SetPreferredNumCaptureChannelsCalled(3)).Times(1);

  proxy->SetControls(&controls);

  MaybeSetNumChannelsOnAnotherThread(proxy, 3);
  MaybeSetNumChannelsOnAnotherThread(proxy, 2);
  task_environment_.RunUntilIdle();
}

}  // namespace blink
