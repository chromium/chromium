// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <list>
#include <memory>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/speech/network_speech_recognition_engine_impl.h"
#include "content/browser/speech/speech_recognition_dispatcher_host.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/google_streaming_api.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_sample_types.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "base/test/scoped_feature_list.h"
#include "components/soda/mock_soda_installer.h"  // nogncheck
#include "components/soda/soda_util.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/browser/speech/soda_speech_recognition_engine_impl.h"
#include "content/public/browser/storage_partition_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#endif  // !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using base::RunLoop;
using CaptureCallback = media::AudioCapturerSource::CaptureCallback;

#if !BUILDFLAG(IS_FUCHSIA)
using testing::_;
using testing::InvokeWithoutArgs;
#endif  // !BUILDFLAG(IS_FUCHSIA)

namespace content {

namespace {

#if !BUILDFLAG(IS_FUCHSIA)
const char kWebSpeechExpectGoodResult1[] = "Pictures of the moon";
const char kWebSpeechPageGoodResult1[] = "goodresult1";
#endif  // !BUILDFLAG(IS_FUCHSIA)

// TODO(crbug.com/40575807) Use FakeSystemInfo instead.
class MockAudioSystem : public media::AudioSystem {
 public:
  MockAudioSystem() = default;

  MockAudioSystem(const MockAudioSystem&) = delete;
  MockAudioSystem& operator=(const MockAudioSystem&) = delete;

  // AudioSystem implementation.
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback on_params_cb) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Posting callback to allow current SpeechRecognizerImpl dispatching event
    // to complete before transitioning to the next FSM state.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
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

  MockCapturerSource(const MockCapturerSource&) = delete;
  MockCapturerSource& operator=(const MockCapturerSource&) = delete;

  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) override {
    audio_parameters_ = params;
    capture_callback_ = callback;
  }

  void Start() override {
    std::move(start_callback_).Run(audio_parameters_, capture_callback_.get());
  }

  void Stop() override { std::move(stop_callback_).Run(); }

  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetOutputDeviceForAec,
               void(const std::string& output_device_id));

 private:
  ~MockCapturerSource() override = default;

  StartCallback start_callback_;
  StopCallback stop_callback_;
  raw_ptr<CaptureCallback, AcrossTasksDanglingUntriaged> capture_callback_;
  media::AudioParameters audio_parameters_;
};

std::string MakeGoodResponse() {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  media::mojom::WebSpeechRecognitionResultPtr result =
      media::mojom::WebSpeechRecognitionResult::New();
  result->hypotheses.push_back(media::mojom::SpeechRecognitionHypothesis::New(
      u"Pictures of the moon", 1.0F));
  proto_result->set_final(!result->is_provisional);
  for (const auto& hypothesis : result->hypotheses) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    proto_alternative->set_confidence(hypothesis->confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis->utterance));
  }

  std::string msg_string;
  proto_event.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  auto msg_size_bytes =
      base::U32ToBigEndian(base::checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0u, base::as_string_view(msg_size_bytes));
  return msg_string;
}

