// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This test generate synthetic data. For audio it's a sinusoid waveform with
// frequency kSoundFrequency and different amplitudes. For video it's a pattern
// that is shifting by one pixel per frame, each pixels neighbors right and down
// is this pixels value +1, since the pixel value is 8 bit it will wrap
// frequently within the image. Visually this will create diagonally color bands
// that moves across the screen

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/test/skewed_single_thread_task_runner.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

static const int64_t kStartMillisecond = INT64_C(1245);
static const int kAudioChannels = 2;
static const double kSoundFrequency = 314.15926535897;  // Freq of sine wave.
static const float kSoundVolume = 0.5f;
static const int kVideoWidth = 320;
static const int kVideoHeight = 180;

// Since the video encoded and decoded an error will be introduced; when
// comparing individual pixels the error can be quite large; we allow a PSNR of
// at least |kVideoAcceptedPSNR|.
static const double kVideoAcceptedPSNR = 38.0;

// The tests are commonly implemented with |kFrameTimerMs| RunTask function;
// a normal video is 30 fps hence the 33 ms between frames.
//
// TODO(miu): The errors in timing will add up significantly.  Find an
// alternative approach that eliminates use of this constant.
static const int kFrameTimerMs = 33;

// The size of audio frames.  The encoder joins/breaks all inserted audio into
// chunks of this size.
static const int kAudioFrameDurationMs = 10;

// The amount of time between frame capture on the sender and playout on the
// receiver.
static const int kTargetPlayoutDelayMs = 100;

// The maximum amount of deviation expected in the playout times emitted by the
// receiver.
static const int kMaxAllowedPlayoutErrorMs = 30;

std::string ConvertFromBase16String(const std::string& base_16) {
  std::string compressed;
  DCHECK_EQ(base_16.size() % 2, 0u) << "Must be a multiple of 2";
  compressed.reserve(base_16.size() / 2);

  if (!base::HexStringToString(base_16, &compressed)) {
    NOTREACHED();
  }
  return compressed;
}

void ExpectSuccessOperationalStatus(OperationalStatus status) {
  EXPECT_EQ(STATUS_INITIALIZED, status);
}

// This is wrapped in a struct because it needs to be put into a std::map.
typedef struct { int counter[kNumOfLoggingEvents]; } LoggingEventCounts;

// Constructs a map from each frame (RTP timestamp) to counts of each event
// type logged for that frame.
std::map<RtpTimeTicks, LoggingEventCounts> GetEventCountsForFrameEvents(
    const std::vector<FrameEvent>& frame_events) {
  std::map<RtpTimeTicks, LoggingEventCounts> event_counter_for_frame;
  for (const FrameEvent& frame_event : frame_events) {
    auto map_it = event_counter_for_frame.find(frame_event.rtp_timestamp);
    if (map_it == event_counter_for_frame.end()) {
      LoggingEventCounts new_counter;
      memset(&new_counter, 0, sizeof(new_counter));
      ++(new_counter.counter[frame_event.type]);
      event_counter_for_frame.insert(
          std::make_pair(frame_event.rtp_timestamp, new_counter));
    } else {
      ++(map_it->second.counter[frame_event.type]);
    }
  }
  return event_counter_for_frame;
}

// Constructs a map from each packet (Packet ID) to counts of each event
// type logged for that packet.
std::map<uint16_t, LoggingEventCounts> GetEventCountsForPacketEvents(
    const std::vector<PacketEvent>& packet_events) {
  std::map<uint16_t, LoggingEventCounts> event_counter_for_packet;
  for (const PacketEvent& packet_event : packet_events) {
    auto map_it = event_counter_for_packet.find(packet_event.packet_id);
    if (map_it == event_counter_for_packet.end()) {
      LoggingEventCounts new_counter;
      memset(&new_counter, 0, sizeof(new_counter));
      ++(new_counter.counter[packet_event.type]);
      event_counter_for_packet.insert(
          std::make_pair(packet_event.packet_id, new_counter));
    } else {
      ++(map_it->second.counter[packet_event.type]);
    }
  }
  return event_counter_for_packet;
}

// Shim that turns forwards packets from a test::PacketPipe to a
// PacketReceiverCallback.
class LoopBackPacketPipe : public test::PacketPipe {
 public:
  explicit LoopBackPacketPipe(const PacketReceiverCallback& packet_receiver)
      : packet_receiver_(packet_receiver) {}

  ~LoopBackPacketPipe() final = default;

  // PacketPipe implementations.
  void Send(std::unique_ptr<Packet> packet) final {
    packet_receiver_.Run(std::move(packet));
  }

 private:
  PacketReceiverCallback packet_receiver_;
};

// Class that sends the packet direct from sender into the receiver with the
// ability to drop packets between the two.
//
// TODO(miu): This should be reconciled/merged into
// media/cast/test/loopback_transport.*.  It's roughly the same class and has
// exactly the same name (and when it was outside of the anonymous namespace bad
// things happened when linking on Android!).
class LoopBackTransport : public PacketTransport {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment)
      : send_packets_(true),
        drop_packets_belonging_to_odd_frames_(false),
        cast_environment_(cast_environment),
        bytes_sent_(0) {}

  void SetPacketReceiver(
      const PacketReceiverCallback& packet_receiver,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock) {
    std::unique_ptr<test::PacketPipe> loopback_pipe(
        new LoopBackPacketPipe(packet_receiver));
    if (packet_pipe_) {
      packet_pipe_->AppendToPipe(std::move(loopback_pipe));
    } else {
      packet_pipe_ = std::move(loopback_pipe);
    }
    packet_pipe_->InitOnIOThread(task_runner, clock);
  }

  bool SendPacket(PacketRef packet, const base::Closure& cb) final {
    DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
    if (!send_packets_)
      return true;

    bytes_sent_ += packet->data.size();
    if (drop_packets_belonging_to_odd_frames_) {
      const uint8_t truncated_frame_id = packet->data[13];
      if (truncated_frame_id % 2 == 1)
        return true;
    }

    std::unique_ptr<Packet> packet_copy(new Packet(packet->data));
    packet_pipe_->Send(std::move(packet_copy));
    return true;
  }

  int64_t GetBytesSent() final { return bytes_sent_; }

  void StartReceiving(
      const PacketReceiverCallbackWithStatus& packet_receiver) final {}

  void StopReceiving() final {}

  void SetSendPackets(bool send_packets) { send_packets_ = send_packets; }

  void DropAllPacketsBelongingToOddFrames() {
    drop_packets_belonging_to_odd_frames_ = true;
  }

  void SetPacketPipe(std::unique_ptr<test::PacketPipe> pipe) {
    // Append the loopback pipe to the end.
    pipe->AppendToPipe(std::move(packet_pipe_));
    packet_pipe_ = std::move(pipe);
  }

 private:
  bool send_packets_;
  bool drop_packets_belonging_to_odd_frames_;
  scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<test::PacketPipe> packet_pipe_;
  int64_t bytes_sent_;
};

