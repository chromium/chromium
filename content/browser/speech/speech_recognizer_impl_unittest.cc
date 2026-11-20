// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "content/browser/speech/network_speech_recognition_engine_impl.h"
#include "content/public/browser/google_streaming_api.pb.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/test_helpers.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::AudioInputStream;
using media::AudioOutputStream;
using media::AudioParameters;

namespace content {

namespace {

class MockCapturerSource : public media::AudioCapturerSource {
 public:
  MockCapturerSource() = default;
  MOCK_METHOD2(Initialize,
               void(const media::AudioParameters& params,
                    CaptureCallback* callback));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetOutputDeviceForAec,
               void(const std::string& output_device_id));

 protected:
  ~MockCapturerSource() override = default;
};

}  // namespace

class SpeechRecognizerImplTest : public SpeechRecognitionEventListener,
                                 public testing::Test {
 public:
  SpeechRecognizerImplTest()
      : audio_capturer_source_(
            base::MakeRefCounted<testing::NiceMock<MockCapturerSource>>()),
        recognition_started_(false),
        recognition_ended_(false),
        result_received_(false),
        audio_started_(false),
        audio_ended_(false),
        sound_started_(false),
        sound_ended_(false),
        error_(media::mojom::SpeechRecognitionErrorCode::kNone),
        volume_(-1.0f) {
    // This test environment is not set up to support out-of-process services.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kAudioServiceOutOfProcess});

    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>(true));
    audio_manager_->SetInputStreamParameters(
        media::AudioParameters::UnavailableDeviceParams());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(
        audio_system_.get(), audio_capturer_source_.get());
    ConfigureRecognizer(/*continuous=*/false, /*provisional_results=*/false);

    const int channels = SpeechRecognizerImpl::kChannelLayoutConfig.channels();

    const int bytes_per_sample =
        SpeechRecognizerImpl::kNumBitsPerAudioSample / 8;

    const int frames = media::AudioTimestampHelper::TimeToFrames(
        base::Milliseconds(
            NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs),
        SpeechRecognizerImpl::kAudioSampleRate);

    const int audio_packet_length_bytes = frames * channels * bytes_per_sample;
    audio_packet_.resize(audio_packet_length_bytes);

    audio_bus_ = media::AudioBus::Create(channels, frames);
    audio_bus_->Zero();
  }

  ~SpeechRecognizerImplTest() override {
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(nullptr, nullptr);
    audio_manager_->Shutdown();
  }

  void ConfigureRecognizer(bool continuous, bool provisional_results) {
    auto sr_engine = std::make_unique<NetworkSpeechRecognitionEngineImpl>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_));
    NetworkSpeechRecognitionEngineImpl::Config config;
    config.audio_num_bits_per_sample =
        SpeechRecognizerImpl::kNumBitsPerAudioSample;
    config.audio_sample_rate = SpeechRecognizerImpl::kAudioSampleRate;
    config.filter_profanities = false;
    config.continuous = continuous;
    config.interim_results = provisional_results;
    sr_engine->SetConfig(config);

    constexpr int kTestingSessionId = 1;
    recognizer_ = new SpeechRecognizerImpl(
        this, audio_system_.get(), kTestingSessionId, continuous,
        provisional_results, std::move(sr_engine), std::nullopt);
    downstream_data_pipe_.reset();
    downstream_bytes_sent_ = 0;
  }

  void SendStreamingProtoEvent(const proto::SpeechRecognitionEvent& event) {
    if (!downstream_data_pipe_.is_valid()) {
      const network::TestURLLoaderFactory::PendingRequest* downstream_request =
          nullptr;
      ASSERT_TRUE(GetDownstreamRequest(&downstream_request));
      auto head = network::mojom::URLResponseHead::New();
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\n\n"));
      mojo::ScopedDataPipeProducerHandle producer_handle;
      mojo::ScopedDataPipeConsumerHandle consumer_handle;
      ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
                MOJO_RESULT_OK);
      downstream_request->client->OnReceiveResponse(
          std::move(head), std::move(consumer_handle), std::nullopt);
      downstream_data_pipe_ = std::move(producer_handle);
    }

    std::string msg_string;
    event.SerializeToString(&msg_string);
    auto msg_size_bytes =
        base::U32ToBigEndian(base::checked_cast<uint32_t>(msg_string.size()));
    msg_string.insert(0u, base::as_string_view(msg_size_bytes));
    downstream_bytes_sent_ += msg_string.size();

    base::span<const uint8_t> remaining = base::as_byte_span(msg_string);
    while (!remaining.empty()) {
      size_t written = 0;
      const MojoResult mojo_result = downstream_data_pipe_->WriteData(
          remaining, MOJO_WRITE_DATA_FLAG_NONE, written);
      if (mojo_result == MOJO_RESULT_OK) {
        remaining = remaining.subspan(written);
        continue;
      }
      ASSERT_EQ(MOJO_RESULT_SHOULD_WAIT, mojo_result);
      base::RunLoop().RunUntilIdle();
    }
    base::RunLoop().RunUntilIdle();
  }

  void StartCapture() {
    // Wait for EVENT_PREPARE and EVENT_START processing, which starts capture.
    base::RunLoop capture_started_loop;
    EXPECT_CALL(*audio_capturer_source_, Start())
        .WillOnce(base::test::RunClosure(capture_started_loop.QuitClosure()));
    recognizer_->StartRecognition(
        media::AudioDeviceDescription::kDefaultDeviceId);
    capture_started_loop.Run();
  }

  // Captures `packets` 100ms packets of low-energy noise, speech, or silence.
  void CaptureNoise(int packets) {
    FillPacketWithNoise();
    CapturePackets(packets);
  }
  void CaptureSpeech(int packets) {
    FillPacketWithTestWaveform();
    CapturePackets(packets);
  }
  void CaptureSilence(int packets) {
    audio_bus_->Zero();
    CapturePackets(packets);
  }
  void CapturePackets(int packets) {
    for (int i = 0; i < packets; ++i) {
      Capture(audio_bus_.get());
    }
    base::RunLoop().RunUntilIdle();
  }

  static proto::SpeechRecognitionEvent MakeEvent() {
    proto::SpeechRecognitionEvent event;
    event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
    return event;
  }

  static void AddResult(proto::SpeechRecognitionEvent& event,
                        bool final,
                        const std::string& transcript) {
    proto::SpeechRecognitionResult* result = event.add_result();
    result->set_final(final);
    proto::SpeechRecognitionAlternative* alternative =
        result->add_alternative();
    alternative->set_transcript(transcript);
    if (final) {
      alternative->set_confidence(0.9f);
    } else {
      result->set_stability(0.8f);
    }
  }

  // Responds to the downstream request with `event` in a single response.
  void SendResponse(const proto::SpeechRecognitionEvent& event) {
    std::string msg_string;
    event.SerializeToString(&msg_string);
    auto msg_size_bytes =
        base::U32ToBigEndian(base::checked_cast<uint32_t>(msg_string.size()));
    msg_string.insert(0u, base::as_string_view(msg_size_bytes));
    const network::TestURLLoaderFactory::PendingRequest* downstream_request;
    ASSERT_TRUE(GetDownstreamRequest(&downstream_request));
    url_loader_factory_.AddResponse(downstream_request->request.url.spec(),
                                    msg_string);
  }

  static media::mojom::WebSpeechRecognitionResultPtr MakeEngineResult(
      std::optional<base::TimeDelta> audio_start_time,
      std::optional<base::TimeDelta> audio_end_time) {
    auto result = media::mojom::WebSpeechRecognitionResult::New();
    result->hypotheses.push_back(
        media::mojom::SpeechRecognitionHypothesis::New(u"hello", 0.9));
    result->is_provisional = false;
    result->audio_start_time = audio_start_time;
    result->audio_end_time = audio_end_time;
    return result;
  }

  // Delivers `results` as if they came from the recognition engine.
  void SendEngineResults(
      std::vector<media::mojom::WebSpeechRecognitionResultPtr> results) {
    static_cast<SpeechRecognitionEngine::Delegate*>(recognizer_.get())
        ->OnSpeechRecognitionEngineResults(results);
    base::RunLoop().RunUntilIdle();
  }

  // Sends a streaming event with a single result.
  void SendStreamingResult(bool final, const std::string& transcript) {
    proto::SpeechRecognitionEvent event = MakeEvent();
    AddResult(event, final, transcript);
    SendStreamingProtoEvent(event);
  }

  // Stops a streaming session and waits for recognition to end.
  void StopStreamingSession() {
    recognizer_->StopAudioCapture();
    base::RunLoop().RunUntilIdle();
    CloseStreamingDownstream();
    ASSERT_TRUE(base::test::RunUntil([&] { return recognition_ended_; }));
  }

  // The start and end of `last_results_[index]` in milliseconds.
  using Times = std::pair<int64_t, int64_t>;
  Times ResultTimes(size_t index) const {
    const auto& result = last_results_[index];
    return {result->audio_start_time->InMilliseconds(),
            result->audio_end_time->InMilliseconds()};
  }

  void CloseStreamingDownstream() {
    const network::TestURLLoaderFactory::PendingRequest* downstream_request =
        nullptr;
    ASSERT_TRUE(GetDownstreamRequest(&downstream_request));
    network::URLLoaderCompletionStatus status(net::OK);
    status.decoded_body_length = base::ByteSize(downstream_bytes_sent_);
    downstream_request->client->OnComplete(status);
    downstream_data_pipe_.reset();
    base::RunLoop().RunUntilIdle();
  }

  [[nodiscard]] bool GetUpstreamRequest(
      const network::TestURLLoaderFactory::PendingRequest**
          pending_request_out) {
    return GetPendingRequest(pending_request_out, "/up");
  }

  [[nodiscard]] bool GetDownstreamRequest(
      const network::TestURLLoaderFactory::PendingRequest**
          pending_request_out) {
    return GetPendingRequest(pending_request_out, "/down");
  }

  [[nodiscard]] bool GetPendingRequest(
      const network::TestURLLoaderFactory::PendingRequest** pending_request_out,
      const char* url_substring) {
    for (const auto& pending_request :
         *url_loader_factory_.pending_requests()) {
      if (pending_request.request.url.spec().find(url_substring) !=
          std::string::npos) {
        *pending_request_out = &pending_request;
        return true;
      }
    }
    return false;
  }

  void CheckEventsConsistency() {
    // Note: "!x || y" == "x implies y".
    EXPECT_TRUE(!recognition_ended_ || recognition_started_);
    EXPECT_TRUE(!audio_ended_ || audio_started_);
    EXPECT_TRUE(!sound_ended_ || sound_started_);
    EXPECT_TRUE(!audio_started_ || recognition_started_);
    EXPECT_TRUE(!sound_started_ || audio_started_);
    EXPECT_TRUE(!audio_ended_ || (sound_ended_ || !sound_started_));
    EXPECT_TRUE(!recognition_ended_ || (audio_ended_ || !audio_started_));
  }

  void CheckFinalEventsConsistency() {
    // Note: "!(x ^ y)" == "(x && y) || (!x && !x)".
    EXPECT_FALSE(recognition_started_ ^ recognition_ended_);
    EXPECT_FALSE(audio_started_ ^ audio_ended_);
    EXPECT_FALSE(sound_started_ ^ sound_ended_);
  }

  // Overridden from SpeechRecognitionEventListener:
  void OnAudioStart(int session_id) override {
    audio_started_ = true;
    CheckEventsConsistency();
  }

  void OnAudioEnd(int session_id) override {
    audio_ended_ = true;
    CheckEventsConsistency();
  }

  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results)
      override {
    result_received_ = true;
    last_results_ = mojo::Clone(results);
  }

  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override {
    EXPECT_TRUE(recognition_started_);
    EXPECT_FALSE(recognition_ended_);
    error_ = error.code;
  }

  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override {
    volume_ = volume;
    noise_volume_ = noise_volume;
  }

  void OnRecognitionEnd(int session_id) override {
    recognition_ended_ = true;
    CheckEventsConsistency();
  }

  void OnRecognitionStart(int session_id) override {
    recognition_started_ = true;
    CheckEventsConsistency();
  }

  void OnSoundStart(int session_id) override {
    sound_started_ = true;
    CheckEventsConsistency();
  }

  void OnSoundEnd(int session_id) override {
    sound_ended_ = true;
    CheckEventsConsistency();
  }

  void CopyPacketToAudioBus() {
    static_assert(SpeechRecognizerImpl::kNumBitsPerAudioSample == 16,
                  "FromInterleaved expects 2 bytes.");
    // Copy the created signal into an audio bus in a deinterleaved format.
    audio_bus_->FromInterleavedBytes<media::SignedInt16SampleTypeTraits>(
        audio_packet_);
  }

  void FillPacketWithTestWaveform() {
    // Fill the input with a simple pattern, a 125Hz sawtooth waveform.
    for (size_t i = 0; i < audio_packet_.size(); ++i)
      audio_packet_[i] = static_cast<uint8_t>(i);
    CopyPacketToAudioBus();
  }

  void FillPacketWithNoise() {
    int value = 0;
    int factor = 175;
    for (size_t i = 0; i < audio_packet_.size(); ++i) {
      value += factor;
      audio_packet_[i] = value % 100;
    }
    CopyPacketToAudioBus();
  }

  void Capture(media::AudioBus* data) {
    auto* capture_callback =
        static_cast<media::AudioCapturerSource::CaptureCallback*>(
            recognizer_.get());
    capture_callback->Capture(data, base::TimeTicks::Now(), {}, 0.0);
  }

  void OnCaptureError() {
    auto* capture_callback =
        static_cast<media::AudioCapturerSource::CaptureCallback*>(
            recognizer_.get());
    capture_callback->OnCaptureError(
        media::AudioCapturerSource::ErrorCode::kUnknown, "");
  }

  void WaitForAudioThreadToPostDeviceInfo() {
    media::WaitableMessageLoopEvent event;
    audio_manager_->GetTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), event.GetClosure());
    // Runs the loop and waits for the audio thread to call event's closure,
    // which means AudioSystem reply containing device parameters is already
    // queued on the main thread.
    event.RunAndWait();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<SpeechRecognizerImpl> recognizer_;
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  scoped_refptr<MockCapturerSource> audio_capturer_source_;
  mojo::ScopedDataPipeProducerHandle downstream_data_pipe_;
  size_t downstream_bytes_sent_ = 0;
  bool recognition_started_;
  bool recognition_ended_;
  bool result_received_;
  bool audio_started_;
  bool audio_ended_;
  bool sound_started_;
  bool sound_ended_;
  media::mojom::SpeechRecognitionErrorCode error_;
  std::vector<uint8_t> audio_packet_;
  std::unique_ptr<media::AudioBus> audio_bus_;
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> last_results_;
  float volume_;
  float noise_volume_;
};

