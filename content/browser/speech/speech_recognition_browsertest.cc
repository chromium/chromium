// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_capturer_source.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using CaptureCallback = media::AudioCapturerSource::CaptureCallback;

namespace content {

namespace {

// TODO(https://crbug.com/841818) Use FakeSystemInfo instead.
class MockAudioSystem : public media::AudioSystem {
 public:
  MockAudioSystem() = default;

  // AudioSystem implementation.
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback on_params_cb) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Posting callback to allow current SpeechRecognizerImpl dispatching event
    // to complete before transitioning to the next FSM state.
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(std::move(on_params_cb),
                       media::AudioParameters::UnavailableDeviceParams()));
  }

  MOCK_METHOD2(GetOutputStreamParameters,
               void(const std::string& device_id,
                    OnAudioParamsCallback on_params_cb));
  MOCK_METHOD1(HasInputDevices, void(OnBoolCallback on_has_devices_cb));
  MOCK_METHOD1(HasOutputDevices, void(OnBoolCallback on_has_devices_cb));
  MOCK_METHOD2(GetDeviceDescriptions,
               void(bool for_input,
                    OnDeviceDescriptionsCallback on_descriptions_cp));
  MOCK_METHOD2(GetAssociatedOutputDeviceID,
               void(const std::string& input_device_id,
                    OnDeviceIdCallback on_device_id_cb));
  MOCK_METHOD2(GetInputDeviceInfo,
               void(const std::string& input_device_id,
                    OnInputDeviceInfoCallback on_input_device_info_cb));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioSystem);
};

class MockCapturerSource : public media::AudioCapturerSource {
 public:
  using StartCallback =
      base::OnceCallback<void(const media::AudioParameters& audio_parameters,
                              CaptureCallback* capture_callback)>;
  using StopCallback = base::OnceCallback<void()>;

  MockCapturerSource(StartCallback start_callback, StopCallback stop_callback) {
    start_callback_ = std::move(start_callback);
    stop_callback_ = std::move(stop_callback);
  }

  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) {
    audio_parameters_ = params;
    capture_callback_ = callback;
  }

  void Start() override {
    std::move(start_callback_).Run(audio_parameters_, capture_callback_);
  }

  void Stop() override { std::move(stop_callback_).Run(); }

  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetOutputDeviceForAec,
               void(const std::string& output_device_id));

 protected:
  ~MockCapturerSource() override = default;

 private:
  StartCallback start_callback_;
  StopCallback stop_callback_;
  CaptureCallback* capture_callback_;
  media::AudioParameters audio_parameters_;

  DISALLOW_COPY_AND_ASSIGN(MockCapturerSource);
};

std::string MakeGoodResponse() {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  blink::mojom::SpeechRecognitionResultPtr result =
      blink::mojom::SpeechRecognitionResult::New();
  result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
      base::UTF8ToUTF16("Pictures of the moon"), 1.0F));
  proto_result->set_final(!result->is_provisional);
  for (size_t i = 0; i < result->hypotheses.size(); ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    const blink::mojom::SpeechRecognitionHypothesisPtr& hypothesis =
        result->hypotheses[i];
    proto_alternative->set_confidence(hypothesis->confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis->utterance));
  }

  std::string msg_string;
  proto_event.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32_t prefix =
      base::HostToNet32(base::checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));
  return msg_string;
}

}  // namespace

class SpeechRecognitionBrowserTest : public ContentBrowserTest {
 public:
  enum StreamingServerState {
    kIdle,
    kTestAudioCapturerSourceOpened,
    kTestAudioCapturerSourceClosed,
  };

  // Helper methods used by test fixtures.
  GURL GetTestUrlFromFragment(const std::string& fragment) {
    return GURL(GetTestUrl("speech", "web_speech_recognition.html").spec() +
        "#" + fragment);
  }

  std::string GetPageFragment() {
    return shell()->web_contents()->GetLastCommittedURL().ref();
  }

  const StreamingServerState &streaming_server_state() {
    return streaming_server_state_;
  }

 protected:
  // ContentBrowserTest methods.
  void SetUpOnMainThread() override {
    streaming_server_state_ = kIdle;

    ASSERT_TRUE(SpeechRecognitionManagerImpl::GetInstance());
    audio_system_ = std::make_unique<MockAudioSystem>();
    audio_capturer_source_ = base::MakeRefCounted<MockCapturerSource>(
        base::BindOnce(&SpeechRecognitionBrowserTest::OnCapturerSourceStart,
                       base::Unretained(this)),
        base::BindOnce(&SpeechRecognitionBrowserTest::OnCapturerSourceStop,
                       base::Unretained(this)));
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(
        audio_system_.get(),
        static_cast<media::AudioCapturerSource*>(audio_capturer_source_.get()));
  }

  void TearDownOnMainThread() override {
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(nullptr, nullptr);
  }

 private:
  void OnCapturerSourceStart(const media::AudioParameters& audio_parameters,
                             CaptureCallback* capture_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kIdle, streaming_server_state_);
    streaming_server_state_ = kTestAudioCapturerSourceOpened;