// Class that verifies the audio frames coming out of the receiver.
class TestReceiverAudioCallback
    : public base::RefCountedThreadSafe<TestReceiverAudioCallback> {
 public:
  struct ExpectedAudioFrame {
    std::unique_ptr<AudioBus> audio_bus;
    base::TimeTicks playout_time;
  };

  TestReceiverAudioCallback() : num_called_(0) {}

  void SetExpectedSamplingFrequency(int expected_sampling_frequency) {
    expected_sampling_frequency_ = expected_sampling_frequency;
  }

  void AddExpectedResult(const AudioBus& audio_bus,
                         base::TimeTicks playout_time) {
    std::unique_ptr<ExpectedAudioFrame> expected_audio_frame(
        new ExpectedAudioFrame());
    expected_audio_frame->audio_bus =
        AudioBus::Create(audio_bus.channels(), audio_bus.frames());
    audio_bus.CopyTo(expected_audio_frame->audio_bus.get());
    expected_audio_frame->playout_time = playout_time;
    expected_frames_.push_back(std::move(expected_audio_frame));
  }

  void IgnoreAudioFrame(std::unique_ptr<AudioBus> audio_bus,
                        base::TimeTicks playout_time,
                        bool is_continuous) {
    ++num_called_;
  }

  void CheckAudioFrame(std::unique_ptr<AudioBus> audio_bus,
                       base::TimeTicks playout_time,
                       bool is_continuous) {
    ++num_called_;

    ASSERT_TRUE(audio_bus);
    ASSERT_FALSE(expected_frames_.empty());
    const std::unique_ptr<ExpectedAudioFrame> expected_audio_frame =
        std::move(expected_frames_.front());
    expected_frames_.pop_front();

    EXPECT_EQ(audio_bus->channels(), kAudioChannels);
    EXPECT_EQ(audio_bus->frames(), expected_audio_frame->audio_bus->frames());
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      EXPECT_NEAR(CountZeroCrossings(
                      expected_audio_frame->audio_bus->channel(ch),
                      expected_audio_frame->audio_bus->frames()),
                  CountZeroCrossings(audio_bus->channel(ch),
                                     audio_bus->frames()),
                  1);
    }

    EXPECT_NEAR(
        (playout_time - expected_audio_frame->playout_time).InMillisecondsF(),
        0.0,
        kMaxAllowedPlayoutErrorMs);
    VLOG_IF(1, !last_playout_time_.is_null())
        << "Audio frame playout time delta (compared to last frame) is "
        << (playout_time - last_playout_time_).InMicroseconds() << " usec.";
    last_playout_time_ = playout_time;

    EXPECT_TRUE(is_continuous);
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverAudioCallback() = default;

 private:
  friend class base::RefCountedThreadSafe<TestReceiverAudioCallback>;

  int num_called_;
  int expected_sampling_frequency_;
  std::list<std::unique_ptr<ExpectedAudioFrame>> expected_frames_;
  base::TimeTicks last_playout_time_;
};

// Class that verifies the video frames coming out of the receiver.
class TestReceiverVideoCallback
    : public base::RefCountedThreadSafe<TestReceiverVideoCallback> {
 public:
  struct ExpectedVideoFrame {
    int frame_number;
    gfx::Size size;
    base::TimeTicks playout_time;
    bool should_be_continuous;
  };

  TestReceiverVideoCallback() : num_called_(0) {}

  void AddExpectedResult(int frame_number,
                         const gfx::Size& size,
                         base::TimeTicks playout_time,
                         bool should_be_continuous) {
    ExpectedVideoFrame expected_video_frame;
    expected_video_frame.frame_number = frame_number;
    expected_video_frame.size = size;
    expected_video_frame.playout_time = playout_time;
    expected_video_frame.should_be_continuous = should_be_continuous;
    expected_frame_.push_back(expected_video_frame);
  }

  void CheckVideoFrame(bool examine_content,
                       scoped_refptr<media::VideoFrame> video_frame,
                       base::TimeTicks playout_time,
                       bool is_continuous) {
    ++num_called_;

    ASSERT_TRUE(video_frame);
    ASSERT_FALSE(expected_frame_.empty());
    ExpectedVideoFrame expected_video_frame = expected_frame_.front();
    expected_frame_.pop_front();

    EXPECT_EQ(expected_video_frame.size.width(),
              video_frame->visible_rect().width());
    EXPECT_EQ(expected_video_frame.size.height(),
              video_frame->visible_rect().height());

    if (examine_content && expected_video_frame.should_be_continuous) {
      scoped_refptr<media::VideoFrame> expected_I420_frame =
          media::VideoFrame::CreateFrame(
              PIXEL_FORMAT_I420, expected_video_frame.size,
              gfx::Rect(expected_video_frame.size), expected_video_frame.size,
              base::TimeDelta());
      PopulateVideoFrame(expected_I420_frame.get(),
                         expected_video_frame.frame_number);
      EXPECT_LE(kVideoAcceptedPSNR,
                I420PSNR(*expected_I420_frame, *video_frame));
    }

    EXPECT_NEAR(
        (playout_time - expected_video_frame.playout_time).InMillisecondsF(),
        0.0,
        kMaxAllowedPlayoutErrorMs);
    VLOG_IF(1, !last_playout_time_.is_null())
        << "Video frame playout time delta (compared to last frame) is "
        << (playout_time - last_playout_time_).InMicroseconds() << " usec.";
    last_playout_time_ = playout_time;

    EXPECT_EQ(expected_video_frame.should_be_continuous, is_continuous);
  }

  int number_times_called() const { return num_called_; }

 protected:
  virtual ~TestReceiverVideoCallback() = default;

 private:
  friend class base::RefCountedThreadSafe<TestReceiverVideoCallback>;

  int num_called_;
  std::list<ExpectedVideoFrame> expected_frame_;
  base::TimeTicks last_playout_time_;
};

}  // namespace

