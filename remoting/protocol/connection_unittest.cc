// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <numbers>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/math_constants.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_source.h"
#include "remoting/protocol/audio_stream.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/desktop_capturer.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/fake_video_renderer.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/ice_connection_to_host.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_connection_to_client.h"
#include "remoting/protocol/webrtc_connection_to_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::StrictMock;

namespace remoting::protocol {

namespace {

MATCHER_P(EqualsCapabilitiesMessage, message, "") {
  return arg.capabilities() == message.capabilities();
}

MATCHER_P(EqualsKeyEvent, event, "") {
  return arg.usb_keycode() == event.usb_keycode() &&
         arg.pressed() == event.pressed();
}

ACTION_P(QuitRunLoop, run_loop) {
  run_loop->Quit();
}

class MockConnectionToHostEventCallback
    : public ConnectionToHost::HostEventCallback {
 public:
  MockConnectionToHostEventCallback() = default;
  ~MockConnectionToHostEventCallback() override = default;

  MOCK_METHOD2(OnConnectionState,
               void(ConnectionToHost::State state, ErrorCode error));
  MOCK_METHOD1(OnConnectionReady, void(bool ready));
  MOCK_METHOD2(OnRouteChanged,
               void(const std::string& channel_name,
                    const TransportRoute& route));
};

class TestScreenCapturer : public DesktopCapturer {
 public:
  TestScreenCapturer() = default;
  ~TestScreenCapturer() override = default;

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    if (capture_request_index_to_fail_ >= 0) {
      capture_request_index_to_fail_--;
      if (capture_request_index_to_fail_ < 0) {
        callback_->OnCaptureResult(
            webrtc::DesktopCapturer::Result::ERROR_TEMPORARY, nullptr);
        return;
      }
    }

    // Return black 100x100 frame.
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(100, 100)));
    memset(frame->data(), frame_index_,
           frame->stride() * frame->size().height());
    frame_index_++;
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeSize(frame->size()));

    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* sources) override { return true; }

  bool SelectSource(SourceId id) override { return true; }

  void FailNthFrame(int n) { capture_request_index_to_fail_ = n; }

 private:
  raw_ptr<Callback> callback_ = nullptr;
  int frame_index_ = 0;

  int capture_request_index_to_fail_ = -1;
};

static const int kAudioSampleRate = AudioPacket::SAMPLING_RATE_48000;
static const int kAudioPacketDurationMs = 50;
static constexpr int kSamplesPerAudioPacket =
    kAudioSampleRate * kAudioPacketDurationMs /
    base::Time::kMillisecondsPerSecond;
static constexpr base::TimeDelta kAudioPacketDuration =
    base::Milliseconds(kAudioPacketDurationMs);

static const int kAudioChannels = 2;

static const int kTestAudioSignalFrequencyLeftHz = 3000;
static const int kTestAudioSignalFrequencyRightHz = 2000;

class TestAudioSource : public AudioSource {
 public:
  TestAudioSource() = default;
  ~TestAudioSource() override = default;

  // AudioSource interface.
  bool Start(const PacketCapturedCallback& callback) override {
    callback_ = callback;
    timer_.Start(FROM_HERE, kAudioPacketDuration,
                 base::BindRepeating(&TestAudioSource::GenerateAudioSamples,
                                     base::Unretained(this)));
    return true;
  }

 private:
  static int16_t GetSampleValue(double pos, int frequency) {
    const int kMaxSampleValue = 32767;
    return static_cast<int>(
        sin(pos * 2 * std::numbers::pi * frequency / kAudioSampleRate) *
            kMaxSampleValue +
        0.5);
  }

