// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#elif defined(OS_ANDROID)
#include "media/audio/android/audio_manager_android.h"
#elif defined(OS_MACOSX)
#include "media/audio/mac/audio_manager_mac.h"
#elif defined(OS_WIN)
#include "media/audio/win/audio_manager_win.h"
#else
#include "media/audio/fake_audio_manager.h"
#endif

using blink::mojom::StreamSelectionInfo;
using blink::mojom::StreamSelectionInfoPtr;
using blink::mojom::StreamSelectionStrategy;
using testing::_;
using testing::Invoke;

namespace content {

#if defined(USE_ALSA)
typedef media::AudioManagerAlsa AudioManagerPlatform;
#elif defined(OS_MACOSX)
typedef media::AudioManagerMac AudioManagerPlatform;
#elif defined(OS_WIN)
typedef media::AudioManagerWin AudioManagerPlatform;
#elif defined(OS_ANDROID)
typedef media::AudioManagerAndroid AudioManagerPlatform;
#else
typedef media::FakeAudioManager AudioManagerPlatform;
#endif

namespace {

const char kMockSalt[] = "";
const char kFakeDeviceIdPrefix[] = "fake_device_id_";

// This class mocks the audio manager and overrides some methods to ensure that
// we can run our tests on the buildbots.
class MockAudioManager : public AudioManagerPlatform {
 public:
  MockAudioManager()
      : AudioManagerPlatform(std::make_unique<media::TestAudioThread>(),
                             &fake_audio_log_factory_),
        num_output_devices_(2),
        num_input_devices_(2) {}
  ~MockAudioManager() override {}

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_input_devices_ > 0)
      device_names->push_back(media::AudioDeviceName::CreateDefault());

    for (size_t i = 0; i < num_input_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::NumberToString(i),
          std::string(kFakeDeviceIdPrefix) + base::NumberToString(i)));
    }
  }

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_output_devices_ > 0)
      device_names->push_back(media::AudioDeviceName::CreateDefault());

    for (size_t i = 0; i < num_output_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::NumberToString(i),
          std::string(kFakeDeviceIdPrefix) + base::NumberToString(i)));
    }
  }

  media::AudioParameters GetDefaultOutputStreamParameters() override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 128);
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 128);
  }

  void SetNumAudioOutputDevices(size_t num_devices) {
    num_output_devices_ = num_devices;
  }

  void SetNumAudioInputDevices(size_t num_devices) {
    num_input_devices_ = num_devices;
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  size_t num_output_devices_;
  size_t num_input_devices_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

class MockMediaObserver : public MediaObserver {
 public:
  MOCK_METHOD0(OnAudioCaptureDevicesChanged, void());
  MOCK_METHOD0(OnVideoCaptureDevicesChanged, void());
  MOCK_METHOD6(OnMediaRequestStateChanged,
               void(int,
                    int,
                    int,
                    const GURL&,
                    blink::mojom::MediaStreamType,
                    MediaRequestState));
  MOCK_METHOD2(OnCreatingAudioStream, void(int, int));
  MOCK_METHOD5(OnSetCapturingLinkSecured,
               void(int, int, int, blink::mojom::MediaStreamType, bool));
};

class TestBrowserClient : public ContentBrowserClient {
 public:
  explicit TestBrowserClient(MediaObserver* media_observer)
      : media_observer_(media_observer) {}
  ~TestBrowserClient() override {}
  MediaObserver* GetMediaObserver() override { return media_observer_; }

 private:
  MediaObserver* media_observer_;
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MockMediaStreamUIProxy()
      : FakeMediaStreamUIProxy(/*tests_use_fake_render_frame_hosts=*/true) {}
  void RequestAccess(std::unique_ptr<MediaStreamRequest> request,
                     ResponseCallback response_callback) override {
    MockRequestAccess(request, response_callback);
  }

  MOCK_METHOD2(MockRequestAccess,
               void(std::unique_ptr<MediaStreamRequest>& request,
                    ResponseCallback& response_callback));
};

}  // namespace