// The actual test class, generate synthetic data for both audio and video and
// send those through the sender and receiver and analyzes the result.
class End2EndTest : public ::testing::Test {
 public:
  void ReceivePacket(std::unique_ptr<media::cast::Packet> packet) {
    cast_receiver_->ReceivePacket(std::move(packet));
  }

 protected:
  End2EndTest()
      : start_time_(),
        task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        testing_clock_sender_(&testing_clock_),
        task_runner_sender_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        testing_clock_receiver_(&testing_clock_),
        task_runner_receiver_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        cast_environment_sender_(new CastEnvironment(&testing_clock_sender_,
                                                     task_runner_sender_,
                                                     task_runner_sender_,
                                                     task_runner_sender_)),
        cast_environment_receiver_(new CastEnvironment(&testing_clock_receiver_,
                                                       task_runner_receiver_,
                                                       task_runner_receiver_,
                                                       task_runner_receiver_)),
        receiver_to_sender_(new LoopBackTransport(cast_environment_receiver_)),
        sender_to_receiver_(new LoopBackTransport(cast_environment_sender_)),
        test_receiver_audio_callback_(new TestReceiverAudioCallback()),
        test_receiver_video_callback_(new TestReceiverVideoCallback()) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    cast_environment_sender_->logger()->Subscribe(&event_subscriber_sender_);
  }

  void Configure(Codec video_codec, Codec audio_codec) {
    audio_sender_config_.sender_ssrc = 1;
    audio_sender_config_.receiver_ssrc = 2;
    audio_sender_config_.max_playout_delay =
        base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    audio_sender_config_.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;
    audio_sender_config_.use_external_encoder = false;
    audio_sender_config_.rtp_timebase = kDefaultAudioSamplingRate;
    audio_sender_config_.channels = kAudioChannels;
    audio_sender_config_.max_bitrate = kDefaultAudioEncoderBitrate;
    audio_sender_config_.codec = audio_codec;
    audio_sender_config_.aes_iv_mask =
        ConvertFromBase16String("abcdeffedcba12345678900987654321");
    audio_sender_config_.aes_key =
        ConvertFromBase16String("deadbeefcafecafedeadbeefb0b0b0b0");

    audio_receiver_config_.receiver_ssrc =
        audio_sender_config_.receiver_ssrc;
    audio_receiver_config_.sender_ssrc = audio_sender_config_.sender_ssrc;
    audio_receiver_config_.rtp_max_delay_ms = kTargetPlayoutDelayMs;
    audio_receiver_config_.rtp_payload_type =
        audio_sender_config_.rtp_payload_type;
    audio_receiver_config_.rtp_timebase = audio_sender_config_.rtp_timebase;
    audio_receiver_config_.channels = kAudioChannels;
    audio_receiver_config_.target_frame_rate = 100;
    audio_receiver_config_.codec = audio_sender_config_.codec;
    audio_receiver_config_.aes_iv_mask = audio_sender_config_.aes_iv_mask;
    audio_receiver_config_.aes_key = audio_sender_config_.aes_key;

    test_receiver_audio_callback_->SetExpectedSamplingFrequency(
        audio_receiver_config_.rtp_timebase);

    video_sender_config_.sender_ssrc = 3;
    video_sender_config_.receiver_ssrc = 4;
    video_sender_config_.max_playout_delay =
        base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    video_sender_config_.rtp_payload_type = RtpPayloadType::VIDEO_VP8;
    video_sender_config_.use_external_encoder = false;
    video_sender_config_.rtp_timebase = kVideoFrequency;
    video_sender_config_.max_bitrate = 50000;
    video_sender_config_.min_bitrate = 10000;
    video_sender_config_.start_bitrate = 10000;
    video_sender_config_.video_codec_params.max_qp = 30;
    video_sender_config_.video_codec_params.min_qp = 4;
    video_sender_config_.max_frame_rate = 30;
    video_sender_config_.codec = video_codec;
    video_sender_config_.aes_iv_mask =
        ConvertFromBase16String("1234567890abcdeffedcba0987654321");
    video_sender_config_.aes_key =
        ConvertFromBase16String("deadbeefcafeb0b0b0b0cafedeadbeef");

    video_receiver_config_.receiver_ssrc =
        video_sender_config_.receiver_ssrc;
    video_receiver_config_.sender_ssrc = video_sender_config_.sender_ssrc;
    video_receiver_config_.rtp_max_delay_ms = kTargetPlayoutDelayMs;
    video_receiver_config_.rtp_payload_type =
        video_sender_config_.rtp_payload_type;
    video_receiver_config_.rtp_timebase = kVideoFrequency;
    video_receiver_config_.channels = 1;
    video_receiver_config_.target_frame_rate =
        video_sender_config_.max_frame_rate;
    video_receiver_config_.codec = video_sender_config_.codec;
    video_receiver_config_.aes_iv_mask = video_sender_config_.aes_iv_mask;
    video_receiver_config_.aes_key = video_sender_config_.aes_key;
  }

  void SetReceiverSkew(double skew, base::TimeDelta offset) {
    testing_clock_receiver_.SetSkew(skew, offset);
    task_runner_receiver_->SetSkew(1.0 / skew);
  }

  // Specify the minimum/maximum difference in playout times between two
  // consecutive frames.  Also, specify the maximum absolute rate of change over
  // each three consecutive frames.
  void SetExpectedVideoPlayoutSmoothness(base::TimeDelta min_delta,
                                         base::TimeDelta max_delta,
                                         base::TimeDelta max_curvature) {
    min_video_playout_delta_ = min_delta;
    max_video_playout_delta_ = max_delta;
    max_video_playout_curvature_ = max_curvature;
  }

  void FeedAudioFrames(int count, bool will_be_checked) {
    for (int i = 0; i < count; ++i) {
      std::unique_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks reference_time =
          testing_clock_sender_.NowTicks() +
          i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      if (will_be_checked) {
        test_receiver_audio_callback_->AddExpectedResult(
            *audio_bus,
            reference_time +
                base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs));
      }
      audio_frame_input_->InsertAudio(std::move(audio_bus), reference_time);
    }
  }

  void FeedAudioFramesWithExpectedDelay(int count, base::TimeDelta delay) {
    for (int i = 0; i < count; ++i) {
      std::unique_ptr<AudioBus> audio_bus(audio_bus_factory_->NextAudioBus(
          base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs)));
      const base::TimeTicks reference_time =
          testing_clock_sender_.NowTicks() +
          i * base::TimeDelta::FromMilliseconds(kAudioFrameDurationMs);
      test_receiver_audio_callback_->AddExpectedResult(
          *audio_bus,
          reference_time + delay +
              base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs));
      audio_frame_input_->InsertAudio(std::move(audio_bus), reference_time);
    }
  }

  void RequestAudioFrames(int count, bool with_check) {
    for (int i = 0; i < count; ++i) {
      cast_receiver_->RequestDecodedAudioFrame(
          base::Bind(with_check ? &TestReceiverAudioCallback::CheckAudioFrame :
                                  &TestReceiverAudioCallback::IgnoreAudioFrame,
                     test_receiver_audio_callback_));
    }
  }

  void Create();

  ~End2EndTest() override {
    cast_environment_sender_->logger()->Unsubscribe(&event_subscriber_sender_);
  }

  void TearDown() final {
    cast_sender_.reset();
    cast_receiver_.reset();
    task_runner_->RunTasks();
  }

  gfx::Size GetTestVideoFrameSize() const {
    if (video_sender_config_.codec == CODEC_VIDEO_FAKE)
      return gfx::Size(2, 2);
    else
      return gfx::Size(kVideoWidth, kVideoHeight);
  }

  void SendVideoFrame(int frame_number, base::TimeTicks reference_time) {
    if (start_time_.is_null())
      start_time_ = reference_time;
    const base::TimeDelta time_diff = reference_time - start_time_;
    scoped_refptr<media::VideoFrame> video_frame;
    if (video_sender_config_.codec == CODEC_VIDEO_FAKE) {
      video_frame =
          media::VideoFrame::CreateBlackFrame(GetTestVideoFrameSize());
    } else {
      const gfx::Size size = GetTestVideoFrameSize();
      video_frame = media::VideoFrame::CreateFrame(
          PIXEL_FORMAT_I420, size, gfx::Rect(size), size, time_diff);
      PopulateVideoFrame(video_frame.get(), frame_number);
    }
    video_frame->set_timestamp(reference_time - start_time_);
    video_frame_input_->InsertRawVideoFrame(video_frame, reference_time);
  }

  void RunTasks(int ms) {
    task_runner_->Sleep(base::TimeDelta::FromMilliseconds(ms));
  }

  // Send and receive audio and video frames for the given |duration|.  Returns
  // the total number of audio and video frames sent.
  std::pair<int, int> RunAudioVideoLoop(base::TimeDelta duration) {
    base::TimeTicks next_video_frame_at = testing_clock_.NowTicks();
    base::TimeTicks video_reference_time;
    int audio_frames_sent = 0;
    int video_frames_sent = 0;
    const base::TimeTicks end_time = testing_clock_.NowTicks() + duration;
    while (testing_clock_.NowTicks() < end_time) {
      // Opus introduces a tiny delay before the sinewave starts; so don't
      // examine the first audio frame's data receiver-side.
      const bool verify_audio_data =
          audio_frames_sent > 0 ||
          audio_sender_config_.codec == CODEC_AUDIO_PCM16;
      FeedAudioFrames(1, verify_audio_data);
      ++audio_frames_sent;

      const bool send_and_receive_a_video_frame =
          testing_clock_.NowTicks() >= next_video_frame_at;
      if (send_and_receive_a_video_frame) {
        video_reference_time = next_video_frame_at;
        next_video_frame_at += base::TimeDelta::FromMilliseconds(kFrameTimerMs);
        test_receiver_video_callback_->AddExpectedResult(
            video_frames_sent, GetTestVideoFrameSize(),
            testing_clock_.NowTicks() +
                base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
            true);
        SendVideoFrame(video_frames_sent, video_reference_time);
        ++video_frames_sent;
      }

      RunTasks(kAudioFrameDurationMs);

      RequestAudioFrames(1, verify_audio_data);
      if (send_and_receive_a_video_frame) {
        cast_receiver_->RequestDecodedVideoFrame(
            base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                       test_receiver_video_callback_,
                       video_sender_config_.codec != CODEC_VIDEO_FAKE));
      }
    }

    // Verify all audio and video frames were received.
    RunTasks(kFrameTimerMs + kTargetPlayoutDelayMs);  // Let the data flow.
    EXPECT_EQ(audio_frames_sent,
              test_receiver_audio_callback_->number_times_called());
    EXPECT_EQ(video_frames_sent,
              test_receiver_video_callback_->number_times_called());

    return std::make_pair(audio_frames_sent, video_frames_sent);
  }

  // Queries the EventSubscriber for all accumulated frame and packet events for
  // audio and video and verifies all logging information was captured
  // correctly.
  void VerifyLogging(int num_expected_audio_frames,
                     int num_expected_video_frames) {
    // Partition the frame and packet events into separate vectors for audio
    // versus video.
    std::vector<FrameEvent> all_frame_events;
    event_subscriber_sender_.GetFrameEventsAndReset(&all_frame_events);
    std::vector<FrameEvent> audio_frame_events;
    std::vector<FrameEvent> video_frame_events;
    for (const FrameEvent& event : all_frame_events) {
      switch (event.media_type) {
        case AUDIO_EVENT:
          audio_frame_events.push_back(event);
          break;
        case VIDEO_EVENT:
          video_frame_events.push_back(event);
          break;
        default:
          FAIL();
          return;
      }
    }
    std::vector<PacketEvent> all_packet_events;
    event_subscriber_sender_.GetPacketEventsAndReset(&all_packet_events);
    std::vector<PacketEvent> audio_packet_events;
    std::vector<PacketEvent> video_packet_events;
    for (const PacketEvent& event : all_packet_events) {
      switch (event.media_type) {
        case AUDIO_EVENT:
          audio_packet_events.push_back(event);
          break;
        case VIDEO_EVENT:
          video_packet_events.push_back(event);
          break;
        default:
          FAIL();
          return;
      }
    }

    // For each frame, count the number of events that occurred for each event
    // for that frame.
    std::map<RtpTimeTicks, LoggingEventCounts> audio_event_counts_by_frame =
        GetEventCountsForFrameEvents(audio_frame_events);
    EXPECT_EQ(static_cast<size_t>(num_expected_audio_frames),
              audio_event_counts_by_frame.size());
    std::map<RtpTimeTicks, LoggingEventCounts> video_event_counts_by_frame =
        GetEventCountsForFrameEvents(video_frame_events);
    EXPECT_EQ(static_cast<size_t>(num_expected_video_frames),
              video_event_counts_by_frame.size());

    // Examine the types of each frame and packet event and verify required
    // events are present and unknown ones are not.
    VerifyLoggingEventCounts(audio_event_counts_by_frame,
                             GetEventCountsForPacketEvents(audio_packet_events),
                             true);
    VerifyLoggingEventCounts(video_event_counts_by_frame,
                             GetEventCountsForPacketEvents(video_packet_events),
                             false);
  }

  // Examines histograms of event types to verify all logging information was
  // captured correctly.
  static void VerifyLoggingEventCounts(
      const std::map<RtpTimeTicks, LoggingEventCounts>& event_counts_by_frame,
      const std::map<uint16_t, LoggingEventCounts>& event_counts_by_packet,
      bool for_audio) {
    // Verify that each frame has the expected types of events logged.
    for (const auto& e : event_counts_by_frame) {
      int total_event_count_for_frame = 0;
      for (int i = 0; i < kNumOfLoggingEvents; ++i) {
        total_event_count_for_frame += e.second.counter[i];
      }

      int count_of_valid_events = 0;
      if (!for_audio) {
        EXPECT_EQ(1, e.second.counter[FRAME_CAPTURE_BEGIN]);
        ++count_of_valid_events;
        EXPECT_EQ(1, e.second.counter[FRAME_CAPTURE_END]);
        ++count_of_valid_events;
      }
      EXPECT_EQ(1, e.second.counter[FRAME_ENCODED]);
      ++count_of_valid_events;
      EXPECT_EQ(1, e.second.counter[FRAME_DECODED]);
      ++count_of_valid_events;
      EXPECT_EQ(1, e.second.counter[FRAME_PLAYOUT]);
      ++count_of_valid_events;

      // There is no guarantee that FRAME_ACK_SENT is logged exactly once per
      // frame.
      EXPECT_GT(e.second.counter[FRAME_ACK_SENT], 0);
      count_of_valid_events += e.second.counter[FRAME_ACK_SENT];

      // There is no guarantee that FRAME_ACK_RECEIVED is logged exactly once
      // per frame.
      EXPECT_GT(e.second.counter[FRAME_ACK_RECEIVED], 0);
      count_of_valid_events += e.second.counter[FRAME_ACK_RECEIVED];

      // Verify that there were no unexpected events logged with respect to this
      // frame.
      EXPECT_EQ(count_of_valid_events, total_event_count_for_frame);
    }

    // Verify that each packet has the expected types of events logged.
    for (const auto& e : event_counts_by_packet) {
      int total_event_count_for_packet = 0;
      for (int i = 0; i < kNumOfLoggingEvents; ++i) {
        total_event_count_for_packet += e.second.counter[i];
      }

      EXPECT_GT(e.second.counter[PACKET_RECEIVED], 0);
      const int packets_received = e.second.counter[PACKET_RECEIVED];
      const int packets_sent = e.second.counter[PACKET_SENT_TO_NETWORK];
      EXPECT_EQ(packets_sent, packets_received);

      // Verify that there were no other events logged with respect to this
      // packet.  An assumption here is that there was no packet loss nor
      // retransmits during the end-to-end run.
      EXPECT_EQ(packets_received + packets_sent, total_event_count_for_packet);
    }
  }

  void BasicPlayerGotVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                                base::TimeTicks playout_time,
                                bool continuous) {
    // The following tests that the sender and receiver clocks can be
    // out-of-sync, drift, and jitter with respect to one another; and depsite
    // this, the receiver will produce smoothly-progressing playout times.
    // Both first-order and second-order effects are tested.
    if (!last_video_playout_time_.is_null() &&
        min_video_playout_delta_ > base::TimeDelta()) {
      const base::TimeDelta delta = playout_time - last_video_playout_time_;
      VLOG(1) << "Video frame playout time delta (compared to last frame) is "
              << delta.InMicroseconds() << " usec.";
      EXPECT_LE(min_video_playout_delta_.InMicroseconds(),
                delta.InMicroseconds());
      EXPECT_GE(max_video_playout_delta_.InMicroseconds(),
                delta.InMicroseconds());
      if (last_video_playout_delta_ > base::TimeDelta()) {
        base::TimeDelta abs_curvature = delta - last_video_playout_delta_;
        if (abs_curvature < base::TimeDelta())
          abs_curvature = -abs_curvature;
        EXPECT_GE(max_video_playout_curvature_.InMicroseconds(),
                  abs_curvature.InMicroseconds());
      }
      last_video_playout_delta_ = delta;
    }
    last_video_playout_time_ = playout_time;

    video_ticks_.push_back(
        std::make_pair(testing_clock_receiver_.NowTicks(), playout_time));
    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&End2EndTest::BasicPlayerGotVideoFrame,
                   base::Unretained(this)));
  }

  void BasicPlayerGotAudioFrame(std::unique_ptr<AudioBus> audio_bus,
                                base::TimeTicks playout_time,
                                bool is_continuous) {
    VLOG_IF(1, !last_audio_playout_time_.is_null())
        << "Audio frame playout time delta (compared to last frame) is "
        << (playout_time - last_audio_playout_time_).InMicroseconds()
        << " usec.";
    last_audio_playout_time_ = playout_time;

    audio_ticks_.push_back(
        std::make_pair(testing_clock_receiver_.NowTicks(), playout_time));
    cast_receiver_->RequestDecodedAudioFrame(
        base::Bind(&End2EndTest::BasicPlayerGotAudioFrame,
                   base::Unretained(this)));
  }

  void StartBasicPlayer() {
    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&End2EndTest::BasicPlayerGotVideoFrame,
                   base::Unretained(this)));
    cast_receiver_->RequestDecodedAudioFrame(
        base::Bind(&End2EndTest::BasicPlayerGotAudioFrame,
                   base::Unretained(this)));
  }

  FrameReceiverConfig audio_receiver_config_;
  FrameReceiverConfig video_receiver_config_;
  FrameSenderConfig audio_sender_config_;
  FrameSenderConfig video_sender_config_;

  base::TimeTicks start_time_;

  // These run in "test time"
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;

  // These run on the sender timeline.
  test::SkewedTickClock testing_clock_sender_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_sender_;

  // These run on the receiver timeline.
  test::SkewedTickClock testing_clock_receiver_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_receiver_;
  base::TimeDelta min_video_playout_delta_;
  base::TimeDelta max_video_playout_delta_;
  base::TimeDelta max_video_playout_curvature_;
  base::TimeTicks last_video_playout_time_;
  base::TimeDelta last_video_playout_delta_;
  base::TimeTicks last_audio_playout_time_;

  scoped_refptr<CastEnvironment> cast_environment_sender_;
  scoped_refptr<CastEnvironment> cast_environment_receiver_;

  LoopBackTransport* receiver_to_sender_;  // Owned by CastTransport.
  LoopBackTransport* sender_to_receiver_;  // Owned by CastTransport.

  std::unique_ptr<CastTransportImpl> transport_sender_;
  std::unique_ptr<CastTransportImpl> transport_receiver_;

  std::unique_ptr<CastReceiver> cast_receiver_;
  std::unique_ptr<CastSender> cast_sender_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;

  scoped_refptr<TestReceiverAudioCallback> test_receiver_audio_callback_;
  scoped_refptr<TestReceiverVideoCallback> test_receiver_video_callback_;

  std::unique_ptr<TestAudioBusFactory> audio_bus_factory_;

  SimpleEventSubscriber event_subscriber_sender_;

  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > audio_ticks_;
  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > video_ticks_;

  // |transport_sender_| has a RepeatingTimer which needs a MessageLoop.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

