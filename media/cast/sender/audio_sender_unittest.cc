// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/test/utility/audio_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {

namespace {

void SaveOperationalStatus(OperationalStatus* out_status,
                           OperationalStatus in_status) {
  DVLOG(1) << "OperationalStatus transitioning from " << *out_status << " to "
           << in_status;
  *out_status = in_status;
}

class TransportClient : public CastTransport::Client {
 public:
  TransportClient() = default;

  TransportClient(const TransportClient&) = delete;
  TransportClient& operator=(const TransportClient&) = delete;

  void OnStatusChanged(CastTransportStatus status) final {
    EXPECT_EQ(TRANSPORT_STREAM_INITIALIZED, status);
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {}
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {}
};

}  // namespace

class TestPacketSender : public PacketTransport {
 public:
  TestPacketSender() : number_of_rtp_packets_(0), number_of_rtcp_packets_(0) {}

  TestPacketSender(const TestPacketSender&) = delete;
  TestPacketSender& operator=(const TestPacketSender&) = delete;

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final {
    if (IsRtcpPacket(&packet->data[0], packet->data.size())) {
      ++number_of_rtcp_packets_;
    } else {
      // Check that at least one RTCP packet was sent before the first RTP
      // packet.  This confirms that the receiver will have the necessary lip
      // sync info before it has to calculate the playout time of the first
      // frame.
      if (number_of_rtp_packets_ == 0)
        EXPECT_LE(1, number_of_rtcp_packets_);
      ++number_of_rtp_packets_;
    }
    return true;
  }

  int64_t GetBytesSent() final { return 0; }

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  int number_of_rtp_packets() const { return number_of_rtp_packets_; }

  int number_of_rtcp_packets() const { return number_of_rtcp_packets_; }

 private:
  int number_of_rtp_packets_;
  int number_of_rtcp_packets_;
};

class AudioSenderTest : public ::testing::Test {
 protected:
  AudioSenderTest() {
    InitializeMediaLibrary();
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
    task_runner_ = new FakeSingleThreadTaskRunner(&testing_clock_);
    cast_environment_ = new CastEnvironment(&testing_clock_, task_runner_,
                                            task_runner_, task_runner_);
    audio_config_.codec = CODEC_AUDIO_OPUS;
    audio_config_.use_hardware_encoder = false;
    audio_config_.rtp_timebase = kDefaultAudioSamplingRate;
    audio_config_.channels = 2;
    audio_config_.max_bitrate = kDefaultAudioEncoderBitrate;
    audio_config_.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;

    transport_ = new TestPacketSender();
    transport_sender_ = std::make_unique<CastTransportImpl>(
        &testing_clock_, base::TimeDelta(), std::make_unique<TransportClient>(),
        base::WrapUnique(transport_.get()), task_runner_);
    OperationalStatus operational_status = STATUS_UNINITIALIZED;
    audio_sender_ = std::make_unique<AudioSender>(
        cast_environment_, audio_config_,
        base::BindOnce(&SaveOperationalStatus, &operational_status),
        transport_sender_.get());
    task_runner_->RunTasks();
    CHECK_EQ(STATUS_INITIALIZED, operational_status);
  }

  ~AudioSenderTest() override = default;

  base::SimpleTestTickClock testing_clock_;
  raw_ptr<TestPacketSender> transport_;  // Owned by CastTransport.
  std::unique_ptr<CastTransportImpl> transport_sender_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  std::unique_ptr<AudioSender> audio_sender_;
  scoped_refptr<CastEnvironment> cast_environment_;
  FrameSenderConfig audio_config_;
};

TEST_F(AudioSenderTest, Encode20ms) {
  const base::TimeDelta kDuration = base::Milliseconds(20);
  std::unique_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels, audio_config_.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration));

  audio_sender_->InsertAudio(std::move(bus), testing_clock_.NowTicks());
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_->number_of_rtp_packets());
  EXPECT_LE(1, transport_->number_of_rtcp_packets());
}

TEST_F(AudioSenderTest, RtcpTimer) {
  const base::TimeDelta kDuration = base::Milliseconds(20);
  std::unique_ptr<AudioBus> bus(
      TestAudioBusFactory(audio_config_.channels, audio_config_.rtp_timebase,
                          TestAudioBusFactory::kMiddleANoteFreq, 0.5f)
          .NextAudioBus(kDuration));

  audio_sender_->InsertAudio(std::move(bus), testing_clock_.NowTicks());
  task_runner_->RunTasks();

  // Make sure that we send at least one RTCP packet.
  base::TimeDelta max_rtcp_timeout =
      base::Milliseconds(1) + kRtcpReportInterval * 3 / 2;
  testing_clock_.Advance(max_rtcp_timeout);
  task_runner_->RunTasks();
  EXPECT_LE(1, transport_->number_of_rtp_packets());
  EXPECT_LE(1, transport_->number_of_rtcp_packets());
}

}  // namespace media::cast