class MediaStreamManagerTest : public ::testing::Test {
 public:
  MediaStreamManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    audio_manager_ = std::make_unique<MockAudioManager>();
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    auto video_capture_provider = std::make_unique<MockVideoCaptureProvider>();
    video_capture_provider_ = video_capture_provider.get();
    service_manager_context_ = std::make_unique<TestServiceManagerContext>();
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), audio_manager_->GetTaskRunner(),
        std::move(video_capture_provider));
    media_observer_ = std::make_unique<MockMediaObserver>();
    browser_content_client_ =
        std::make_unique<TestBrowserClient>(media_observer_.get());
    SetBrowserClientForTesting(browser_content_client_.get());
    base::RunLoop().RunUntilIdle();

    ON_CALL(*video_capture_provider_, DoGetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [](VideoCaptureProvider::GetDeviceInfosCallback& result_callback) {
              std::vector<media::VideoCaptureDeviceInfo> stub_results;
              std::move(result_callback).Run(stub_results);
            }));
  }

  ~MediaStreamManagerTest() override {
    audio_manager_->Shutdown();
    service_manager_context_.reset();
  }

  MOCK_METHOD1(Response, void(int index));
  void ResponseCallback(int index,
                        const blink::MediaStreamDevices& devices,
                        std::unique_ptr<MediaStreamUIProxy> ui_proxy) {
    Response(index);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 protected:
  std::string MakeMediaAccessRequest(int index) {
    const int render_process_id = 1;
    const int render_frame_id = 1;
    const int requester_id = 1;
    const int page_request_id = 1;
    const url::Origin security_origin;
    MediaStreamManager::MediaAccessRequestCallback callback =
        base::BindOnce(&MediaStreamManagerTest::ResponseCallback,
                       base::Unretained(this), index);
    blink::StreamControls controls(true, true);
    return media_stream_manager_->MakeMediaAccessRequest(
        render_process_id, render_frame_id, requester_id, page_request_id,
        controls, security_origin, std::move(callback));
  }

  void RequestAndStopGetDisplayMedia(bool request_audio) {
    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
      return std::make_unique<FakeMediaStreamUIProxy>(
          true
          /*tests_use_fake_render_frame_hosts=*/);
    }));

    blink::StreamControls controls(request_audio /* request_audio */,
                                   true /* request_video */);
    controls.video.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
    if (request_audio)
      controls.audio.stream_type =
          blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
    const int render_process_id = 1;
    const int render_frame_id = 1;
    const int requester_id = 1;
    const int page_request_id = 1;

    blink::MediaStreamDevice video_device;
    blink::MediaStreamDevice audio_device;
    MediaStreamManager::GenerateStreamCallback generate_stream_callback =
        base::BindOnce(GenerateStreamCallback, &run_loop_, request_audio,
                       true /* request_video */, &audio_device, &video_device);
    MediaStreamManager::DeviceStoppedCallback stopped_callback;
    MediaStreamManager::DeviceChangedCallback changed_callback;

    std::vector<blink::mojom::MediaStreamType> expected_types;
    expected_types.push_back(
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE);
    if (request_audio)
      expected_types.push_back(
          blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE);
    for (blink::mojom::MediaStreamType expected_type : expected_types) {
      EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(
                                        _, _, _, _, expected_type,
                                        MEDIA_REQUEST_STATE_PENDING_APPROVAL));
      EXPECT_CALL(*media_observer_,
                  OnMediaRequestStateChanged(_, _, _, _, expected_type,
                                             MEDIA_REQUEST_STATE_OPENING));
      EXPECT_CALL(*media_observer_,
                  OnMediaRequestStateChanged(_, _, _, _, expected_type,
                                             MEDIA_REQUEST_STATE_DONE));
    }
    media_stream_manager_->GenerateStream(
        render_process_id, render_frame_id, requester_id, page_request_id,
        controls, MediaDeviceSaltAndOrigin(), false /* user_gesture */,
        StreamSelectionInfo::New(
            blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
            base::nullopt),
        std::move(generate_stream_callback), std::move(stopped_callback),
        std::move(changed_callback));
    run_loop_.Run();

    EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
              video_device.type);
    if (request_audio)
      EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                audio_device.type);

    EXPECT_CALL(
        *media_observer_,
        OnMediaRequestStateChanged(
            _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            MEDIA_REQUEST_STATE_CLOSING));
    media_stream_manager_->StopStreamDevice(render_process_id, render_frame_id,
                                            requester_id, video_device.id,
                                            video_device.session_id());
    if (request_audio) {
      EXPECT_CALL(
          *media_observer_,
          OnMediaRequestStateChanged(
              _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
              MEDIA_REQUEST_STATE_CLOSING));
      media_stream_manager_->StopStreamDevice(
          render_process_id, render_frame_id, requester_id, audio_device.id,
          audio_device.session_id());
    }
  }

  static void GenerateStreamCallback(
      base::RunLoop* wait_loop,
      bool request_audio,
      bool request_video,
      blink::MediaStreamDevice* audio_device,
      blink::MediaStreamDevice* video_device,
      blink::mojom::MediaStreamRequestResult result,
      const std::string& label,
      const blink::MediaStreamDevices& audio_devices,
      const blink::MediaStreamDevices& video_devices) {
    if (request_audio) {
      EXPECT_EQ(1u, audio_devices.size());
      *audio_device = audio_devices[0];
    } else {
      EXPECT_EQ(0u, audio_devices.size());
    }

    if (request_video) {
      ASSERT_EQ(1u, video_devices.size());
      *video_device = video_devices[0];
    } else {
      EXPECT_EQ(0u, video_devices.size());
    }

    wait_loop->Quit();
  }

  blink::MediaStreamDevice CreateOrSearchAudioDeviceStream(
      const StreamSelectionStrategy& strategy,
      const base::Optional<base::UnguessableToken>& session_id,
      const blink::StreamControls& controls =
          blink::StreamControls(true /* request_audio */,
                                false /* request_video */),
      int render_process_id = 1,
      int render_frame_id = 1,
      int requester_id = 1,
      int page_request_id = 1) {
    base::RunLoop run_loop;
    blink::MediaStreamDevice audio_device;

    MediaStreamManager::GenerateStreamCallback generate_stream_callback =
        base::BindOnce(GenerateStreamCallback, &run_loop, true, false,
                       &audio_device, nullptr);
    MediaStreamManager::DeviceStoppedCallback stopped_callback;
    MediaStreamManager::DeviceChangedCallback changed_callback;

    StreamSelectionInfoPtr info =
        StreamSelectionInfo::New(strategy, session_id);
    media_stream_manager_->GenerateStream(
        render_process_id, render_frame_id, requester_id, page_request_id,
        controls, MediaDeviceSaltAndOrigin(), false /* user_gesture */,
        std::move(info), std::move(generate_stream_callback),
        std::move(stopped_callback), std::move(changed_callback));
    run_loop.Run();

    return audio_device;
  }

  // media_stream_manager_ needs to outlive task_environment_ because it is a
  // MessageLoopCurrent::DestructionObserver. audio_manager_ needs to outlive
  // task_environment_ because it uses the underlying message loop.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<MockMediaObserver> media_observer_;
  std::unique_ptr<ContentBrowserClient> browser_content_client_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  MockVideoCaptureProvider* video_capture_provider_;
  base::RunLoop run_loop_;

 private:
  std::unique_ptr<TestServiceManagerContext> service_manager_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamManagerTest);
};