namespace {

class TransportClient : public CastTransport::Client {
 public:
  TransportClient(LogEventDispatcher* log_event_dispatcher,
                  End2EndTest* e2e_test)
      : log_event_dispatcher_(log_event_dispatcher), e2e_test_(e2e_test) {}

  void OnStatusChanged(media::cast::CastTransportStatus status) final {
    EXPECT_EQ(TRANSPORT_STREAM_INITIALIZED, status);
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {
    log_event_dispatcher_->DispatchBatchOfEvents(std::move(frame_events),
                                                 std::move(packet_events));
  }
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {
    if (e2e_test_)
      e2e_test_->ReceivePacket(std::move(packet));
  }

 private:
  LogEventDispatcher* const log_event_dispatcher_;  // Not owned by this class.
  End2EndTest* const e2e_test_;                     // Not owned by this class.

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

}  // namespace

void End2EndTest::Create() {
  transport_sender_.reset(new CastTransportImpl(
      &testing_clock_sender_, base::TimeDelta::FromMilliseconds(1),
      std::make_unique<TransportClient>(cast_environment_sender_->logger(),
                                        nullptr),
      base::WrapUnique(sender_to_receiver_), task_runner_sender_));

  transport_receiver_.reset(new CastTransportImpl(
      &testing_clock_sender_, base::TimeDelta::FromMilliseconds(1),
      std::make_unique<TransportClient>(cast_environment_receiver_->logger(),
                                        this),
      base::WrapUnique(receiver_to_sender_), task_runner_sender_));

  cast_receiver_ =
      CastReceiver::Create(cast_environment_receiver_, audio_receiver_config_,
                           video_receiver_config_, transport_receiver_.get());

  cast_sender_ =
      CastSender::Create(cast_environment_sender_, transport_sender_.get());

  // Initializing audio and video senders.
  cast_sender_->InitializeAudio(audio_sender_config_,
                                base::Bind(&ExpectSuccessOperationalStatus));
  cast_sender_->InitializeVideo(video_sender_config_,
                                base::Bind(&ExpectSuccessOperationalStatus),
                                CreateDefaultVideoEncodeAcceleratorCallback(),
                                CreateDefaultVideoEncodeMemoryCallback());
  task_runner_->RunTasks();

  receiver_to_sender_->SetPacketReceiver(
      transport_sender_->PacketReceiverForTesting(), task_runner_,
      &testing_clock_);
  sender_to_receiver_->SetPacketReceiver(
      transport_receiver_->PacketReceiverForTesting(), task_runner_,
      &testing_clock_);

  audio_frame_input_ = cast_sender_->audio_frame_input();
  video_frame_input_ = cast_sender_->video_frame_input();

  audio_bus_factory_.reset(new TestAudioBusFactory(
      audio_sender_config_.channels, audio_sender_config_.rtp_timebase,
      kSoundFrequency, kSoundVolume));
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_LoopWithLosslessEncoding DISABLED_LoopWithLosslessEncoding
#else
#define MAYBE_LoopWithLosslessEncoding LoopWithLosslessEncoding
#endif
TEST_F(End2EndTest, MAYBE_LoopWithLosslessEncoding) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();

  const auto frames_sent = RunAudioVideoLoop(base::TimeDelta::FromSeconds(3));

  // Make sure that we send a RTCP message containing receiver log data, then
  // verify the accumulated logging data.
  RunTasks(750);
  VerifyLogging(frames_sent.first, frames_sent.second);
}

TEST_F(End2EndTest, LoopWithLossyEncoding) {
  Configure(CODEC_VIDEO_VP8, CODEC_AUDIO_OPUS);
  Create();

  const auto frames_sent = RunAudioVideoLoop(base::TimeDelta::FromSeconds(1));

  // Run tasks for 750 ms to ensure RTCP messages containing log data from the
  // receiver are sent and processed by the sender.  Then, verify the expected
  // logging data is present.
  RunTasks(750);
  VerifyLogging(frames_sent.first, frames_sent.second);
}

// This tests start sending audio and video at start-up time before the receiver
// is ready; it sends 2 frames before the receiver comes online.
//
// Test disabled due to flakiness: It appears that the RTCP synchronization
// sometimes kicks in, and sometimes doesn't.  When it does, there's a sharp
// discontinuity in the timeline, throwing off the test expectations.  See TODOs
// in audio_receiver.cc for likely cause(s) of this bug.
// http://crbug.com/573126 (history: http://crbug.com/314233)
TEST_F(End2EndTest, DISABLED_StartSenderBeforeReceiver) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();