  void GenerateAudioSamples() {
    std::vector<int16_t> data(kSamplesPerAudioPacket * kAudioChannels);
    for (int i = 0; i < kSamplesPerAudioPacket; ++i) {
      data[i * kAudioChannels] = GetSampleValue(
          position_samples_ + i, kTestAudioSignalFrequencyLeftHz);
      data[i * kAudioChannels + 1] = GetSampleValue(
          position_samples_ + i, kTestAudioSignalFrequencyRightHz);
    }
    position_samples_ += kSamplesPerAudioPacket;

    std::unique_ptr<AudioPacket> packet(new AudioPacket());
    packet->add_data(reinterpret_cast<char*>(&(data[0])),
                     kSamplesPerAudioPacket * kAudioChannels * sizeof(int16_t));
    packet->set_encoding(AudioPacket::ENCODING_RAW);
    packet->set_sampling_rate(AudioPacket::SAMPLING_RATE_48000);
    packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
    packet->set_channels(AudioPacket::CHANNELS_STEREO);
    callback_.Run(std::move(packet));
  }

  PacketCapturedCallback callback_;
  base::RepeatingTimer timer_;
  int position_samples_ = 0;
};

class FakeAudioPlayer : public AudioStub {
 public:
  FakeAudioPlayer() {}
  ~FakeAudioPlayer() override = default;

  // AudioStub interface.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet,
                          base::OnceClosure done) override {
    EXPECT_TRUE(thread_checker_.CalledOnValidThread());
    EXPECT_EQ(AudioPacket::ENCODING_RAW, packet->encoding());
    EXPECT_EQ(AudioPacket::SAMPLING_RATE_48000, packet->sampling_rate());
    EXPECT_EQ(AudioPacket::BYTES_PER_SAMPLE_2, packet->bytes_per_sample());
    EXPECT_EQ(AudioPacket::CHANNELS_STEREO, packet->channels());

    data_.insert(data_.end(), packet->data(0).begin(), packet->data(0).end());

    if (run_loop_ && data_.size() >= samples_expected_ * 4) {
      run_loop_->Quit();
    }

    if (!done.is_null()) {
      std::move(done).Run();
    }
  }

  void WaitForSamples(size_t samples_expected) {
    samples_expected_ = samples_expected;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  void Verify() {
    const int16_t* data = reinterpret_cast<const int16_t*>(data_.data());
    int num_samples = data_.size() / kAudioChannels / sizeof(int16_t);

    // Skip the first 200 ms as these samples are more likely to be affected by
    // concealment which causes the zero-crossing frequency estimation to fail.
    // This is even more likely for ASAN builds.
    constexpr int kSkippedSamples =
        200 * kAudioSampleRate / base::Time::kMillisecondsPerSecond;
    ASSERT_GT(num_samples, kSkippedSamples);

    // Estimate signal frequency by counting how often it crosses 0.
    int left = 0;
    int right = 0;
    for (int i = kSkippedSamples; i < num_samples; ++i) {
      if (data[(i - 1) * kAudioChannels] < 0 && data[i * kAudioChannels] >= 0) {
        ++left;
      }
      if (data[(i - 1) * kAudioChannels + 1] < 0 &&
          data[i * kAudioChannels + 1] >= 0) {
        ++right;
      }
    }

    const int kMaxErrorHz = 50;
    int left_hz = (left * kAudioSampleRate / (num_samples - kSkippedSamples));
    EXPECT_LE(kTestAudioSignalFrequencyLeftHz - kMaxErrorHz, left_hz);
    EXPECT_GE(kTestAudioSignalFrequencyLeftHz + kMaxErrorHz, left_hz);
    int right_hz = (right * kAudioSampleRate / (num_samples - kSkippedSamples));
    EXPECT_LE(kTestAudioSignalFrequencyRightHz - kMaxErrorHz, right_hz);
    EXPECT_GE(kTestAudioSignalFrequencyRightHz + kMaxErrorHz, right_hz);
  }

  base::WeakPtr<AudioStub> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  base::ThreadChecker thread_checker_;
  std::vector<char> data_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  size_t samples_expected_ = 0;

  base::WeakPtrFactory<FakeAudioPlayer> weak_factory_{this};
};

}  // namespace