TEST_F(SpeechRecognizerImplTest, StartNoInputDevices) {
  // Check for callbacks when stopping record before any audio gets recorded.
  audio_manager_->SetHasInputDevices(false);
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  OnCaptureError();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kAudioCapture, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, StartFakeInputDevice) {
  // Check for callbacks when stopping record before any audio gets recorded.
  audio_manager_->SetHasInputDevices(false);
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  Capture(audio_bus_.get());
  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, StopBeforeDeviceInfoReceived) {
  // Check for callbacks when stopping record before reply is received from
  // AudioSystem.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Block audio thread.
  audio_manager_->GetTaskRunner()->PostTask(FROM_HERE,
                                            event.GetWaitCallbackForTesting());

  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();

  // Release audio thread and receive a callback from it.
  event.Signal();
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, CancelBeforeDeviceInfoReceived) {
  // Check for callbacks when stopping record before reply is received from
  // AudioSystem.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Block audio thread.
  audio_manager_->GetTaskRunner()->PostTask(FROM_HERE,
                                            event.GetWaitCallbackForTesting());

  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();

  // Release audio thread and receive a callback from it.
  event.Signal();
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, StopNoData) {
  // Check for callbacks when stopping record before any audio gets recorded.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();  // EVENT_START and EVENT_STOP processing.
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, CancelNoData) {
  // Check for callbacks when canceling recognition before any audio gets
  // recorded.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();  // EVENT_START and EVENT_ABORT processing.
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kAborted, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, StopWithData) {
  // Start recording, give some data and then stop. This should wait for the
  // network callback to arrive before completion.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.

  // Try sending 5 chunks of mock audio data and verify that each of them
  // resulted immediately in a packet sent out via the network. This verifies
  // that we are streaming out encoded data as chunks without waiting for the
  // full recording to complete.
  const size_t kNumChunks = 5;
  mojo::Remote<network::mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
            MOJO_RESULT_OK);
  for (size_t i = 0; i < kNumChunks; ++i) {
    Capture(audio_bus_.get());

    if (i == 0) {
      // Set up data channel to read chunked upload data. Must be done after the
      // first OnData() call.
      base::RunLoop().RunUntilIdle();
      const network::TestURLLoaderFactory::PendingRequest* upstream_request;
      ASSERT_TRUE(GetUpstreamRequest(&upstream_request));
      ASSERT_TRUE(upstream_request->request.request_body);
      ASSERT_EQ(1u, upstream_request->request.request_body->elements()->size());
      auto& element =
          (*upstream_request->request.request_body->elements_mutable())[0];
      ASSERT_EQ(network::DataElement::Tag::kChunkedDataPipe, element.type());
      chunked_data_pipe_getter.Bind(
          element.As<network::DataElementChunkedDataPipe>()
              .ReleaseChunkedDataPipeGetter());
      chunked_data_pipe_getter->StartReading(std::move(producer_handle));
    }

    std::string data;
    while (true) {
      base::RunLoop().RunUntilIdle();

      base::span<const uint8_t> buffer;
      MojoResult result =
          consumer_handle->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
      if (result == MOJO_RESULT_OK) {
        data.append(base::as_string_view(buffer));
        consumer_handle->EndReadData(buffer.size());
        continue;
      }
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        // Some data has already been read, assume there's no more to read.
        if (!data.empty())
          break;
        continue;
      }

      FAIL() << "Mojo pipe closed unexpectedly";
    }

    EXPECT_FALSE(data.empty());
  }

  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);

  // Create a response string.
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_final(true);
  proto::SpeechRecognitionAlternative* proto_alternative =
      proto_result->add_alternative();
  proto_alternative->set_confidence(0.5f);
  proto_alternative->set_transcript("123");
  std::string msg_string;
  proto_event.SerializeToString(&msg_string);
  auto msg_size_bytes =
      base::U32ToBigEndian(base::checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0u, base::as_string_view(msg_size_bytes));

  // Issue the network callback to complete the process.
  const network::TestURLLoaderFactory::PendingRequest* downstream_request;
  ASSERT_TRUE(GetDownstreamRequest(&downstream_request));
  url_loader_factory_.AddResponse(downstream_request->request.url.spec(),
                                  msg_string);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(recognition_ended_);
  EXPECT_TRUE(result_received_);
  // No speech was detected, so the result spans all audio the endpointer
  // processed, which skips the first chunk.
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(base::TimeDelta(), last_results_[0]->audio_start_time);
  EXPECT_EQ(base::Milliseconds(
                (kNumChunks - 1) *
                NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs),
            last_results_[0]->audio_end_time);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, CancelWithData) {
  // Start recording, give some data and then cancel.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  Capture(audio_bus_.get());
  base::RunLoop().RunUntilIdle();
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();
  // There should be both upstream and downstream pending requests.
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kAborted, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, ConnectionError) {
  // Start recording, give some data and then stop. Issue the network callback
  // with a connection error and verify that the recognizer bubbles the error up
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  Capture(audio_bus_.get());
  base::RunLoop().RunUntilIdle();
  // There should be both upstream and downstream pending requests.
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());

  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);

  // Issue the network callback to complete the process.
  const network::TestURLLoaderFactory::PendingRequest* pending_request;
  ASSERT_TRUE(GetUpstreamRequest(&pending_request));
  url_loader_factory_.AddResponse(
      pending_request->request.url, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_REFUSED));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNetwork, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, ServerError) {
  // Start recording, give some data and then stop. Issue the network callback
  // with a 500 error and verify that the recognizer bubbles the error up
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.
  Capture(audio_bus_.get());
  base::RunLoop().RunUntilIdle();
  // There should be both upstream and downstream pending requests.
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());

  recognizer_->StopAudioCapture();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(audio_started_);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);

  const network::TestURLLoaderFactory::PendingRequest* pending_request;
  ASSERT_TRUE(GetUpstreamRequest(&pending_request));
  auto response = network::mojom::URLResponseHead::New();
  const char kHeaders[] = "HTTP/1.0 500 Internal Server Error";
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(kHeaders));
  url_loader_factory_.AddResponse(pending_request->request.url,
                                  std::move(response), "",
                                  network::URLLoaderCompletionStatus());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recognition_ended_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNetwork, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, OnCaptureError_PropagatesError) {
  // Check if things tear down properly if AudioInputController threw an error.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.

  OnCaptureError();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_FALSE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kAudioCapture, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, NoSpeechCallbackIssued) {
  // Start recording and give a lot of packets with audio samples set to zero.
  // This should trigger the no-speech detector and issue a callback.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.

  int num_packets =
      (SpeechRecognizerImpl::kNoSpeechTimeoutMs) /
          NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs +
      1;
  // The vector is already filled with zero value samples on create.
  for (int i = 0; i < num_packets; ++i) {
    Capture(audio_bus_.get());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(recognition_started_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(result_received_);
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNoSpeech, error_);
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, NoSpeechCallbackNotIssued) {
  // Start recording and give a lot of packets with audio samples set to zero
  // and then some more with reasonably loud audio samples. This should be
  // treated as normal speech input and the no-speech detector should not get
  // triggered.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.

  int num_packets = (SpeechRecognizerImpl::kNoSpeechTimeoutMs) /
                    NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs;

  // The vector is already filled with zero value samples on create.
  for (int i = 0; i < num_packets / 2; ++i) {
    Capture(audio_bus_.get());
  }

  FillPacketWithTestWaveform();
  for (int i = 0; i < num_packets / 2; ++i) {
    Capture(audio_bus_.get());
  }

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  EXPECT_TRUE(audio_started_);
  EXPECT_FALSE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, SetInputVolumeCallback) {
  // Start recording and give a lot of packets with audio samples set to zero
  // and then some more with reasonably loud audio samples. Check that we don't
  // get the callback during estimation phase, then get zero for the silence
  // samples and proper volume for the loud audio.
  recognizer_->StartRecognition(
      media::AudioDeviceDescription::kDefaultDeviceId);
  base::RunLoop().RunUntilIdle();  // EVENT_PREPARE processing.
  WaitForAudioThreadToPostDeviceInfo();
  base::RunLoop().RunUntilIdle();  // EVENT_START processing.

  // Feed some samples to begin with for the endpointer to do noise estimation.
  int num_packets = SpeechRecognizerImpl::kEndpointerEstimationTimeMs /
                    NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs;
  FillPacketWithNoise();
  for (int i = 0; i < num_packets; ++i) {
    Capture(audio_bus_.get());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(-1.0f, volume_);  // No audio volume set yet.

  // The vector is already filled with zero value samples on create.
  Capture(audio_bus_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FLOAT_EQ(0.74939233f, volume_);

  FillPacketWithTestWaveform();
  Capture(audio_bus_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_NEAR(0.89926866f, volume_, 0.00001f);
  EXPECT_FLOAT_EQ(0.75071919f, noise_volume_);

  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNone, error_);
  EXPECT_FALSE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);
  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest, OneShotEndpointerTimestamps) {
  StartCapture();
  // Silence through environment estimation, speech (500..1000ms), then silence
  // until the endpointer completes the utterance. Times are on the endpointer
  // clock, which starts after the first chunk.
  CaptureSilence(6);
  CaptureSpeech(5);
  EXPECT_TRUE(sound_started_);
  CaptureSilence(10);
  EXPECT_TRUE(audio_ended_);
  EXPECT_FALSE(recognition_ended_);

  proto::SpeechRecognitionEvent event = MakeEvent();
  AddResult(event, /*final=*/true, "hello");
  SendResponse(event);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(recognition_ended_);
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1300), ResultTimes(0));
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest,
       ContinuousEndpointerTimestampsAndPreservesEngineTimestamps) {
  ConfigureRecognizer(/*continuous=*/true, /*provisional_results=*/true);
  StartCapture();
  // Silence through environment estimation, then the first utterance
  // (500..1000ms).
  CaptureSilence(6);
  CaptureSpeech(5);
  EXPECT_TRUE(sound_started_);

  // An interim result mid-utterance spans from the onset to the latest audio.
  SendStreamingResult(/*final=*/false, "hel");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1000), ResultTimes(0));

  // After a pause (1000..1500ms), the final result ends at the offset.
  CaptureSilence(5);
  SendStreamingResult(/*final=*/true, "hello");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1300), ResultTimes(0));

  // Quiet (1500..2000ms), the second utterance (2000..2500ms), then silence.
  CaptureSilence(5);
  CaptureSpeech(5);
  CaptureSilence(5);
  SendStreamingResult(/*final=*/true, "world");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(2100, 2800), ResultTimes(0));

  // Engine timestamps, e.g. from SODA, are kept as is.
  std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
  results.push_back(
      MakeEngineResult(base::Milliseconds(123), base::Milliseconds(456)));
  SendEngineResults(std::move(results));
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(123, 456), ResultTimes(0));

  // An engine-supplied end is kept, and the start never lands after it.
  results.clear();
  results.push_back(MakeEngineResult(std::nullopt, base::Milliseconds(300)));
  SendEngineResults(std::move(results));
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(300, 300), ResultTimes(0));

  recognizer_->AbortRecognition();
  base::RunLoop().RunUntilIdle();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest,
       ContinuousEndpointerTimestampsAfterFalseStart) {
  ConfigureRecognizer(/*continuous=*/true, /*provisional_results=*/true);
  StartCapture();
  CaptureNoise(6);
  CaptureSpeech(5);
  CaptureSilence(5);
  SendStreamingResult(/*final=*/true, "hi");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1300), ResultTimes(0));

  // A short noise burst (1500..1700ms) gets an interim result that is never
  // finalized.
  CaptureSpeech(2);
  SendStreamingResult(/*final=*/false, "uh");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(1600, 1700), ResultTimes(0));

  // After 1.5s of silence, a real utterance (3200..3700ms) and its result.
  // Known limit: the false start pulls the start earlier, but never before the
  // previous final result's end.
  CaptureSilence(15);
  CaptureSpeech(5);
  CaptureSilence(6);
  SendStreamingResult(/*final=*/true, "hello");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(1600, 4000), ResultTimes(0));

  StopStreamingSession();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest,
       ContinuousEndpointerTimestampsWhenNextUtteranceStartsEarly) {
  ConfigureRecognizer(/*continuous=*/true, /*provisional_results=*/true);
  StartCapture();
  CaptureNoise(6);
  // The first utterance (500..1000ms) and a pause, then the second utterance
  // (1600..2000ms) starts before the first final result arrives.
  CaptureSpeech(5);
  CaptureSilence(6);
  CaptureSpeech(4);
  proto::SpeechRecognitionEvent overlap = MakeEvent();
  AddResult(overlap, /*final=*/true, "first");
  AddResult(overlap, /*final=*/false, "sec");
  SendStreamingProtoEvent(overlap);
  ASSERT_EQ(2u, last_results_.size());
  // Known limit: the final result runs to its arrival time, and the next
  // utterance's results start where it ended.
  EXPECT_EQ(Times(600, 2000), ResultTimes(0));
  EXPECT_EQ(Times(2000, 2000), ResultTimes(1));

  // The second utterance ends, and its final result ends at the offset.
  CaptureSilence(6);
  SendStreamingResult(/*final=*/true, "second");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(2000, 2300), ResultTimes(0));

  StopStreamingSession();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest,
       ContinuousEndpointerTimestampsWithMidSentencePause) {
  ConfigureRecognizer(/*continuous=*/true, /*provisional_results=*/true);
  StartCapture();
  CaptureNoise(6);
  // One sentence with a pause long enough to confirm an offset: speech
  // (500..1000ms), silence (1000..1400ms), then more speech (1400..1700ms).
  CaptureSpeech(5);
  CaptureSilence(4);
  CaptureSpeech(3);

  // Several interim chunks in one event all cover the sentence so far.
  proto::SpeechRecognitionEvent interim = MakeEvent();
  AddResult(interim, /*final=*/false, "hello wor");
  AddResult(interim, /*final=*/false, "ld");
  SendStreamingProtoEvent(interim);
  ASSERT_EQ(2u, last_results_.size());
  EXPECT_EQ(Times(600, 1700), ResultTimes(0));
  EXPECT_EQ(Times(600, 1700), ResultTimes(1));

  // The final result arrives before the second part's offset is confirmed,
  // and still covers the whole sentence.
  CaptureSpeech(2);
  SendStreamingResult(/*final=*/true, "hello world");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1900), ResultTimes(0));

  StopStreamingSession();
  CheckFinalEventsConsistency();
}

TEST_F(SpeechRecognizerImplTest,
       ContinuousEndpointerTimestampsWhenClauseFinalizedMidSpeech) {
  ConfigureRecognizer(/*continuous=*/true, /*provisional_results=*/true);
  StartCapture();
  CaptureNoise(6);
  // The engine finalizes a clause while speech continues (500..1000ms).
  CaptureSpeech(5);
  SendStreamingResult(/*final=*/true, "first");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(600, 1000), ResultTimes(0));

  // Speech continues (1000..1500ms), then pauses. The next final result ends
  // at the detected offset rather than its arrival time.
  CaptureSpeech(5);
  CaptureSilence(6);
  SendStreamingResult(/*final=*/true, "second");
  ASSERT_EQ(1u, last_results_.size());
  EXPECT_EQ(Times(1000, 1800), ResultTimes(0));

  StopStreamingSession();
  CheckFinalEventsConsistency();
}

}  // namespace content