  int frame_number = 0;
  int audio_diff = kFrameTimerMs;

  sender_to_receiver_->SetSendPackets(false);

  const int test_delay_ms = 100;

  const int kNumVideoFramesBeforeReceiverStarted = 2;
  const base::TimeTicks initial_send_time = testing_clock_sender_.NowTicks();
  const base::TimeDelta expected_delay =
      base::TimeDelta::FromMilliseconds(test_delay_ms + kFrameTimerMs);
  for (int i = 0; i < kNumVideoFramesBeforeReceiverStarted; ++i) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    if (num_audio_frames > 0)
      FeedAudioFramesWithExpectedDelay(1, expected_delay);

    // Frame will be rendered with 100mS delay, as the transmission is delayed.
    // The receiver at this point cannot be synced to the sender's clock, as no
    // packets, and specifically no RTCP packets were sent.
    test_receiver_video_callback_->AddExpectedResult(
        frame_number, GetTestVideoFrameSize(),
        initial_send_time + expected_delay +
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);
    SendVideoFrame(frame_number++, testing_clock_sender_.NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFramesWithExpectedDelay(num_audio_frames - 1, expected_delay);

    RunTasks(kFrameTimerMs - kAudioFrameDurationMs);
    audio_diff += kFrameTimerMs;
  }