class ConnectionTest : public testing::Test,
                       public testing::WithParamInterface<bool> {
 public:
  ConnectionTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        video_encode_thread_("VideoEncode"),
        audio_encode_thread_("AudioEncode"),
        audio_decode_thread_("AudioDecode") {
    video_encode_thread_.Start();
    audio_encode_thread_.Start();
    audio_decode_thread_.Start();
  }

  ConnectionTest(const ConnectionTest&) = delete;
  ConnectionTest& operator=(const ConnectionTest&) = delete;

  void DestroyHost() {
    host_connection_.reset();
    run_loop_->Quit();
  }

 protected:
  bool is_using_webrtc() { return GetParam(); }

  void SetUp() override {
    // Create fake sessions.
    host_session_ = new FakeSession();
    owned_client_session_ = std::make_unique<FakeSession>();
    client_session_ = owned_client_session_.get();

    // Create Connection objects.
    if (is_using_webrtc()) {
      // Set the polling interval to zero to prevent hangs from PostDelayedTask.
      WebrtcTransport::SetDataChannelPollingIntervalForTests(base::TimeDelta());

      host_connection_ = std::make_unique<WebrtcConnectionToClient>(
          base::WrapUnique(host_session_.get()),
          TransportContext::ForTests(protocol::TransportRole::SERVER),
          task_environment_.GetMainThreadTaskRunner());
      client_connection_ = std::make_unique<WebrtcConnectionToHost>();
    } else {
      host_connection_ = std::make_unique<IceConnectionToClient>(
          base::WrapUnique(host_session_.get()),
          TransportContext::ForTests(protocol::TransportRole::SERVER),
          task_environment_.GetMainThreadTaskRunner(),
          task_environment_.GetMainThreadTaskRunner());
      client_connection_ = std::make_unique<IceConnectionToHost>();
    }

    // Setup host side.
    host_connection_->SetEventHandler(&host_event_handler_);
    host_connection_->set_clipboard_stub(&host_clipboard_stub_);
    host_connection_->set_host_stub(&host_stub_);
    host_connection_->set_input_stub(&host_input_stub_);

    // Setup client side.
    client_connection_->set_client_stub(&client_stub_);
    client_connection_->set_clipboard_stub(&client_clipboard_stub_);
    client_connection_->set_video_renderer(&client_video_renderer_);

    client_connection_->InitializeAudio(audio_decode_thread_.task_runner(),
                                        client_audio_player_.GetWeakPtr());
  }

  void Connect() {
    {
      testing::InSequence sequence;
      EXPECT_CALL(host_event_handler_, OnConnectionAuthenticating());
      EXPECT_CALL(host_event_handler_, OnConnectionAuthenticated(nullptr));
    }
    EXPECT_CALL(host_event_handler_, OnConnectionChannelsConnected())
        .WillOnce(InvokeWithoutArgs(this, &ConnectionTest::OnHostConnected));
    EXPECT_CALL(host_event_handler_, OnRouteChange(_, _))
        .Times(testing::AnyNumber());

    {
      testing::InSequence sequence;
      EXPECT_CALL(
          client_event_handler_,
          OnConnectionState(ConnectionToHost::CONNECTING, ErrorCode::OK));
      EXPECT_CALL(
          client_event_handler_,
          OnConnectionState(ConnectionToHost::AUTHENTICATED, ErrorCode::OK));
      EXPECT_CALL(client_event_handler_,
                  OnConnectionState(ConnectionToHost::CONNECTED, ErrorCode::OK))
          .WillOnce(
              InvokeWithoutArgs(this, &ConnectionTest::OnClientConnected));
    }
    EXPECT_CALL(client_event_handler_, OnRouteChanged(_, _))
        .Times(testing::AnyNumber());

    NetworkSettings network_settings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    host_connection_->ApplyNetworkSettings(network_settings);

    client_connection_->Connect(
        std::move(owned_client_session_),
        TransportContext::ForTests(protocol::TransportRole::CLIENT),
        &client_event_handler_);
    client_connection_->ApplyNetworkSettings(network_settings);
    client_session_->SimulateConnection(host_session_);

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();

    EXPECT_TRUE(client_connected_);
    EXPECT_TRUE(host_connected_);
  }

  void TearDown() override {
    client_connection_.reset();
    host_connection_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void OnHostConnected() {
    host_connected_ = true;
    if (client_connected_ && run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnClientConnected() {
    client_connected_ = true;
    if (host_connected_ && run_loop_) {
      run_loop_->Quit();
    }
  }

  void WaitNextVideoFrame() {
    size_t received_frames =
        is_using_webrtc()
            ? client_video_renderer_.GetFrameConsumer()
                  ->received_frames()
                  .size()
            : client_video_renderer_.GetVideoStub()->received_packets().size();

    base::RunLoop run_loop;

    // Expect frames to be passed to FrameConsumer when WebRTC is used, or to
    // VideoStub otherwise.
    if (is_using_webrtc()) {
      client_video_renderer_.GetFrameConsumer()->set_on_frame_callback(
          base::BindRepeating(&base::RunLoop::Quit,
                              base::Unretained(&run_loop)));
    } else {
      client_video_renderer_.GetVideoStub()->set_on_frame_callback(
          base::BindRepeating(&base::RunLoop::Quit,
                              base::Unretained(&run_loop)));
    }

    run_loop.Run();

    if (is_using_webrtc()) {
      EXPECT_EQ(
          client_video_renderer_.GetFrameConsumer()->received_frames().size(),
          received_frames + 1);
      EXPECT_EQ(
          client_video_renderer_.GetVideoStub()->received_packets().size(), 0U);
      client_video_renderer_.GetFrameConsumer()->set_on_frame_callback({});
    } else {
      EXPECT_EQ(
          client_video_renderer_.GetFrameConsumer()->received_frames().size(),
          0U);
      EXPECT_EQ(
          client_video_renderer_.GetVideoStub()->received_packets().size(),
          received_frames + 1);
      client_video_renderer_.GetVideoStub()->set_on_frame_callback({});
    }
  }

  void WaitFirstFrameStats() {
    if (!client_video_renderer_.GetFrameStatsConsumer()
             ->received_stats()
             .empty()) {
      return;
    }

    base::RunLoop run_loop;
    client_video_renderer_.GetFrameStatsConsumer()->set_on_stats_callback(
        base::BindRepeating(&base::RunLoop::Quit, base::Unretained(&run_loop)));
    run_loop.Run();
    client_video_renderer_.GetFrameStatsConsumer()->set_on_stats_callback({});

    EXPECT_FALSE(client_video_renderer_.GetFrameStatsConsumer()
                     ->received_stats()
                     .empty());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  MockConnectionToClientEventHandler host_event_handler_;
  MockClipboardStub host_clipboard_stub_;
  MockHostStub host_stub_;
  MockInputStub host_input_stub_;
  std::unique_ptr<ConnectionToClient> host_connection_;
  raw_ptr<FakeSession, AcrossTasksDanglingUntriaged>
      host_session_;  // Owned by |host_connection_|.
  bool host_connected_ = false;

  MockConnectionToHostEventCallback client_event_handler_;
  MockClientStub client_stub_;
  MockClipboardStub client_clipboard_stub_;
  FakeVideoRenderer client_video_renderer_;
  FakeAudioPlayer client_audio_player_;
  std::unique_ptr<ConnectionToHost> client_connection_;
  raw_ptr<FakeSession, AcrossTasksDanglingUntriaged>
      client_session_;  // Owned by |client_connection_|.
  std::unique_ptr<FakeSession> owned_client_session_;
  bool client_connected_ = false;

  base::Thread video_encode_thread_;
  base::Thread audio_encode_thread_;
  base::Thread audio_decode_thread_;
};

INSTANTIATE_TEST_SUITE_P(Ice, ConnectionTest, ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(Webrtc, ConnectionTest, ::testing::Values(true));

TEST_P(ConnectionTest, RejectConnection) {
  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CONNECTING, ErrorCode::OK));
  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CLOSED, ErrorCode::OK));

  client_connection_->Connect(
      std::move(owned_client_session_),
      TransportContext::ForTests(protocol::TransportRole::CLIENT),
      &client_event_handler_);
  client_session_->event_handler()->OnSessionStateChange(Session::CLOSED);
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Disconnect DISABLED_Disconnect
#else
#define MAYBE_Disconnect Disconnect
#endif
TEST_P(ConnectionTest, MAYBE_Disconnect) {
  Connect();

  EXPECT_CALL(client_event_handler_,
              OnConnectionState(ConnectionToHost::CLOSED, ErrorCode::OK));
  EXPECT_CALL(host_event_handler_, OnConnectionClosed(ErrorCode::OK));

  client_session_->Close(ErrorCode::OK);
  base::RunLoop().RunUntilIdle();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Control DISABLED_Control
#else
#define MAYBE_Control Control
#endif
TEST_P(ConnectionTest, MAYBE_Control) {
  Connect();

  Capabilities capabilities_msg;
  capabilities_msg.set_capabilities("test_capability");

  base::RunLoop run_loop;

  EXPECT_CALL(client_stub_,
              SetCapabilities(EqualsCapabilitiesMessage(capabilities_msg)))
      .WillOnce(QuitRunLoop(&run_loop));

  // Send capabilities from the host.
  host_connection_->client_stub()->SetCapabilities(capabilities_msg);

  run_loop.Run();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Events DISABLED_Events
#else
#define MAYBE_Events Events
#endif
TEST_P(ConnectionTest, MAYBE_Events) {
  Connect();

  KeyEvent event;
  event.set_usb_keycode(3);
  event.set_pressed(true);

  base::RunLoop run_loop;

  EXPECT_CALL(host_input_stub_, InjectKeyEvent(EqualsKeyEvent(event)))
      .WillOnce(QuitRunLoop(&run_loop));

  // Send key event from the client.
  client_connection_->input_stub()->InjectKeyEvent(event);

  run_loop.Run();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Video DISABLED_Video
#else
#define MAYBE_Video Video
#endif
TEST_P(ConnectionTest, MAYBE_Video) {
  Connect();

  std::unique_ptr<VideoStream> video_stream =
      host_connection_->StartVideoStream(
          0, std::make_unique<TestScreenCapturer>());

  // Receive 5 frames.
  for (int i = 0; i < 5; ++i) {
    WaitNextVideoFrame();
  }
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VideoWithSlowSignaling DISABLED_VideoWithSlowSignaling
#else
#define MAYBE_VideoWithSlowSignaling VideoWithSlowSignaling
#endif
// Verifies that the VideoStream doesn't lose any video frames while the
// connection is being established.
TEST_P(ConnectionTest, MAYBE_VideoWithSlowSignaling) {
  // Add signaling delay to slow down connection handshake.
  host_session_->set_signaling_delay(base::Milliseconds(100));
  client_session_->set_signaling_delay(base::Milliseconds(100));

  Connect();

  std::unique_ptr<VideoStream> video_stream =
      host_connection_->StartVideoStream(
          0, base::WrapUnique(new TestScreenCapturer()));

  WaitNextVideoFrame();
}

// crbug.com/1224862: Tests are flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_DestroyOnIncomingMessage DISABLED_DestroyOnIncomingMessage
#else
#define MAYBE_DestroyOnIncomingMessage DestroyOnIncomingMessage
#endif
TEST_P(ConnectionTest, MAYBE_DestroyOnIncomingMessage) {
  Connect();

  KeyEvent event;
  event.set_usb_keycode(3);
  event.set_pressed(true);

  base::RunLoop run_loop;

  EXPECT_CALL(host_input_stub_, InjectKeyEvent(EqualsKeyEvent(event)))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &ConnectionTest::DestroyHost),
                      QuitRunLoop(&run_loop)));

  // Send key event from the client.
  client_connection_->input_stub()->InjectKeyEvent(event);

  run_loop.Run();
}

// TODO(crbug.com/40729981): Test is flaky.
TEST_P(ConnectionTest, DISABLED_VideoStats) {
  // Currently this test only works for WebRTC because ICE connections stats are
  // reported by SoftwareVideoRenderer which is not used in this test.
  // TODO(sergeyu): Fix this.
  if (!is_using_webrtc()) {
    return;
  }

  Connect();

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::TimeTicks event_timestamp = base::TimeTicks::FromInternalValue(42);

  scoped_refptr<InputEventTimestampsSourceImpl> input_event_timestamps_source =
      new InputEventTimestampsSourceImpl();
  input_event_timestamps_source->OnEventReceived(
      InputEventTimestamps{event_timestamp, start_time});

  std::unique_ptr<VideoStream> video_stream =
      host_connection_->StartVideoStream(
          0, std::make_unique<TestScreenCapturer>());
  video_stream->SetEventTimestampsSource(input_event_timestamps_source);

  WaitNextVideoFrame();

  base::TimeTicks finish_time = base::TimeTicks::Now();

  WaitFirstFrameStats();

  const FrameStats& stats =
      client_video_renderer_.GetFrameStatsConsumer()->received_stats().front();

  EXPECT_GT(stats.host_stats.frame_size, 0);

  EXPECT_EQ(stats.host_stats.latest_event_timestamp, event_timestamp);
  EXPECT_NE(stats.host_stats.capture_delay, base::TimeDelta::Max());
  EXPECT_NE(stats.host_stats.capture_overhead_delay, base::TimeDelta::Max());
  EXPECT_NE(stats.host_stats.encode_delay, base::TimeDelta::Max());
  EXPECT_NE(stats.host_stats.send_pending_delay, base::TimeDelta::Max());

  EXPECT_FALSE(stats.client_stats.time_received.is_null());
  EXPECT_FALSE(stats.client_stats.time_decoded.is_null());
  EXPECT_FALSE(stats.client_stats.time_rendered.is_null());

  EXPECT_LE(start_time + stats.host_stats.capture_pending_delay +
                stats.host_stats.capture_delay +
                stats.host_stats.capture_overhead_delay +
                stats.host_stats.encode_delay +
                stats.host_stats.send_pending_delay,
            stats.client_stats.time_received);
  EXPECT_LE(stats.client_stats.time_received, stats.client_stats.time_decoded);
  EXPECT_LE(stats.client_stats.time_decoded, stats.client_stats.time_rendered);
  EXPECT_LE(stats.client_stats.time_rendered, finish_time);
}

// Slow/fails on Linux ASan/TSan (crbug.com/1045344) and flaky on Mac
// (crbug.com/1237376) and Windows (crbug.com/1503680).
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&               \
        (defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)) || \
    BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_Audio DISABLED_Audio
#else
#define MAYBE_Audio Audio
#endif
TEST_P(ConnectionTest, MAYBE_Audio) {
  Connect();

  std::unique_ptr<AudioStream> audio_stream =
      host_connection_->StartAudioStream(std::make_unique<TestAudioSource>());

  // Wait for 1 second worth of audio samples.
  client_audio_player_.WaitForSamples(kAudioSampleRate * 2);
  client_audio_player_.Verify();
}

// Flaky on multiple platforms
// https://crbug.com/1143671
TEST_P(ConnectionTest, DISABLED_FirstCaptureFailed) {
  Connect();

  auto capturer = std::make_unique<TestScreenCapturer>();
  capturer->FailNthFrame(0);
  auto video_stream =
      host_connection_->StartVideoStream(0, std::move(capturer));

  WaitNextVideoFrame();
}

// Flaky on multiple platforms
// https://crbug.com/1143671
TEST_P(ConnectionTest, DISABLED_SecondCaptureFailed) {
  Connect();

  auto capturer = std::make_unique<TestScreenCapturer>();
  capturer->FailNthFrame(1);
  auto video_stream =
      host_connection_->StartVideoStream(0, std::move(capturer));

  WaitNextVideoFrame();
  WaitNextVideoFrame();
}

}  // namespace remoting::protocol