TEST_F(MediaStreamManagerTest, MakeMediaAccessRequest) {
  MakeMediaAccessRequest(0);
  EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(_, _, _, _, _, _))
      .Times(testing::AtLeast(1));

  // Expecting the callback will be triggered and quit the test.
  EXPECT_CALL(*this, Response(0));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMediaAccessRequest) {
  std::string label = MakeMediaAccessRequest(0);

  // Request cancellation notifies closing of all stream types.
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  media_stream_manager_->CancelRequest(label);
  run_loop_.RunUntilIdle();
}

TEST_F(MediaStreamManagerTest, MakeMultipleRequests) {
  // First request.
  std::string label1 = MakeMediaAccessRequest(0);

  // Second request.
  int render_process_id = 2;
  int render_frame_id = 2;
  int requester_id = 2;
  int page_request_id = 2;
  url::Origin security_origin;
  blink::StreamControls controls(true, true);
  MediaStreamManager::MediaAccessRequestCallback callback = base::BindOnce(
      &MediaStreamManagerTest::ResponseCallback, base::Unretained(this), 1);
  std::string label2 = media_stream_manager_->MakeMediaAccessRequest(
      render_process_id, render_frame_id, requester_id, page_request_id,
      controls, security_origin, std::move(callback));

  // Expecting the callbackS from requests will be triggered and quit the test.
  // Note, the callbacks might come in a different order depending on the
  // value of labels.
  EXPECT_CALL(*this, Response(0));
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMultipleRequests) {
  std::string label1 = MakeMediaAccessRequest(0);
  std::string label2 = MakeMediaAccessRequest(1);

  // Cancelled request notifies closing of all stream types.
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_CLOSING));

  media_stream_manager_->CancelRequest(label1);

  // The request that proceeds sets state to MEDIA_REQUEST_STATE_REQUESTED when
  // starting a device enumeration, and to MEDIA_REQUEST_STATE_PENDING_APPROVAL
  // when the enumeration is completed and also when the request is posted to
  // the UI.
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_REQUESTED));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_REQUESTED));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
          MEDIA_REQUEST_STATE_PENDING_APPROVAL))
      .Times(2);
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_PENDING_APPROVAL))
      .Times(2);

  // Expecting the callback from the second request will be triggered and
  // quit the test.
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, DeviceID) {
  url::Origin security_origin = url::Origin::Create(GURL("http://localhost"));
  const std::string unique_default_id(
      media::AudioDeviceDescription::kDefaultDeviceId);
  const std::string hashed_default_id =
      MediaStreamManager::GetHMACForMediaDeviceID(kMockSalt, security_origin,
                                                  unique_default_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      kMockSalt, security_origin, hashed_default_id, unique_default_id));
  EXPECT_EQ(unique_default_id, hashed_default_id);

  const std::string unique_communications_id(
      media::AudioDeviceDescription::kCommunicationsDeviceId);
  const std::string hashed_communications_id =
      MediaStreamManager::GetHMACForMediaDeviceID(kMockSalt, security_origin,
                                                  unique_communications_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      kMockSalt, security_origin, hashed_communications_id,
      unique_communications_id));
  EXPECT_EQ(unique_communications_id, hashed_communications_id);

  const std::string unique_other_id("other-unique-id");
  const std::string hashed_other_id =
      MediaStreamManager::GetHMACForMediaDeviceID(kMockSalt, security_origin,
                                                  unique_other_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      kMockSalt, security_origin, hashed_other_id, unique_other_id));
  EXPECT_NE(unique_other_id, hashed_other_id);
  EXPECT_EQ(hashed_other_id.size(), 64U);
  for (const char& c : hashed_other_id)
    EXPECT_TRUE(base::IsAsciiDigit(c) || (c >= 'a' && c <= 'f'));
}