    const int capture_packet_interval_ms =
        (1000 * audio_parameters.frames_per_buffer()) /
        audio_parameters.sample_rate();
    ASSERT_EQ(SpeechRecognitionEngine::kAudioPacketIntervalMs,
              capture_packet_interval_ms);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 500 /* ms */,
                            /*noise=*/false);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 1000 /* ms */,
                            /*noise=*/true);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 1000 /* ms */,
                            /*noise=*/false);
  }

  void OnCapturerSourceStop() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kTestAudioCapturerSourceOpened, streaming_server_state_);
    streaming_server_state_ = kTestAudioCapturerSourceClosed;

    // Reset capturer source so SpeechRecognizerImpl destructor doesn't call
    // AudioCaptureSourcer::Stop() again.
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(nullptr, nullptr);

    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&SpeechRecognitionBrowserTest::SendResponse,
                                  base::Unretained(this)));
  }

  void SendResponse() {}

  static void FeedSingleBufferToAudioCapturerSource(
      const media::AudioParameters& audio_params,
      CaptureCallback* capture_callback,
      size_t buffer_size,
      bool fill_with_noise) {
    DCHECK(capture_callback);
    std::unique_ptr<uint8_t[]> audio_buffer(new uint8_t[buffer_size]);
    if (fill_with_noise) {
      for (size_t i = 0; i < buffer_size; ++i)
        audio_buffer[i] =
            static_cast<uint8_t>(127 * sin(i * 3.14F / (16 * buffer_size)));
    } else {
      memset(audio_buffer.get(), 0, buffer_size);
    }

    std::unique_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(audio_params);
    audio_bus->FromInterleaved<media::SignedInt16SampleTypeTraits>(
        reinterpret_cast<int16_t*>(&audio_buffer.get()[0]),
        audio_bus->frames());
    capture_callback->Capture(audio_bus.get(), base::TimeTicks::Now(), 0.0,
                              false);
  }

  void FeedAudioCapturerSource(const media::AudioParameters& audio_params,
                               CaptureCallback* capture_callback,
                               int duration_ms,
                               bool feed_with_noise) {
    const size_t buffer_size =
        audio_params.GetBytesPerBuffer(media::kSampleFormatS16);
    const int ms_per_buffer = audio_params.GetBufferDuration().InMilliseconds();
    // We can only simulate durations that are integer multiples of the
    // buffer size. In this regard see
    // SpeechRecognitionEngine::GetDesiredAudioChunkDurationMs().
    ASSERT_EQ(0, duration_ms % ms_per_buffer);

    const int n_buffers = duration_ms / ms_per_buffer;
    for (int i = 0; i < n_buffers; ++i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&FeedSingleBufferToAudioCapturerSource, audio_params,
                         capture_callback, buffer_size, feed_with_noise));
    }
  }

  std::unique_ptr<media::AudioSystem> audio_system_;
  scoped_refptr<MockCapturerSource> audio_capturer_source_;
  StreamingServerState streaming_server_state_;
};

// Simply loads the test page and checks if it was able to create a Speech
// Recognition object in JavaScript, to make sure the Web Speech API is enabled.
// Flaky on all platforms. http://crbug.com/396414.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, DISABLED_Precheck) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GetTestUrlFromFragment("precheck"), 2);

  EXPECT_EQ(kIdle, streaming_server_state());
  EXPECT_EQ("success", GetPageFragment());
}

// Flaky on mac, see https://crbug.com/794645.
#if defined(OS_MACOSX)
#define MAYBE_OneShotRecognition DISABLED_OneShotRecognition
#else
#define MAYBE_OneShotRecognition OneShotRecognition
#endif
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, MAYBE_OneShotRecognition) {
  // Set up a test server, with two response handlers.
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", true /* relative_url_is_prefix */);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", true /* relative_url_is_prefix */);
  ASSERT_TRUE(embedded_test_server()->Start());
  // Use a base path that doesn't end in a slash to mimic the default URL.
  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  SpeechRecognitionEngine::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  // Need to watch for two navigations. Can't use
  // NavigateToURLBlockUntilNavigationsComplete so that the
  // ControllableHttpResponses can be used to wait for the test server to see
  // the network requests, and response to them.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(GetTestUrlFromFragment("oneshot"));

  // Wait for the upstream HTTP request to be completely received, and return an
  // empty response.
  upstream_response.WaitForRequest();
  EXPECT_FALSE(upstream_response.http_request()->content.empty());
  EXPECT_EQ(net::test_server::METHOD_POST,
            upstream_response.http_request()->method);
  EXPECT_EQ("chunked",
            upstream_response.http_request()->headers.at("Transfer-Encoding"));
  EXPECT_EQ("audio/x-flac; rate=16000",
            upstream_response.http_request()->headers.at("Content-Type"));
  upstream_response.Send("HTTP/1.1 200 OK\r\n\r\n");
  upstream_response.Done();

  // Wait for the downstream HTTP request to be received, and response with a
  // valid response.
  downstream_response.WaitForRequest();
  EXPECT_EQ(net::test_server::METHOD_GET,
            downstream_response.http_request()->method);
  downstream_response.Send("HTTP/1.1 200 OK\r\n\r\n" + MakeGoodResponse());
  downstream_response.Done();

  navigation_observer.Wait();

  EXPECT_EQ(kTestAudioCapturerSourceClosed, streaming_server_state());
  EXPECT_EQ("goodresult1", GetPageFragment());

  // Remove reference to URL string that's on the stack.
  SpeechRecognitionEngine::set_web_service_base_url_for_tests(nullptr);
}

}  // namespace content