  RunTasks(test_delay_ms);
  sender_to_receiver_->SetSendPackets(true);

  int num_audio_frames_requested = 0;
  for (int j = 0; j < 10; ++j) {
    const int num_audio_frames = audio_diff / kAudioFrameDurationMs;
    audio_diff -= num_audio_frames * kAudioFrameDurationMs;

    if (num_audio_frames > 0)
      FeedAudioFrames(1, true);

    test_receiver_video_callback_->AddExpectedResult(
        frame_number, GetTestVideoFrameSize(),
        testing_clock_sender_.NowTicks() +
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs),
        true);
    SendVideoFrame(frame_number++, testing_clock_sender_.NowTicks());

    if (num_audio_frames > 0)
      RunTasks(kAudioFrameDurationMs);  // Advance clock forward.
    if (num_audio_frames > 1)
      FeedAudioFrames(num_audio_frames - 1, true);

    RequestAudioFrames(num_audio_frames, true);
    num_audio_frames_requested += num_audio_frames;

    cast_receiver_->RequestDecodedVideoFrame(
        base::Bind(&TestReceiverVideoCallback::CheckVideoFrame,
                   test_receiver_video_callback_,
                   video_sender_config_.codec != CODEC_VIDEO_FAKE));

    RunTasks(kFrameTimerMs - kAudioFrameDurationMs);
    audio_diff += kFrameTimerMs;
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the receiver pipeline.
  EXPECT_EQ(num_audio_frames_requested,
            test_receiver_audio_callback_->number_times_called());
  EXPECT_EQ(10, test_receiver_video_callback_->number_times_called());
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_BasicFakeSoftwareVideo DISABLED_BasicFakeSoftwareVideo
#else
#define MAYBE_BasicFakeSoftwareVideo BasicFakeSoftwareVideo
#endif
TEST_F(End2EndTest, MAYBE_BasicFakeSoftwareVideo) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(1.0, base::TimeDelta::FromMilliseconds(1));

