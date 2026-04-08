// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(jophba): remove these helpers once the abstract Sender interface lands
// and pure virtual sender interfaces can be easily constructed.

#ifndef MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_
#define MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/cast/cast_config.h"
#include "media/cast/test/mock_openscreen_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/openscreen/src/cast/streaming/impl/rtp_defines.h"  // nogncheck
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/sender_packet_router.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/time.h"
#include "third_party/openscreen/src/platform/base/trivial_clock_traits.h"

namespace media::cast {

// Returns a valid session configuration with dummy values for testing.
openscreen::cast::SessionConfig GetDefaultSessionConfigForTesting();

class MockSender : public openscreen::cast::Sender {
 public:
  MockSender();
  explicit MockSender(openscreen::cast::SessionConfig config);
  ~MockSender() override;

  MOCK_METHOD(const openscreen::cast::SessionConfig&,
              config,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetObserver,
              (openscreen::cast::Sender::Observer*),
              (override));
  MOCK_METHOD(size_t, GetInFlightFrameCount, (), (const, override));
  MOCK_METHOD(openscreen::Clock::duration,
              GetInFlightMediaDuration,
              (openscreen::cast::RtpTimeTicks),
              (const, override));
  MOCK_METHOD(openscreen::Clock::duration,
              GetMaxInFlightMediaDuration,
              (),
              (const, override));
  MOCK_METHOD(bool, NeedsKeyFrame, (), (const, override));
  MOCK_METHOD(openscreen::cast::FrameId, GetNextFrameId, (), (const, override));
  MOCK_METHOD(openscreen::Clock::duration,
              GetCurrentRoundTripTime,
              (),
              (const, override));
  MOCK_METHOD(openscreen::cast::Sender::EnqueueFrameResult,
              EnqueueFrame,
              (const openscreen::cast::EncodedFrame&),
              (override));
  MOCK_METHOD(void, CancelInFlightData, (), (override));
  MOCK_METHOD(void,
              ReportFrameDropEvent,
              (openscreen::cast::FrameId,
               openscreen::cast::RtpTimeTicks,
               openscreen::Clock::time_point),
              (override));

  openscreen::cast::Sender::Observer* observer() const { return observer_; }
  void set_observer(openscreen::cast::Sender::Observer* observer) {
    observer_ = observer;
  }

  std::set<openscreen::cast::FrameId>& in_flight_frames() {
    return in_flight_frames_;
  }

  void set_environment(MockOpenscreenEnvironment* environment) {
    environment_ = environment;
  }

  void set_task_runner(openscreen::TaskRunner* task_runner) {
    task_runner_ = task_runner;
  }

  base::WeakPtr<MockSender> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  openscreen::cast::SessionConfig config_;
  raw_ptr<openscreen::cast::Sender::Observer> observer_ = nullptr;
  std::set<openscreen::cast::FrameId> in_flight_frames_;
  raw_ptr<MockOpenscreenEnvironment> environment_ = nullptr;
  raw_ptr<openscreen::TaskRunner> task_runner_ = nullptr;

  base::WeakPtrFactory<MockSender> weak_factory_{this};
};

class MockReceiver : public openscreen::cast::Receiver {
 public:
  MockReceiver();
  explicit MockReceiver(openscreen::cast::SessionConfig config);
  ~MockReceiver() override;

  MOCK_METHOD(const openscreen::cast::SessionConfig&,
              config,
              (),
              (const, override));
  MOCK_METHOD(void,
              SetConsumer,
              (openscreen::cast::Receiver::Consumer*),
              (override));
  MOCK_METHOD(void,
              SetPlayerProcessingTime,
              (openscreen::Clock::duration),
              (override));
  MOCK_METHOD(openscreen::Error,
              ReportPlayoutEvent,
              (openscreen::cast::FrameId,
               openscreen::cast::RtpTimeTicks,
               openscreen::Clock::time_point),
              (override));
  MOCK_METHOD(void, RequestKeyFrame, (), (override));
  MOCK_METHOD(std::optional<size_t>, AdvanceToNextFrame, (), (override));
  MOCK_METHOD(openscreen::cast::EncodedFrame,
              ConsumeNextFrame,
              (openscreen::ByteBuffer),
              (override));

 private:
  openscreen::cast::SessionConfig config_;
};

// Used to construct a valid set of openscreen::cast::Senders for use in tests.
// Takes care of some of the complex Open Screen configuration.
struct OpenscreenTestSenders {
  struct Config {
    Config(scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
           const base::TickClock* clock,
           std::optional<openscreen::cast::RtpPayloadType> audio_rtp_type =
               std::nullopt,
           std::optional<openscreen::cast::RtpPayloadType> video_rtp_type =
               std::nullopt,
           std::optional<FrameSenderConfig> audio_config = std::nullopt,
           std::optional<FrameSenderConfig> video_config = std::nullopt);
    Config(const Config&) = delete;
    Config(Config&&);
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&);
    ~Config();

    // The task runner to be used with the Open Screen environment.
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner;

    // Used for setting up the fake Open Screen clock.
    raw_ptr<const base::TickClock> clock;

    // If set, will construct a valid audio sender.
    std::optional<openscreen::cast::RtpPayloadType> audio_rtp_type;

    // If set, will construct a valid video sender.
    std::optional<openscreen::cast::RtpPayloadType> video_rtp_type;

    // If set, will use this config to help set up the audio sender.
    std::optional<FrameSenderConfig> audio_config;

    // If set, will use this config to help set up the video sender.
    std::optional<FrameSenderConfig> video_config;
  };

  explicit OpenscreenTestSenders(const Config& config);
  OpenscreenTestSenders(const OpenscreenTestSenders&) = delete;
  OpenscreenTestSenders(OpenscreenTestSenders&&) = delete;
  OpenscreenTestSenders& operator=(const OpenscreenTestSenders&) = delete;
  OpenscreenTestSenders& operator=(OpenscreenTestSenders&&) = delete;
  ~OpenscreenTestSenders();

  openscreen_platform::TaskRunner task_runner;
  std::unique_ptr<MockOpenscreenEnvironment> environment;
  std::unique_ptr<openscreen::cast::SenderPacketRouter> sender_packet_router;
  std::unique_ptr<MockSender> audio_sender;
  std::unique_ptr<MockSender> video_sender;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_OPENSCREEN_TEST_HELPERS_H_