TEST_F(MediaStreamManagerTest, GenerateSameStreamForAudioDevice) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        true /* tests_use_fake_render_frame_hosts */);
  }));

  const int num_call_iterations = 3;

  // Test that if |info.strategy| has value SEARCH_BY_DEVICE_ID, we only create
  // a single session for a device.
  std::set<base::UnguessableToken> session_ids;
  for (int i = 0; i < num_call_iterations; ++i) {
    blink::MediaStreamDevice audio_device = CreateOrSearchAudioDeviceStream(
        blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
        base::nullopt);

    EXPECT_EQ(audio_device.id, "default");
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              audio_device.type);
    EXPECT_TRUE(audio_device.session_id());
    session_ids.insert(audio_device.session_id());
  }
  EXPECT_EQ(session_ids.size(), 1u);
}

TEST_F(MediaStreamManagerTest, GenerateDifferentStreamsForAudioDevice) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        true /* tests_use_fake_render_frame_hosts */);
  }));

  const size_t num_call_iterations = 3;

  // Test that if |info.strategy| is provided as FORCE_NEW_STREAM, we create a
  // new stream each time.
  std::set<base::UnguessableToken> session_ids;
  for (size_t i = 0; i < num_call_iterations; ++i) {
    blink::MediaStreamDevice audio_device = CreateOrSearchAudioDeviceStream(
        blink::mojom::StreamSelectionStrategy::FORCE_NEW_STREAM, base::nullopt);

    EXPECT_EQ(audio_device.id, "default");
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              audio_device.type);
    EXPECT_TRUE(audio_device.session_id());
    session_ids.insert(audio_device.session_id());
  }
  EXPECT_EQ(session_ids.size(), num_call_iterations);
}