class MockSpeechRecognitionSessionClient
    : public media::mojom::SpeechRecognitionSessionClient {
 public:
  MockSpeechRecognitionSessionClient() = default;
  ~MockSpeechRecognitionSessionClient() override = default;

  void ResultRetrieved(std::vector<media::mojom::WebSpeechRecognitionResultPtr>
                           results) override {}

  void ErrorOccurred(media::mojom::SpeechRecognitionErrorPtr error) override {
    error_code_ = error->code;
    event_occurred_ = true;
    if (error_closure_) {
      std::move(error_closure_).Run();
    }
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  void Started() override {
    started_occurred_ = true;
    event_occurred_ = true;
    if (started_closure_) {
      std::move(started_closure_).Run();
    }
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  void AudioStarted() override {
    audio_started_occurred_ = true;
    event_occurred_ = true;
    if (audio_started_closure_) {
      std::move(audio_started_closure_).Run();
    }
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  void SoundStarted() override {}
  void SoundEnded() override {}

  void AudioEnded() override {
    audio_ended_occurred_ = true;
    event_occurred_ = true;
    if (audio_ended_closure_) {
      std::move(audio_ended_closure_).Run();
    }
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  void Ended() override {
    ended_occurred_ = true;
    event_occurred_ = true;
    if (ended_closure_) {
      std::move(ended_closure_).Run();
    }
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  void WaitForStarted() {
    if (started_occurred_) {
      return;
    }
    base::RunLoop run_loop;
    started_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForAudioStarted() {
    if (audio_started_occurred_) {
      return;
    }
    base::RunLoop run_loop;
    audio_started_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForEnded() {
    if (ended_occurred_) {
      return;
    }
    base::RunLoop run_loop;
    ended_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForError() {
    if (error_code_ != media::mojom::SpeechRecognitionErrorCode::kNone) {
      return;
    }
    base::RunLoop run_loop;
    error_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForEvent() {
    if (event_occurred_) {
      return;
    }
    base::RunLoop run_loop;
    event_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  media::mojom::SpeechRecognitionErrorCode error_code() const {
    return error_code_;
  }
  bool audio_started_occurred() const { return audio_started_occurred_; }
  bool audio_ended_occurred() const { return audio_ended_occurred_; }

  void OnDisconnected() {
    event_occurred_ = true;
    if (event_closure_) {
      std::move(event_closure_).Run();
    }
  }

  mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
  BindNewPipeAndPassRemote() {
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&MockSpeechRecognitionSessionClient::OnDisconnected,
                       base::Unretained(this)));
    return remote;
  }

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionSessionClient> receiver_{this};
  base::OnceClosure started_closure_;
  base::OnceClosure audio_started_closure_;
  base::OnceClosure audio_ended_closure_;
  base::OnceClosure ended_closure_;
  base::OnceClosure error_closure_;
  base::OnceClosure event_closure_;

  media::mojom::SpeechRecognitionErrorCode error_code_ =
      media::mojom::SpeechRecognitionErrorCode::kNone;
  bool started_occurred_ = false;
  bool audio_started_occurred_ = false;
  bool audio_ended_occurred_ = false;
  bool ended_occurred_ = false;
  bool event_occurred_ = false;
};

}  // namespace

class SpeechRecognitionBrowserTest : public ContentBrowserTest {
 public:
  enum StreamingServerState {
    kIdle,
    kTestAudioCapturerSourceOpened,
    kTestAudioCapturerSourceClosed,
  };

#if !BUILDFLAG(IS_FUCHSIA)
  SpeechRecognitionBrowserTest() {
    // Setup the SODA On-Device feature flags.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            media::kOnDeviceWebSpeech,
#if BUILDFLAG(IS_CHROMEOS)
            ash::features::kOnDeviceSpeechRecognition,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{});
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

  // Helper methods used by test fixtures.
  GURL GetTestUrlFromFragment(const std::string& fragment) {
    return GURL(GetTestUrl("speech", "web_speech_recognition.html").spec() +
        "#" + fragment);
  }

  std::string GetPageFragment() {
    return shell()->web_contents()->GetLastCommittedURL().GetRef();
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

#if !BUILDFLAG(IS_FUCHSIA)
  // Set SODA On-Device speech recognition features flags.
  base::test::ScopedFeatureList scoped_feature_list_;
  // Setup mock SODA installer
  speech::MockSodaInstaller mock_soda_installer_;
#endif  // !BUILDFLAG(IS_FUCHSIA)

 private:
  void OnCapturerSourceStart(const media::AudioParameters& audio_parameters,
                             CaptureCallback* capture_callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kIdle, streaming_server_state_);
    streaming_server_state_ = kTestAudioCapturerSourceOpened;

    const int capture_packet_interval_ms =
        (1000 * audio_parameters.frames_per_buffer()) /
        audio_parameters.sample_rate();
    ASSERT_EQ(NetworkSpeechRecognitionEngineImpl::kAudioPacketIntervalMs,
              capture_packet_interval_ms);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 500 /* ms */,
                            /*feed_with_noise=*/false);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 1000 /* ms */,
                            /*feed_with_noise=*/true);
    FeedAudioCapturerSource(audio_parameters, capture_callback, 1000 /* ms */,
                            /*feed_with_noise=*/false);
  }

  void OnCapturerSourceStop() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kTestAudioCapturerSourceOpened, streaming_server_state_);
    streaming_server_state_ = kTestAudioCapturerSourceClosed;

    // Reset capturer source so SpeechRecognizerImpl destructor doesn't call
    // AudioCaptureSourcer::Stop() again.
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(nullptr, nullptr);

    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SpeechRecognitionBrowserTest::SendResponse,
                                  base::Unretained(this)));
  }

  void SendResponse() {}

  static void FeedSingleBufferToAudioCapturerSource(
      const media::AudioParameters& audio_params,
      CaptureCallback* capture_callback,
      size_t buffer_size,
      bool fill_with_noise) {
    DCHECK(capture_callback);
    auto audio_buffer = base::HeapArray<uint8_t>::Uninit(buffer_size);
    if (fill_with_noise) {
      for (size_t i = 0; i < buffer_size; ++i)
        audio_buffer[i] =
            static_cast<uint8_t>(127 * sin(i * 3.14F / (16 * buffer_size)));
    } else {
      std::ranges::fill(audio_buffer, 0);
    }

    std::unique_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(audio_params);
    audio_bus->FromInterleavedBytes<media::SignedInt16SampleTypeTraits>(
        audio_buffer);
    capture_callback->Capture(audio_bus.get(), base::TimeTicks::Now(), {}, 0.0);
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
    // NetworkSpeechRecognitionEngineImpl::GetDesiredAudioChunkDurationMs().
    ASSERT_EQ(0, duration_ms % ms_per_buffer);

    const int n_buffers = duration_ms / ms_per_buffer;
    for (int i = 0; i < n_buffers; ++i) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
#if BUILDFLAG(IS_MAC)
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
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
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
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       FrameSessionTrackerMemoryLeak) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();

  const char kTriggerLeakScript[] = R"(
    new Promise(resolve => {
      const SpeechRecognition = window.SpeechRecognition ||
        window.webkitSpeechRecognition;
      const recognition = new SpeechRecognition();
      recognition.onend = () => { resolve("ended"); };
      recognition.onerror = () => { resolve("error"); };
      recognition.start();
    });
  )";

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ("error", EvalJs(rfh, kTriggerLeakScript));
  }
  // Wait for the asynchronously posted cleanup tasks from the IO thread to
  // execute on the UI thread.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       PermissionRevocationStopsAudioCapture) {
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", /*relative_url_is_prefix=*/true);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", /*relative_url_is_prefix=*/true);
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();
  url::Origin origin = rfh->GetLastCommittedOrigin();

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
  ASSERT_TRUE(permission_controller);

  // Set initial microphone permission to GRANTED.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> grant_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::GRANTED, grant_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            grant_future.Get());

  mojo::Remote<media::mojom::SpeechRecognizer> speech_recognizer;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::Create, global_id,
                     speech_recognizer.BindNewPipeAndPassReceiver()));

  MockSpeechRecognitionSessionClient client;
  media::mojom::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::StartSpeechRecognitionRequestParams::New();
  params->client = client.BindNewPipeAndPassRemote();
  mojo::Remote<media::mojom::SpeechRecognitionSession> session_remote;
  params->session_receiver = session_remote.BindNewPipeAndPassReceiver();
  params->continuous = true;

  speech_recognizer->Start(std::move(params));

  // Wait for audio capture to start.
  client.WaitForAudioStarted();

  EXPECT_EQ(1, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));
  EXPECT_EQ(kTestAudioCapturerSourceOpened, streaming_server_state());

  // Revoke microphone permission by changing the setting to DENIED.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> deny_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::DENIED, deny_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            deny_future.Get());

  // Wait for the session to end following permission revocation.
  client.WaitForEnded();

  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNotAllowed,
            client.error_code());
  EXPECT_EQ(kTestAudioCapturerSourceClosed, streaming_server_state());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               global_id) == 0;
  }));

  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       PermissionResetStopsAudioCapture) {
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", /*relative_url_is_prefix=*/true);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", /*relative_url_is_prefix=*/true);
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();
  url::Origin origin = rfh->GetLastCommittedOrigin();

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
  ASSERT_TRUE(permission_controller);

  // Set initial microphone permission to GRANTED.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> grant_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::GRANTED, grant_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            grant_future.Get());

  mojo::Remote<media::mojom::SpeechRecognizer> speech_recognizer;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::Create, global_id,
                     speech_recognizer.BindNewPipeAndPassReceiver()));

  MockSpeechRecognitionSessionClient client;
  media::mojom::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::StartSpeechRecognitionRequestParams::New();
  params->client = client.BindNewPipeAndPassRemote();
  mojo::Remote<media::mojom::SpeechRecognitionSession> session_remote;
  params->session_receiver = session_remote.BindNewPipeAndPassReceiver();
  params->continuous = true;

  speech_recognizer->Start(std::move(params));

  // Wait for audio capture to start.
  client.WaitForAudioStarted();

  EXPECT_EQ(1, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));
  EXPECT_EQ(kTestAudioCapturerSourceOpened, streaming_server_state());

  // Reset microphone permission to ASK.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> reset_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::ASK, reset_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            reset_future.Get());

  // Wait for the session to end following permission reset.
  client.WaitForEnded();

  EXPECT_EQ(media::mojom::SpeechRecognitionErrorCode::kNotAllowed,
            client.error_code());
  EXPECT_EQ(kTestAudioCapturerSourceClosed, streaming_server_state());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               global_id) == 0;
  }));

  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

