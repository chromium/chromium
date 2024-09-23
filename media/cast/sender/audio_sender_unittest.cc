// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/audio_codecs.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/constants.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"
#include "third_party/openscreen/src/platform/base/trivial_clock_traits.h"

using testing::_;

namespace media::cast {

namespace {

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

}  // namespace

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest()
      : task_runner_(
            base::MakeRefCounted<FakeSingleThreadTaskRunner>(&testing_clock_)),
        cast_environment_(base::MakeRefCounted<CastEnvironment>(&testing_clock_,
                                                                task_runner_,
                                                                task_runner_,
                                                                task_runner_)),
        openscreen_task_runner_(task_runner_) {
    FakeOpenscreenClock::SetTickClock(&testing_clock_);
    InitializeMediaLibrary();
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());

    mock_openscreen_environment_ = std::make_unique<MockOpenscreenEnvironment>(
        &FakeOpenscreenClock::now, openscreen_task_runner_);
    openscreen_packet_router_ =
        std::make_unique<openscreen::cast::SenderPacketRouter>(
            *mock_openscreen_environment_);

    audio_config_.sender_ssrc = 35535;
    audio_config_.receiver_ssrc = 35536;
    audio_config_.audio_codec_params =
        AudioCodecParams{.codec = AudioCodec::kOpus};
    audio_config_.use_hardware_encoder = false;
    audio_config_.rtp_timebase = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.max_bitrate = kDefaultAudioEncoderBitrate;
    audio_config_.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;

    openscreen::cast::SessionConfig openscreen_audio_config =
        ToOpenscreenSessionConfig(audio_config_, /* is_pli_enabled= */ true);

    auto openscreen_audio_sender = std::make_unique<openscreen::cast::Sender>(
        *mock_openscreen_environment_, *openscreen_packet_router_,
        openscreen_audio_config, openscreen::cast::RtpPayloadType::kAudioOpus);
    openscreen_audio_sender_ = openscreen_audio_sender.get();

    OperationalStatus operational_status = STATUS_UNINITIALIZED;
    audio_sender_ = std::make_unique<AudioSender>(
        cast_environment_, audio_config_,
        base::BindOnce(&SaveOperationalStatus, &operational_status),
        std::move(openscreen_audio_sender));
    task_runner_->RunTasks();
    CHECK_EQ(STATUS_INITIALIZED, operational_status);
  }

  ~AudioSenderTest() override {
    FakeOpenscreenClock::ClearTickClock();
    openscreen_audio_sender_ = nullptr;
  }

  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  // openscreen::Sender related classes.
  openscreen_platform::TaskRunner openscreen_task_runner_;
  std::unique_ptr<media::cast::MockOpenscreenEnvironment>
      mock_openscreen_environment_;
  std::unique_ptr<openscreen::cast::SenderPacketRouter>
      openscreen_packet_router_;
  FrameSenderConfig audio_config_;
  std::unique_ptr<AudioSender> audio_sender_;
  // Unowned pointer to the openscreen::cast::Sender.
  raw_ptr<openscreen::cast::Sender> openscreen_audio_sender_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  const base::TimeDelta kDuration = base::Milliseconds(20);
  std::unique_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels, audio_config_.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration));

  EXPECT_CALL(*mock_openscreen_environment_, SendPacket(_, _)).Times(3);

  audio_sender_->InsertAudio(std::move(bus), testing_clock_.NowTicks());
  task_runner_->RunTasks();
  EXPECT_EQ(2, openscreen_audio_sender_->GetInFlightFrameCount());
}

}  // namespace media::cast