  // Expect very smooth playout when there is no clock skew.
  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 99 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 101 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 100);

  int frames_counter = 0;
  for (; frames_counter < 30; ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(30ul, video_ticks_.size());
}

// The following tests run many many iterations to make sure that buffers don't
// fill, timers don't go askew etc. However, these high-level tests are too
// expensive when running under sanitizers, or in non-optimized debug builds.
// In these cases, we reduce the number of iterations.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
const int kLongTestIterations = 500;  // http://crbug.com/487033
#elif defined(NDEBUG)
const int kLongTestIterations = 10000;
#else
const int kLongTestIterations = 1000;
#endif

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_ReceiverClockFast DISABLED_ReceiverClockFast
#else
#define MAYBE_ReceiverClockFast ReceiverClockFast
#endif
TEST_F(End2EndTest, MAYBE_ReceiverClockFast) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(2.0, base::TimeDelta::FromMicroseconds(1234567));

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(static_cast<size_t>(kLongTestIterations), video_ticks_.size());
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_ReceiverClockSlow DISABLED_ReceiverClockSlow
#else
#define MAYBE_ReceiverClockSlow ReceiverClockSlow
#endif
TEST_F(End2EndTest, MAYBE_ReceiverClockSlow) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(0.5, base::TimeDelta::FromMicroseconds(-765432));

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(static_cast<size_t>(kLongTestIterations), video_ticks_.size());
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_SmoothPlayoutWithFivePercentClockRateSkew \
  DISABLED_SmoothPlayoutWithFivePercentClockRateSkew
#else
#define MAYBE_SmoothPlayoutWithFivePercentClockRateSkew \
  SmoothPlayoutWithFivePercentClockRateSkew
#endif
TEST_F(End2EndTest, MAYBE_SmoothPlayoutWithFivePercentClockRateSkew) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  Create();
  StartBasicPlayer();
  SetReceiverSkew(1.05, base::TimeDelta::FromMilliseconds(-42));