IN_PROC_BROWSER_TEST_F(
    SpeechRecognitionBrowserTest,
    AudioForwarderSessionUnaffectedByMicrophonePermissionRevocation) {
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", /*relative_url_is_prefix=*/true);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", /*relative_url_is_prefix=*/true);
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();
  url::Origin origin = rfh->GetLastCommittedOrigin();

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
  ASSERT_TRUE(permission_controller);

  // Set initial microphone permission to GRANTED.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> grant_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::GRANTED, grant_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            grant_future.Get());

  mojo::Remote<media::mojom::SpeechRecognizer> speech_recognizer;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SpeechRecognitionDispatcherHost::Create, global_id,
                     speech_recognizer.BindNewPipeAndPassReceiver()));

  MockSpeechRecognitionSessionClient client;
  media::mojom::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::StartSpeechRecognitionRequestParams::New();
  params->client = client.BindNewPipeAndPassRemote();
  mojo::Remote<media::mojom::SpeechRecognitionSession> session_remote;
  params->session_receiver = session_remote.BindNewPipeAndPassReceiver();

  mojo::PendingRemote<media::mojom::SpeechRecognitionAudioForwarder>
      audio_forwarder_remote;
  params->audio_forwarder =
      audio_forwarder_remote.InitWithNewPipeAndPassReceiver();
  params->channel_count = 1;
  params->sample_rate = 16000;

  speech_recognizer->Start(std::move(params));

  // Wait for the session to be fully started.
  client.WaitForStarted();

  EXPECT_EQ(1, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));

  // Revoking microphone permission should NOT affect audio forwarder session.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> deny_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::DENIED, deny_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            deny_future.Get());

  base::RunLoop run_loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
  run_loop.Run();

  // The session should still be active.
  EXPECT_EQ(1, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));
  EXPECT_FALSE(client.audio_ended_occurred());

  // Clean up session by aborting it explicitly.
  session_remote->Abort();
  client.WaitForEnded();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               global_id) == 0;
  }));

  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       WebSpeechPermissionRevocationEndsSession) {
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", /*relative_url_is_prefix=*/true);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", /*relative_url_is_prefix=*/true);
  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderFrameHost* rfh = shell()->web_contents()->GetPrimaryMainFrame();
  content::GlobalRenderFrameHostId global_id = rfh->GetGlobalId();
  url::Origin origin = rfh->GetLastCommittedOrigin();

  PermissionControllerImpl* permission_controller =
      PermissionControllerImpl::FromBrowserContext(rfh->GetBrowserContext());
  ASSERT_TRUE(permission_controller);

  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> grant_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::GRANTED, grant_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            grant_future.Get());

  const char kStartScript[] = R"(
    window.sessionEnded = false;
    window.sessionError = '';
    window.audioStartedPromise = new Promise(resolve => {
      const SpeechRecognition = window.SpeechRecognition ||
                                window.webkitSpeechRecognition;
      window.recognition = new SpeechRecognition();
      window.recognition.continuous = true;
      window.recognition.onaudiostart = () => {
        resolve('audiostarted');
      };
      window.recognition.onerror = (e) => { window.sessionError = e.error; };
      window.endPromise = new Promise(endResolve => {
        window.recognition.onend = () => {
          window.sessionEnded = true;
          endResolve(window.sessionError);
        };
      });
      window.recognition.start();
    });
    'started';
  )";

  EXPECT_EQ("started", EvalJs(rfh, kStartScript));
  EXPECT_EQ("audiostarted", EvalJs(rfh, "window.audioStartedPromise"));

  EXPECT_EQ(1, SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                   global_id));
  EXPECT_EQ(kTestAudioCapturerSourceOpened, streaming_server_state());

  // Revoke microphone permission.
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> deny_future;
  permission_controller->SetPermissionOverride(
      origin, origin, blink::PermissionType::AUDIO_CAPTURE,
      blink::mojom::PermissionStatus::DENIED, deny_future.GetCallback());
  EXPECT_EQ(PermissionControllerImpl::OverrideStatus::kOverrideSet,
            deny_future.Get());

  EXPECT_EQ("not-allowed", EvalJs(rfh, "window.endPromise"));
  EXPECT_EQ(true, EvalJs(rfh, "window.sessionEnded"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               global_id) == 0 &&
           streaming_server_state() == kTestAudioCapturerSourceClosed;
  }));

  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       CompromisedRendererVisibilityBypass) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  mojo::Remote<media::mojom::SpeechRecognizer> speech_recognizer;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpeechRecognitionDispatcherHost::Create,
          shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
          speech_recognizer.BindNewPipeAndPassReceiver()));

  MockSpeechRecognitionSessionClient client;
  media::mojom::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::StartSpeechRecognitionRequestParams::New();
  params->client = client.BindNewPipeAndPassRemote();
  mojo::Remote<media::mojom::SpeechRecognitionSession> session_remote;
  params->session_receiver = session_remote.BindNewPipeAndPassReceiver();

  speech_recognizer->Start(std::move(params));

  // Wait for the session to be fully started and tracked by the manager.
  client.WaitForStarted();

  // Verify the session is tracked.
  EXPECT_EQ(1,
            SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId()));

  // Hide the WebContents to simulate the user switching tabs or backgrounding
  // Chrome.
  shell()->web_contents()->WasHidden();

  // Wait for the browser process to abort the session and signal the client.
  client.WaitForEvent();

  // Without the fix, the session tracker count would NOT be 0 because
  // SpeechRecognitionManagerImpl did not observe visibility changes.
  // The test asserts it is 0 to ensure the secure behavior is enforced.
  EXPECT_EQ(0,
            SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       CompromisedRendererStartWhileHiddenBypass) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Hide the WebContents to simulate the user switching tabs or backgrounding
  // Chrome.
  shell()->web_contents()->WasHidden();

  mojo::Remote<media::mojom::SpeechRecognizer> speech_recognizer;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SpeechRecognitionDispatcherHost::Create,
          shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
          speech_recognizer.BindNewPipeAndPassReceiver()));

  MockSpeechRecognitionSessionClient client;
  media::mojom::StartSpeechRecognitionRequestParamsPtr params =
      media::mojom::StartSpeechRecognitionRequestParams::New();
  params->client = client.BindNewPipeAndPassRemote();
  mojo::Remote<media::mojom::SpeechRecognitionSession> session_remote;
  params->session_receiver = session_remote.BindNewPipeAndPassReceiver();

  speech_recognizer->Start(std::move(params));

  // Wait for the session to either start (vulnerable) or error out/end
  // (secure).
  client.WaitForEvent();

  // Without the fix, the session tracker count would NOT be 0
  // even though the page is hidden, because SpeechRecognitionManagerImpl
  // did not observe visibility changes during Start.
  // The test asserts it is 0 to ensure the secure behavior is enforced.
  EXPECT_EQ(0,
            SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                shell()->web_contents()->GetPrimaryMainFrame()->GetGlobalId()));
}
#endif