TEST_F(MediaStreamManagerTest, GenerateAndReuseStreamForAudioDevice) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        true /* tests_use_fake_render_frame_hosts */);
  }));

  const int num_call_iterations = 3;

  // Test that if |info.strategy| is provided as SEARCH_BY_SESSION_ID with
  // |info.session_id| set to an non-existing ID a new stream is provided and
  // that if the ID is valid, that the stream is reused.
  auto token = base::UnguessableToken::Create();
  blink::MediaStreamDevice reference_device = CreateOrSearchAudioDeviceStream(
      blink::mojom::StreamSelectionStrategy::SEARCH_BY_SESSION_ID, token);
  EXPECT_NE(reference_device.session_id(), token);

  for (int i = 0; i < num_call_iterations; ++i) {
    blink::MediaStreamDevice audio_device = CreateOrSearchAudioDeviceStream(
        blink::mojom::StreamSelectionStrategy::SEARCH_BY_SESSION_ID,
        reference_device.session_id());
    EXPECT_EQ(audio_device.id, "default");
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              audio_device.type);
    EXPECT_TRUE(audio_device.session_id());
    EXPECT_EQ(audio_device.session_id(), reference_device.session_id());
  }
}

TEST_F(MediaStreamManagerTest, GetDisplayMediaRequestVideoOnly) {
  RequestAndStopGetDisplayMedia(false /* request_audio */);
}

TEST_F(MediaStreamManagerTest, GetDisplayMediaRequestAudioAndVideo) {
  RequestAndStopGetDisplayMedia(true /* request_audio */);
}

TEST_F(MediaStreamManagerTest, GetDisplayMediaRequestCallsUIProxy) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
      [](base::RunLoop* run_loop) {
        auto mock_ui = std::make_unique<MockMediaStreamUIProxy>();
        EXPECT_CALL(*mock_ui, MockRequestAccess(_, _))
            .WillOnce(testing::Invoke(
                [run_loop](std::unique_ptr<MediaStreamRequest>& request,
                           testing::Unused) {
                  EXPECT_EQ(
                      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                      request->video_type);
                  run_loop->Quit();
                }));
        return std::unique_ptr<FakeMediaStreamUIProxy>(std::move(mock_ui));
      },
      &run_loop_));
  blink::StreamControls controls(false /* request_audio */,
                                 true /* request_video */);
  controls.video.stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;

  MediaStreamManager::GenerateStreamCallback generate_stream_callback =
      base::BindOnce([](blink::mojom::MediaStreamRequestResult result,
                        const std::string& label,
                        const blink::MediaStreamDevices& audio_devices,
                        const blink::MediaStreamDevices& video_devices) {});
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_PENDING_APPROVAL));
  const int render_process_id = 0;
  const int render_frame_id = 0;
  const int requester_id = 0;
  const int page_request_id = 0;
  media_stream_manager_->GenerateStream(
      render_process_id, render_frame_id, requester_id, page_request_id,
      controls, MediaDeviceSaltAndOrigin(), false /* user_gesture */,
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          base::nullopt),
      std::move(generate_stream_callback),
      MediaStreamManager::DeviceStoppedCallback(),
      MediaStreamManager::DeviceChangedCallback());
  run_loop_.Run();

  EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(_, _, _, _, _, _))
      .Times(testing::AtLeast(1));
  media_stream_manager_->CancelAllRequests(0, 0, 0);
}

TEST_F(MediaStreamManagerTest, DesktopCaptureDeviceStopped) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        /*tests_use_fake_render_frame_hosts=*/true);
  }));

  blink::StreamControls controls(false /* request_audio */,
                                 true /* request_video */);
  controls.video.stream_type =
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  const int render_process_id = 1;
  const int render_frame_id = 1;
  const int requester_id = 1;
  const int page_request_id = 1;

  blink::MediaStreamDevice video_device;
  MediaStreamManager::GenerateStreamCallback generate_stream_callback =
      base::BindOnce(GenerateStreamCallback, &run_loop_,
                     false /* request_audio */, true /* request_video */,
                     nullptr, &video_device);
  MediaStreamManager::DeviceStoppedCallback stopped_callback =
      base::BindRepeating(
          [](const std::string& label, const blink::MediaStreamDevice& device) {
            EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
                      device.type);
            EXPECT_NE(DesktopMediaID::TYPE_NONE,
                      DesktopMediaID::Parse(device.id).type);
          });
  MediaStreamManager::DeviceChangedCallback changed_callback;
  EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(_, _, _, _, _, _))
      .Times(testing::AtLeast(1));

  media_stream_manager_->GenerateStream(
      render_process_id, render_frame_id, requester_id, page_request_id,
      controls, MediaDeviceSaltAndOrigin(), false /* user_gesture */,
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          base::nullopt),
      std::move(generate_stream_callback), std::move(stopped_callback),
      std::move(changed_callback));
  run_loop_.Run();
  EXPECT_EQ(controls.video.stream_type, video_device.type);
  EXPECT_NE(DesktopMediaID::TYPE_NONE,
            DesktopMediaID::Parse(video_device.id).type);

  // |request_label| is cached in the |device.name| for testing purpose.
  std::string request_label = video_device.name;
  media_stream_manager_->StopMediaStreamFromBrowser(request_label);

  media_stream_manager_->StopStreamDevice(render_process_id, render_frame_id,
                                          requester_id, video_device.id,
                                          video_device.session_id());
}