  // Expect smooth playout when there is 5% skew.
  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 90 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 110 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 10);

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(2 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_EQ(static_cast<size_t>(kLongTestIterations), video_ticks_.size());
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_EvilNetwork DISABLED_EvilNetwork
#else
#define MAYBE_EvilNetwork EvilNetwork
#endif
TEST_F(End2EndTest, MAYBE_EvilNetwork) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  receiver_to_sender_->SetPacketPipe(test::EvilNetwork());
  sender_to_receiver_->SetPacketPipe(test::EvilNetwork());
  Create();
  StartBasicPlayer();

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  base::TimeTicks test_end = testing_clock_receiver_.NowTicks();
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_LT(static_cast<size_t>(kLongTestIterations / 100),
            video_ticks_.size());
  VLOG(1) << "Fully transmitted " << video_ticks_.size() << " frames.";
  EXPECT_GT(1000, (video_ticks_.back().second - test_end).InMilliseconds());
}

// Tests that a system configured for 30 FPS drops frames when input is provided
// at a much higher frame rate.
// Fails consistently on official builds: crbug.com/612496
// crbug.com/997944. Flaky on multiple platforms.
#if defined(OFFICIAL_BUILD) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)
#define MAYBE_ShoveHighFrameRateDownYerThroat \
  DISABLED_ShoveHighFrameRateDownYerThroat
#else
#define MAYBE_ShoveHighFrameRateDownYerThroat ShoveHighFrameRateDownYerThroat
#endif
TEST_F(End2EndTest, MAYBE_ShoveHighFrameRateDownYerThroat) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  receiver_to_sender_->SetPacketPipe(test::EvilNetwork());
  sender_to_receiver_->SetPacketPipe(test::EvilNetwork());
  Create();
  StartBasicPlayer();

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(10 /* 10 ms, but 33.3 expected by system */);
  }
  base::TimeTicks test_end = testing_clock_receiver_.NowTicks();
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.
  EXPECT_LT(static_cast<size_t>(kLongTestIterations / 100),
            video_ticks_.size());
  EXPECT_GE(static_cast<size_t>(kLongTestIterations / 3), video_ticks_.size());
  VLOG(1) << "Fully transmitted " << video_ticks_.size() << " frames.";
  EXPECT_LT((video_ticks_.back().second - test_end).InMilliseconds(), 1000);
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_OldPacketNetwork DISABLED_OldPacketNetwork
#else
#define MAYBE_OldPacketNetwork OldPacketNetwork
#endif
TEST_F(End2EndTest, MAYBE_OldPacketNetwork) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  sender_to_receiver_->SetPacketPipe(test::NewRandomDrop(0.01));
  std::unique_ptr<test::PacketPipe> echo_chamber(
      test::NewDuplicateAndDelay(1, 10 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 20 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 40 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 80 * kFrameTimerMs));
  echo_chamber->AppendToPipe(
      test::NewDuplicateAndDelay(1, 160 * kFrameTimerMs));

  receiver_to_sender_->SetPacketPipe(std::move(echo_chamber));
  Create();
  StartBasicPlayer();

  SetExpectedVideoPlayoutSmoothness(
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 90 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) * 110 / 100,
      base::TimeDelta::FromMilliseconds(kFrameTimerMs) / 10);

  for (int frames_counter = 0; frames_counter < kLongTestIterations;
       ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.

  EXPECT_EQ(static_cast<size_t>(kLongTestIterations), video_ticks_.size());
}

// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_TestSetPlayoutDelay DISABLED_TestSetPlayoutDelay
#else
#define MAYBE_TestSetPlayoutDelay TestSetPlayoutDelay
#endif
TEST_F(End2EndTest, MAYBE_TestSetPlayoutDelay) {
  Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
  video_sender_config_.min_playout_delay =
      video_sender_config_.max_playout_delay;
  audio_sender_config_.min_playout_delay =
      audio_sender_config_.max_playout_delay;
  video_sender_config_.max_playout_delay = base::TimeDelta::FromSeconds(1);
  audio_sender_config_.max_playout_delay = base::TimeDelta::FromSeconds(1);
  Create();
  StartBasicPlayer();
  const int kNewDelay = 600;

  int frames_counter = 0;
  for (; frames_counter < 50; ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  cast_sender_->SetTargetPlayoutDelay(
      base::TimeDelta::FromMilliseconds(kNewDelay));
  for (; frames_counter < 100; ++frames_counter) {
    SendVideoFrame(frames_counter, testing_clock_sender_.NowTicks());
    RunTasks(kFrameTimerMs);
  }
  RunTasks(100 * kFrameTimerMs + 1);  // Empty the pipeline.
  size_t jump = 0;
  for (size_t i = 1; i < video_ticks_.size(); i++) {
    int64_t delta =
        (video_ticks_[i].second - video_ticks_[i - 1].second).InMilliseconds();
    if (delta > 100) {
      EXPECT_EQ(kNewDelay - kTargetPlayoutDelayMs + kFrameTimerMs, delta);
      EXPECT_EQ(0u, jump);
      jump = i;
    }
  }
  EXPECT_GT(jump, 49u);
  EXPECT_LT(jump, 120u);
}

}  // namespace cast
}  // namespace media