#if !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       OnDeviceWebSpeechRecognition) {
  // Speech On-Device not supported.
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }

  std::unique_ptr<MockOnDeviceWebSpeechRecognitionService> mock_speech_service =
      std::make_unique<MockOnDeviceWebSpeechRecognitionService>(
          shell()->web_contents()->GetBrowserContext());

  std::unique_ptr<FakeSpeechRecognitionManagerDelegate>
      fake_speech_recognition_mgr_delegate =
          std::make_unique<FakeSpeechRecognitionManagerDelegate>(
              mock_speech_service.get());
  SodaSpeechRecognitionEngineImpl::
      SetSpeechRecognitionManagerDelegateForTesting(
          fake_speech_recognition_mgr_delegate.get());

  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillRepeatedly(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

  bool has_reponsed = false;
  EXPECT_CALL(*mock_speech_service, SendAudioToSpeechRecognitionService(_, _))
      .WillRepeatedly([&](media::mojom::AudioDataS16Ptr data,
                          std::optional<base::TimeDelta> media_start_pts) {
        if (!has_reponsed) {
          has_reponsed = true;
          media::SpeechRecognitionResult result =
              media::SpeechRecognitionResult(kWebSpeechExpectGoodResult1, true);
          GetIOThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&MockOnDeviceWebSpeechRecognitionService::
                                 SendSpeechRecognitionResult,
                             mock_speech_service->GetWeakPtr(),
                             std::move(result)));
        }
      });

  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(GetTestUrlFromFragment("oneshot"));
  navigation_observer.Wait();

  EXPECT_EQ(kTestAudioCapturerSourceClosed, streaming_server_state());
  EXPECT_EQ(kWebSpeechPageGoodResult1, GetPageFragment());

  base::RunLoop().RunUntilIdle();

  // clean
  SodaSpeechRecognitionEngineImpl::
      SetSpeechRecognitionManagerDelegateForTesting(nullptr);
  fake_speech_recognition_mgr_delegate->Reset(nullptr);
  fake_speech_recognition_mgr_delegate.reset();
  // Clear raw_ptr<content::BrowserContext> before object released.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<MockOnDeviceWebSpeechRecognitionService>
                            mock_service) { mock_service.reset(); },
                     std::move(mock_speech_service)));
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest,
                       NonDefaultPartitionThrowsError) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }
  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillRepeatedly(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

  auto* browser_context = shell()->web_contents()->GetBrowserContext();
  auto storage_partition_config = StoragePartitionConfig::Create(
      browser_context, "SpeechRecognitionBrowserTest", "FixedStoragePartition",
      true);
  ASSERT_TRUE(embedded_test_server()->Start());
  auto url = embedded_test_server()->GetURL("/");
  auto* shell = Shell::CreateNewWindow(
      browser_context, url,
      SiteInstanceImpl::CreateForFixedStoragePartition(
          browser_context, url, storage_partition_config),
      gfx::Size());

  auto GetSiteInstance = [](Shell* shell) {
    return static_cast<SiteInstanceImpl*>(
        shell->web_contents()->GetSiteInstance());
  };

  EXPECT_EQ(GetSiteInstance(shell)
                ->GetSecurityPrincipal()
                .GetStoragePartitionConfig(),
            storage_partition_config);
  EXPECT_TRUE(GetSiteInstance(shell)->IsFixedStoragePartition());

  ASSERT_TRUE(
      NavigateToURL(shell, embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(GetSiteInstance(shell)
                ->GetSecurityPrincipal()
                .GetStoragePartitionConfig(),
            storage_partition_config);
  EXPECT_TRUE(GetSiteInstance(shell)->IsFixedStoragePartition());

  std::string js_to_execute = R"(
    new Promise((resolve, reject) => {
      try {
        var recognition = new webkitSpeechRecognition();
        var error_received = false;

        recognition.continuous = false;
        recognition.interimResults = false;
        recognition.mode = 'ondevice-only';

        recognition.onstart = function(event) {
          console.log('onstart');
        };
        recognition.onaudiostart = function(event) {
          console.log('onaudiostart');
        };
        recognition.onsoundstart = function(event) {
          console.log('onsoundstart');
        };
        recognition.onspeechstart = function(event) {
          console.log('onspeechstart');
        };
        recognition.onspeechend = function(event) {
          console.log('onspeechend');
        };
        recognition.onsoundend = function(event) {
          console.log('onsoundend');
        };
        recognition.onaudioend = function(event) {
          console.log('onaudioend');
        };
        recognition.onresult = function(event) {
          console.log('onresult');
          resolve();
        };
        recognition.onnomatch = function(event) {
          console.log('onnomatch');
          resolve();
        };
        recognition.onerror = function(event) {
          console.log('onerror from ExecJs: ' + event.error);
          if (error_received) { resolve(); return; }
          error_received = true;
          window.location.hash = 'error_' + event.error;
          resolve();
        };
        recognition.start();
      } catch (e) {
        window.location.hash = 'error_js_exception_in_execjs_' + e.name;
        resolve();
      }
    });
  )";

  ASSERT_TRUE(
      ExecJs(shell->web_contents()->GetPrimaryMainFrame(), js_to_execute));
  EXPECT_THAT(shell->web_contents()->GetLastCommittedURL().GetRef(),
              testing::HasSubstr("error_service-not-allowed"));
}

class SpeechRecognitionCrossOriginBrowserTest
    : public SpeechRecognitionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SpeechRecognitionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("use-fake-device-for-media-stream");
    command_line->AppendSwitch("use-fake-ui-for-media-stream");
    command_line->AppendSwitchASCII("autoplay-policy",
                                    "no-user-gesture-required");
    command_line->AppendSwitchASCII("enable-blink-features",
                                    "MediaStreamTrackWebSpeech");
  }
};