TEST_F(MediaStreamManagerTest, DesktopCaptureDeviceChanged) {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        /*tests_use_fake_render_frame_hosts=*/true);
  }));

  blink::StreamControls controls(false /* request_audio */,
                                 true /* request_video */);
  controls.video.stream_type =
      blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  const int render_process_id = 1;
  const int render_frame_id = 1;
  const int requester_id = 1;
  const int page_request_id = 1;

  blink::MediaStreamDevice video_device;
  MediaStreamManager::GenerateStreamCallback generate_stream_callback =
      base::BindOnce(GenerateStreamCallback, &run_loop_,
                     false /* request_audio */, true /* request_video */,
                     nullptr, &video_device);
  MediaStreamManager::DeviceStoppedCallback stopped_callback;
  MediaStreamManager::DeviceChangedCallback changed_callback =
      base::BindRepeating(
          [](blink::MediaStreamDevice* video_device, const std::string& label,
             const blink::MediaStreamDevice& old_device,
             const blink::MediaStreamDevice& new_device) {
            EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
                      old_device.type);
            EXPECT_NE(DesktopMediaID::TYPE_NONE,
                      DesktopMediaID::Parse(old_device.id).type);
            EXPECT_EQ(blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
                      new_device.type);
            EXPECT_NE(DesktopMediaID::TYPE_NONE,
                      DesktopMediaID::Parse(new_device.id).type);
            *video_device = new_device;
          },
          &video_device);
  EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(_, _, _, _, _, _))
      .Times(testing::AtLeast(1));

  media_stream_manager_->GenerateStream(
      render_process_id, render_frame_id, requester_id, page_request_id,
      controls, MediaDeviceSaltAndOrigin(), false /* user_gesture */,
      StreamSelectionInfo::New(
          blink::mojom::StreamSelectionStrategy::SEARCH_BY_DEVICE_ID,
          base::nullopt),
      std::move(generate_stream_callback), std::move(stopped_callback),
      std::move(changed_callback));
  run_loop_.Run();
  EXPECT_EQ(controls.video.stream_type, video_device.type);
  EXPECT_NE(DesktopMediaID::TYPE_NONE,
            DesktopMediaID::Parse(video_device.id).type);

  // |request_label| is cached in the |device.name| for testing purpose.
  std::string request_label = video_device.name;
  media_stream_manager_->ChangeMediaStreamSourceFromBrowser(request_label,
                                                            DesktopMediaID());

  // Wait to check callbacks before stopping the device.
  base::RunLoop().RunUntilIdle();
  media_stream_manager_->StopStreamDevice(render_process_id, render_frame_id,
                                          requester_id, video_device.id,
                                          video_device.session_id());
}

TEST_F(MediaStreamManagerTest, GetMediaDeviceIDForHMAC) {
  const char kSalt[] = "my salt";
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
  const std::string kExistingRawDeviceId =
      std::string(kFakeDeviceIdPrefix) + "0";
  const std::string kExistingHmacDeviceId =
      MediaStreamManager::GetHMACForMediaDeviceID(kSalt, kOrigin,
                                                  kExistingRawDeviceId);

  MediaStreamManager::GetMediaDeviceIDForHMAC(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, kSalt, kOrigin,
      kExistingHmacDeviceId, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(
          [](const std::string& expected_raw_device_id,
             const base::Optional<std::string>& raw_device_id) {
            ASSERT_TRUE(raw_device_id.has_value());
            EXPECT_EQ(*raw_device_id, expected_raw_device_id);
          },
          kExistingRawDeviceId));
  base::RunLoop().RunUntilIdle();

  const std::string kNonexistingHmacDeviceId = "does not exist";
  MediaStreamManager::GetMediaDeviceIDForHMAC(
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, kSalt, kOrigin,
      kNonexistingHmacDeviceId, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce([](const base::Optional<std::string>& raw_device_id) {
        EXPECT_FALSE(raw_device_id.has_value());
      }));
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