IN_PROC_BROWSER_TEST_F(SpeechRecognitionCrossOriginBrowserTest,
                       OnDeviceWebSpeechCrossOriginIframeBypass) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }
  mock_soda_installer_.NotifySodaInstalledForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL main_url = embedded_test_server()->GetURL("127.0.0.1", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL iframe_url = embedded_test_server()->GetURL("localhost", "/empty.html");
  std::string js_add_iframe =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'myiframe';"
      "iframe.allow = 'microphone';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(
      ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), js_add_iframe));
  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), "myiframe", iframe_url));

  RenderFrameHost* iframe_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe_rfh);
  EXPECT_EQ(iframe_url, iframe_rfh->GetLastCommittedURL());

  const char js_to_execute[] = R"(
    new Promise(async resolve => {
      try {
        let stream = await navigator.mediaDevices.getUserMedia({audio: true});
        let track = stream.getAudioTracks()[0];
        if (!track) { resolve('no-track'); return; }

        let recognition = new webkitSpeechRecognition();
        recognition.onerror = function(event) {
          resolve('error_' + event.error);
        };
        recognition.onstart = function() {
          // do not resolve
        };
        recognition.onend = function() {
          resolve('ended');
        };

        setTimeout(() => resolve('timeout_in_js'), 5000);

        recognition.start(track);
      } catch (e) {
        resolve('exception_' + e.name);
      }
    })
  )";

  EXPECT_EQ("error_network", EvalJs(iframe_rfh, js_to_execute));

  // Remove reference to URL string that's on the stack.
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionCrossOriginBrowserTest,
                       OnDeviceWebSpeechCrossOriginIframeBypassProcessLocally) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }
  mock_soda_installer_.NotifySodaInstalledForTesting();

  ASSERT_TRUE(embedded_test_server()->Start());

  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  GURL main_url = embedded_test_server()->GetURL("127.0.0.1", "/empty.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  GURL iframe_url = embedded_test_server()->GetURL("localhost", "/empty.html");
  std::string js_add_iframe =
      "var iframe = document.createElement('iframe');"
      "iframe.id = 'myiframe';"
      "iframe.allow = 'microphone';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(
      ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), js_add_iframe));
  EXPECT_TRUE(
      NavigateIframeToURL(shell()->web_contents(), "myiframe", iframe_url));

  RenderFrameHost* iframe_rfh =
      ChildFrameAt(shell()->web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(iframe_rfh);
  EXPECT_EQ(iframe_url, iframe_rfh->GetLastCommittedURL());

  const char js_to_execute[] = R"(
    new Promise(async resolve => {
      try {
        let stream = await navigator.mediaDevices.getUserMedia({audio: true});
        let track = stream.getAudioTracks()[0];
        if (!track) { resolve('no-track'); return; }

        let recognition = new webkitSpeechRecognition();
        recognition.processLocally = true;
        recognition.onerror = function(event) {
          resolve('error_' + event.error);
        };
        recognition.onstart = function() {
          // do not resolve
        };
        recognition.onend = function() {
          resolve('ended');
        };

        setTimeout(() => resolve('timeout_in_js'), 5000);

        recognition.start(track);
      } catch (e) {
        resolve('exception_' + e.name);
      }
    })
  )";

  EXPECT_EQ("exception_NotAllowedError", EvalJs(iframe_rfh, js_to_execute));

  // Remove reference to URL string that's on the stack.
  NetworkSpeechRecognitionEngineImpl::set_web_service_base_url_for_tests(
      nullptr);
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

}  // namespace content
