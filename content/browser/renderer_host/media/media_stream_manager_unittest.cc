// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/media_observer.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_captured_surface_controller.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "media/capture/content/screen_enumerator.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#elif BUILDFLAG(IS_ANDROID)
#include "media/audio/android/audio_manager_android.h"
#elif BUILDFLAG(IS_MAC)
#include "media/audio/mac/audio_manager_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "media/audio/win/audio_manager_win.h"
#else
#include "media/audio/fake_audio_manager.h"
#endif

using ::blink::mojom::CapturedSurfaceControlResult;
using ::blink::mojom::MediaStreamType;
using ::blink::mojom::StreamSelectionInfo;
using ::blink::mojom::StreamSelectionInfoPtr;
using ::testing::_;
using testing::ElementsAre;
using ::testing::Invoke;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
using CapturedWheelAction = ::blink::mojom::CapturedWheelAction;
using CapturedWheelActionPtr = ::blink::mojom::CapturedWheelActionPtr;
using CapturedSurfaceControllerFactoryCallback =
    ::content::MediaStreamManager::CapturedSurfaceControllerFactoryCallback;
#endif

using DeviceStoppedCallback =
    ::content::MediaStreamManager::DeviceStoppedCallback;

namespace content {

#if defined(USE_ALSA)
typedef media::AudioManagerAlsa AudioManagerPlatform;
#elif BUILDFLAG(IS_MAC)
typedef media::AudioManagerMac AudioManagerPlatform;
#elif BUILDFLAG(IS_WIN)
typedef media::AudioManagerWin AudioManagerPlatform;
#elif BUILDFLAG(IS_ANDROID)
typedef media::AudioManagerAndroid AudioManagerPlatform;
#else
typedef media::FakeAudioManager AudioManagerPlatform;
#endif

namespace {

const char kFakeDeviceIdPrefix[] = "fake_device_id_";
const GlobalRenderFrameHostId kRenderFrameHostId{1, 2};

std::string GetAudioInputDeviceId(size_t index) {
  return std::string(kFakeDeviceIdPrefix) + base::NumberToString(index);
}

std::string GetAudioInputDeviceName(size_t index) {
  return std::string("fake_device_name_") + base::NumberToString(index);
}

// This class mocks the audio manager and overrides some methods to ensure that
// we can run our tests on the buildbots.
class MockAudioManager : public AudioManagerPlatform {
 public:
  MockAudioManager()
      : AudioManagerPlatform(std::make_unique<media::TestAudioThread>(),
                             &fake_audio_log_factory_),
        num_output_devices_(2),
        num_input_devices_(2) {}

  MockAudioManager(const MockAudioManager&) = delete;
  MockAudioManager& operator=(const MockAudioManager&) = delete;

  ~MockAudioManager() override = default;

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_input_devices_ > 0)
      device_names->push_back(media::AudioDeviceName::CreateDefault());

    for (size_t i = 0; i < num_input_devices_; i++) {
      device_names->emplace_back(
          /*device_name=*/GetAudioInputDeviceName(i),
          /*unique_id=*/GetAudioInputDeviceId(i));
    }
  }

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_output_devices_ > 0)
      device_names->push_back(media::AudioDeviceName::CreateDefault());

    for (size_t i = 0; i < num_output_devices_; i++) {
      device_names->emplace_back(
          /*device_name=*/std::string("fake_device_name_") +
              base::NumberToString(i),
          /*unique_id=*/std::string(kFakeDeviceIdPrefix) +
              base::NumberToString(i));
    }
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::ChannelLayoutConfig::Stereo(), 48000,
                                  128);
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

class ScreenEnumeratorMock : public media::ScreenEnumerator {
 public:
  explicit ScreenEnumeratorMock(const size_t* screen_count)
      : screen_count_(screen_count) {}
  ~ScreenEnumeratorMock() override = default;

  void EnumerateScreens(
      blink::mojom::MediaStreamType stream_type,
      base::OnceCallback<
          void(const blink::mojom::StreamDevicesSet& stream_devices_set,
               blink::mojom::MediaStreamRequestResult result)> screens_callback)
      const override {
    blink::mojom::StreamDevicesSet stream_devices_set;
    for (size_t screen_idx = 0; screen_idx < *screen_count_; ++screen_idx) {
      stream_devices_set.stream_devices.push_back(
          blink::mojom::StreamDevices::New(
              /*audio_device=*/std::nullopt,
              /*video_device=*/blink::MediaStreamDevice(
                  blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
                  base::StrCat({"id_", base::NumberToString(screen_idx)}),
                  base::StrCat({"name_", base::NumberToString(screen_idx)}))));
    }
    std::move(screens_callback)
        .Run(stream_devices_set, blink::mojom::MediaStreamRequestResult::OK);
  }

 private:
  raw_ptr<const size_t> screen_count_;
};

class MediaStreamProviderListenerMock
    : public content::MediaStreamProviderListener {
 public:
  void Opened(blink::mojom::MediaStreamType stream_type,
              const base::UnguessableToken& capture_session_id) override {
    capture_session_ids_.push_back(capture_session_id);
  }

  void Closed(blink::mojom::MediaStreamType stream_type,
              const base::UnguessableToken& capture_session_id) override {}

  void Aborted(blink::mojom::MediaStreamType stream_type,
               const base::UnguessableToken& capture_session_id) override {}

  const std::vector<base::UnguessableToken>& capture_session_ids() {
    return capture_session_ids_;
  }

 private:
  std::vector<base::UnguessableToken> capture_session_ids_;
};

class TestBrowserClient : public ContentBrowserClient {
 public:
  explicit TestBrowserClient(MediaObserver* media_observer,
                             const size_t* screen_count)
      : media_observer_(media_observer), screen_count_(screen_count) {}
  ~TestBrowserClient() override = default;
  MediaObserver* GetMediaObserver() override { return media_observer_; }
  std::unique_ptr<media::ScreenEnumerator> CreateScreenEnumerator()
      const override {
    return std::make_unique<ScreenEnumeratorMock>(screen_count_);
  }

  MOCK_METHOD(void,
              NotifyMultiCaptureStateChanged,
              (GlobalRenderFrameHostId render_frame_host_id,
               const std::string& label,
               MultiCaptureChanged state),
              (override));

 private:
  raw_ptr<MediaObserver> media_observer_;
  raw_ptr<const size_t> screen_count_;
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

class TestMediaStreamDispatcherHost
    : public blink::mojom::MediaStreamDispatcherHost {
  void GenerateStreams(
      int32_t request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamsCallback callback) override {}
  void CancelRequest(int32_t request_id) override {}
  void StopStreamDevice(
      const std::string& device_id,
      const std::optional<base::UnguessableToken>& session_id) override {}
  void OpenDevice(int32_t request_id,
                  const std::string& device_id,
                  blink::mojom::MediaStreamType type,
                  OpenDeviceCallback callback) override {}
  void CloseDevice(const std::string& label) override {}
  void SetCapturingLinkSecured(
      const std::optional<base::UnguessableToken>& session_id,
      blink::mojom::MediaStreamType type,
      bool is_secure) override {}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void SendWheel(const base::UnguessableToken& device_id,
                 blink::mojom::CapturedWheelActionPtr action,
                 SendWheelCallback callback) override {}
  void SetZoomLevel(const base::UnguessableToken& device_id,
                    int32_t zoom_level,
                    SetZoomLevelCallback callback) override {}
  void RequestCapturedSurfaceControlPermission(
      const base::UnguessableToken& device_id,
      RequestCapturedSurfaceControlPermissionCallback callback) override {}
  void FocusCapturedSurface(const std::string& label, bool focus) override {}
  void ApplySubCaptureTarget(const base::UnguessableToken& device_id,
                             media::mojom::SubCaptureTargetType type,
                             const base::Token& target,
                             uint32_t sub_capture_target_version,
                             ApplySubCaptureTargetCallback callback) override {}
#endif

  void GetOpenDevice(int32_t page_request_id,
                     const base::UnguessableToken& session_id,
                     const base::UnguessableToken& transfer_id,
                     GetOpenDeviceCallback callback) override {}
  void KeepDeviceAliveForTransfer(
      const base::UnguessableToken& session_id,
      const base::UnguessableToken& transfer_id,
      base::OnceCallback<void(bool device_found)> callback) override {}
};

class TestVideoCaptureHost : public media::mojom::VideoCaptureHost {
  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer)
      override {}
  void Stop(const base::UnguessableToken& device_id) override {}
  void Pause(const base::UnguessableToken& device_id) override {}
  void Resume(const base::UnguessableToken& device_id,
              const base::UnguessableToken& session_id,
              const media::VideoCaptureParams& params) override {}
  void RequestRefreshFrame(const base::UnguessableToken& device_id) override {}
  void ReleaseBuffer(const base::UnguessableToken& device_id,
                     int32_t buffer_id,
                     const media::VideoCaptureFeedback& feedback) override {}
  void GetDeviceSupportedFormats(
      const base::UnguessableToken& device_id,
      const base::UnguessableToken& session_id,
      GetDeviceSupportedFormatsCallback callback) override {}
  void GetDeviceFormatsInUse(const base::UnguessableToken& device_id,
                             const base::UnguessableToken& session_id,
                             GetDeviceFormatsInUseCallback callback) override {}
  void OnLog(const base::UnguessableToken& device_id,
             const std::string& reason) override {}
};

blink::StreamControls GetVideoStreamControls(std::string hmac_device_id) {
  blink::StreamControls stream_controls{/*request_audio=*/false,
                                        /*request_video=*/true};
  stream_controls.video.device_ids = {hmac_device_id};
  return stream_controls;
}

blink::StreamControls GetAudioStreamControls(std::string hmac_device_id) {
  blink::StreamControls stream_controls{/*request_audio=*/true,
                                        /*request_video=*/false};
  stream_controls.audio.device_ids = {hmac_device_id};
  return stream_controls;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
enum class CapturedSurfaceControlAPI {
  kSendWheel,
  kSetZoomLevel,
  kRequestPermission,
};

// Make an arbitrary valid CapturedWheelAction.
CapturedWheelActionPtr MakeCapturedWheelActionPtr() {
  return CapturedWheelAction::New(
      /*x=*/0,
      /*y=*/0,
      /*wheel_delta_x=*/0,
      /*wheel_delta_y=*/0);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

blink::mojom::StreamSelectionInfoPtr NewSearchBySessionId(
    base::flat_map<std::string, base::UnguessableToken> session_id_map) {
  return blink::mojom::StreamSelectionInfo::NewSearchBySessionId(
      blink::mojom::SearchBySessionId::New(session_id_map));
}

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
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::move(video_capture_provider));
    media_observer_ = std::make_unique<MockMediaObserver>();
    browser_content_client_ = std::make_unique<TestBrowserClient>(
        media_observer_.get(), &screen_count_);
    SetBrowserClientForTesting(browser_content_client_.get());
    base::RunLoop().RunUntilIdle();

    SetVideoCaptureDevices(/*devices=*/{});
  }

  MediaStreamManagerTest(const MediaStreamManagerTest&) = delete;
  MediaStreamManagerTest& operator=(const MediaStreamManagerTest&) = delete;

  ~MediaStreamManagerTest() override { audio_manager_->Shutdown(); }

  MOCK_METHOD1(Response, void(int index));
  void ResponseCallback(
      int index,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      std::unique_ptr<MediaStreamUIProxy> ui_proxy) {
    Response(index);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
  }

 protected:
  std::string MakeMediaAccessRequest(
      int index,
      const GlobalRenderFrameHostId render_frame_host_id = kRenderFrameHostId) {
    const int requester_id = 1;
    const int page_request_id = 1;
    const url::Origin security_origin;
    MediaStreamManager::MediaAccessRequestCallback callback =
        base::BindOnce(&MediaStreamManagerTest::ResponseCallback,
                       base::Unretained(this), index);
    blink::StreamControls controls(true, true);
    return media_stream_manager_->MakeMediaAccessRequest(
        render_frame_host_id, requester_id, page_request_id, controls,
        security_origin, std::move(callback));
  }

  void RequestMultiScreenCapture(size_t screen_count,
                                 const base::UnguessableToken& session) {
    screen_count_ = screen_count;
    const GlobalRenderFrameHostId render_frame_host_id{0, 0};
    const int requester_id = 0;
    const int page_request_id = 0;
    blink::StreamControls controls(/*request_audio=*/false,
                                   /*request_video=*/true);
    controls.request_all_screens = true;
    controls.video.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
    EXPECT_CALL(*media_observer_,
                OnMediaRequestStateChanged(
                    _, _, _, _,
                    blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
                    MEDIA_REQUEST_STATE_OPENING));
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    EXPECT_CALL(*browser_content_client_,
                NotifyMultiCaptureStateChanged(
                    _, _, ContentBrowserClient::MultiCaptureChanged::kStarted));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    stream_provider_listener_ =
        std::make_unique<MediaStreamProviderListenerMock>();
    media_stream_manager_->video_capture_manager_->RegisterListener(
        stream_provider_listener_.get());
    media_stream_manager_->video_capture_manager_->UnregisterListener(
        media_stream_manager_.get());
    media_stream_manager_->GenerateStreams(
        render_frame_host_id, requester_id, page_request_id, controls,
        MediaDeviceSaltAndOrigin::Empty(), /*user_gesture=*/false,
        /*audio_stream_selection_info_ptr=*/
        NewSearchBySessionId({}),
        /*generate_stream_callback=*/base::DoNothing(),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void SetMediaObserverExpectations(bool app_requested_audio,
                                    bool user_shared_audio) {
    EXPECT_CALL(*media_observer_,
                OnMediaRequestStateChanged(
                    _, _, _, _, MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                    MEDIA_REQUEST_STATE_PENDING_APPROVAL));
    EXPECT_CALL(*media_observer_,
                OnMediaRequestStateChanged(
                    _, _, _, _, MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                    MEDIA_REQUEST_STATE_OPENING));
    EXPECT_CALL(*media_observer_,
                OnMediaRequestStateChanged(
                    _, _, _, _, MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                    MEDIA_REQUEST_STATE_DONE));
    if (app_requested_audio) {
      EXPECT_CALL(*media_observer_,
                  OnMediaRequestStateChanged(
                      _, _, _, _, MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                      MEDIA_REQUEST_STATE_PENDING_APPROVAL));
      if (user_shared_audio) {
        EXPECT_CALL(*media_observer_,
                    OnMediaRequestStateChanged(
                        _, _, _, _, MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                        MEDIA_REQUEST_STATE_OPENING));
        EXPECT_CALL(*media_observer_,
                    OnMediaRequestStateChanged(
                        _, _, _, _, MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                        MEDIA_REQUEST_STATE_DONE));
      }
    }
  }

  void RequestAndStopGetDisplayMedia(bool app_requested_audio,
                                     bool user_shared_audio) {
    DCHECK(app_requested_audio || !user_shared_audio);
    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
        [](bool user_shared_audio) {
          auto fake_ui = std::make_unique<FakeMediaStreamUIProxy>(
              /*tests_use_fake_render_frame_hosts=*/true);
          fake_ui->SetAudioShare(user_shared_audio);
          return std::unique_ptr<FakeMediaStreamUIProxy>(std::move(fake_ui));
        },
        user_shared_audio));

    blink::StreamControls controls(
        app_requested_audio /* app_requested_audio */,
        true /* request_video */);
    controls.video.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
    if (app_requested_audio)
      controls.audio.stream_type =
          blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;

    const int requester_id = 1;
    const int page_request_id = 1;

    blink::MediaStreamDevice video_device;
    blink::MediaStreamDevice audio_device;
    MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
        base::BindOnce(GenerateStreamsCallback, &run_loop_, app_requested_audio,
                       /*requst_video=*/true, &audio_device, &video_device,
                       user_shared_audio);
    base::MockCallback<MediaStreamManager::DeviceStoppedCallback>
        stopped_callback;
    MediaStreamManager::DeviceChangedCallback changed_callback;
    MediaStreamManager::DeviceRequestStateChangeCallback
        request_state_change_callback;
    MediaStreamManager::DeviceCaptureConfigurationChangeCallback
        capture_configuration_change_callback;
    MediaStreamManager::DeviceCaptureHandleChangeCallback
        capture_handle_change_callback;
    MediaStreamManager::ZoomLevelChangeCallback zoom_level_change_callback;

    SetMediaObserverExpectations(app_requested_audio, user_shared_audio);

    media_stream_manager_->GenerateStreams(
        kRenderFrameHostId, requester_id, page_request_id, controls,
        MediaDeviceSaltAndOrigin::Empty(), false /* user_gesture */,
        StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        std::move(generate_stream_callback), stopped_callback.Get(),
        std::move(changed_callback), std::move(request_state_change_callback),
        std::move(capture_configuration_change_callback),
        std::move(capture_handle_change_callback),
        std::move(zoom_level_change_callback));
    run_loop_.Run();

    EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
              video_device.type);
    if (app_requested_audio && user_shared_audio) {
      EXPECT_EQ(blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                audio_device.type);
    } else {
      EXPECT_EQ(blink::mojom::MediaStreamType::NO_SERVICE, audio_device.type);
    }

    EXPECT_CALL(
        *media_observer_,
        OnMediaRequestStateChanged(
            _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
            MEDIA_REQUEST_STATE_CLOSING));
    media_stream_manager_->StopStreamDevice(kRenderFrameHostId, requester_id,
                                            video_device.id,
                                            video_device.session_id());
    blink::MediaStreamDevice device;
    if (app_requested_audio && user_shared_audio) {
      EXPECT_CALL(
          *media_observer_,
          OnMediaRequestStateChanged(
              _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
              MEDIA_REQUEST_STATE_CLOSING));
      EXPECT_CALL(stopped_callback, Run(_, _))
          .WillOnce(testing::SaveArg<1>(&device));
    }
    media_stream_manager_->StopStreamDevice(kRenderFrameHostId, requester_id,
                                            audio_device.id,
                                            audio_device.session_id());
    EXPECT_EQ(device.type,
              app_requested_audio && user_shared_audio
                  ? blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE
                  : blink::mojom::MediaStreamType::NO_SERVICE);
  }

  static void GenerateStreamsCallback(
      base::RunLoop* wait_loop,
      bool request_audio,
      bool request_video,
      blink::MediaStreamDevice* audio_device,
      blink::MediaStreamDevice* video_device,
      bool audio_share,
      blink::mojom::MediaStreamRequestResult result,
      const std::string& label,
      blink::mojom::StreamDevicesSetPtr stream_devices_set,
      bool pan_tilt_zoom_allowed) {
    // TODO(crbug.com/40216442): Generalize to multiple streams.
    DCHECK_EQ(stream_devices_set->stream_devices.size(), 1u);
    if (request_audio && audio_share) {
      ASSERT_TRUE(
          stream_devices_set->stream_devices[0]->audio_device.has_value());
      *audio_device =
          stream_devices_set->stream_devices[0]->audio_device.value();
    } else {
      ASSERT_FALSE(
          stream_devices_set->stream_devices[0]->audio_device.has_value());
    }

    if (request_video) {
      ASSERT_TRUE(
          stream_devices_set->stream_devices[0]->video_device.has_value());
      *video_device =
          stream_devices_set->stream_devices[0]->video_device.value();
    } else {
      ASSERT_FALSE(
          stream_devices_set->stream_devices[0]->video_device.has_value());
    }

    wait_loop->Quit();
  }

  blink::MediaStreamDevice CreateOrSearchAudioDeviceStream(
      StreamSelectionInfoPtr selection_info,
      GlobalRenderFrameHostId render_frame_host_id = kRenderFrameHostId,
      const blink::StreamControls& controls =
          blink::StreamControls(true /* request_audio */,
                                false /* request_video */),
      int requester_id = 1,
      int page_request_id = 1) {
    base::RunLoop run_loop;
    blink::MediaStreamDevice audio_device;

    MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
        base::BindOnce(GenerateStreamsCallback, &run_loop,
                       /*request_audio=*/true,
                       /*request_video=*/false, &audio_device,
                       /*audio_device=*/nullptr,
                       /*audio_share=*/true);
    MediaStreamManager::DeviceStoppedCallback stopped_callback;
    MediaStreamManager::DeviceChangedCallback changed_callback;
    MediaStreamManager::DeviceRequestStateChangeCallback
        request_state_change_callback;
    MediaStreamManager::DeviceCaptureConfigurationChangeCallback
        capture_configuration_change_callback;
    MediaStreamManager::DeviceCaptureHandleChangeCallback
        capture_handle_change_callback;
    MediaStreamManager::ZoomLevelChangeCallback zoom_level_change_callback;

    media_stream_manager_->GenerateStreams(
        render_frame_host_id, requester_id, page_request_id, controls,
        MediaDeviceSaltAndOrigin::Empty(), false /* user_gesture */,
        std::move(selection_info), std::move(generate_stream_callback),
        std::move(stopped_callback), std::move(changed_callback),
        std::move(request_state_change_callback),
        std::move(capture_configuration_change_callback),
        std::move(capture_handle_change_callback),
        std::move(zoom_level_change_callback));
    run_loop.Run();

    return audio_device;
  }

  void SetVideoCaptureDevices(
      const std::vector<media::VideoCaptureDeviceInfo>& devices) {
    ON_CALL(*video_capture_provider_, GetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [devices](
                VideoCaptureProvider::GetDeviceInfosCallback result_callback) {
              std::move(result_callback)
                  .Run(media::mojom::DeviceEnumerationResult::kSuccess,
                       devices);
            }));
  }

  std::unique_ptr<MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  // media_stream_manager_ needs to outlive task_environment_ because it is a
  // CurrentThread::DestructionObserver. audio_manager_ needs to outlive
  // task_environment_ because it uses the underlying message loop.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<MockMediaObserver> media_observer_;
  std::unique_ptr<TestBrowserClient> browser_content_client_;
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<MockVideoCaptureProvider> video_capture_provider_;
  std::unique_ptr<MediaStreamProviderListenerMock> stream_provider_listener_;
  size_t screen_count_ = 0;
  base::RunLoop run_loop_;
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
  EXPECT_CALL(*media_observer_,
              OnMediaRequestStateChanged(
                  _, _, _, _,
                  blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB,
                  MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
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

  const GlobalRenderFrameHostId render_frame_host_id{3, 4};
  int requester_id = 2;
  int page_request_id = 2;
  url::Origin security_origin;
  blink::StreamControls controls(true, true);
  MediaStreamManager::MediaAccessRequestCallback callback = base::BindOnce(
      &MediaStreamManagerTest::ResponseCallback, base::Unretained(this), 1);
  std::string label2 = media_stream_manager_->MakeMediaAccessRequest(
      render_frame_host_id, requester_id, page_request_id, controls,
      security_origin, std::move(callback));

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
  EXPECT_CALL(*media_observer_,
              OnMediaRequestStateChanged(
                  _, _, _, _,
                  blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB,
                  MEDIA_REQUEST_STATE_CLOSING));
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
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
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}));

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
    blink::MediaStreamDevice audio_device =
        CreateOrSearchAudioDeviceStream(NewSearchBySessionId({}));

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

  // Test that `StreamSelectionInfo` is provided as `SearchBySessionId` with
  // the id set to an non-existing ID a new stream is provided and
  // that if the ID is valid, that the stream is reused.
  auto token = base::UnguessableToken::Create();
  blink::MediaStreamDevice reference_device =
      CreateOrSearchAudioDeviceStream(NewSearchBySessionId(
          {{media::AudioDeviceDescription::kDefaultDeviceId, token}}));
  EXPECT_NE(reference_device.session_id(), token);

  for (int i = 0; i < num_call_iterations; ++i) {
    blink::MediaStreamDevice audio_device =
        CreateOrSearchAudioDeviceStream(NewSearchBySessionId({
            {media::AudioDeviceDescription::kDefaultDeviceId,
             reference_device.session_id()},
            {GetAudioInputDeviceId(0), token},
        }));
    EXPECT_EQ(audio_device.id, media::AudioDeviceDescription::kDefaultDeviceId);
    EXPECT_EQ(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              audio_device.type);
    EXPECT_TRUE(audio_device.session_id());
    EXPECT_EQ(audio_device.session_id(), reference_device.session_id());
  }
}

TEST_F(MediaStreamManagerTest, GetDisplayMediaRequestVideoOnly) {
  RequestAndStopGetDisplayMedia(/*app_requested_audio=*/false,
                                /*user_shared_audio=*/false);
}

TEST_F(MediaStreamManagerTest, GetDisplayMediaRequestAudioAndVideo) {
  RequestAndStopGetDisplayMedia(/*app_requested_audio=*/true,
                                /*user_shared_audio=*/true);
}

// The application requested audio, but the user deselected sharing of audio.
TEST_F(MediaStreamManagerTest,
       GetDisplayMediaRequestAudioAndVideoNoAudioShare) {
  RequestAndStopGetDisplayMedia(/*app_requested_audio=*/true,
                                /*user_shared_audio=*/false);
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

  MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
      base::BindOnce([](blink::mojom::MediaStreamRequestResult result,
                        const std::string& label,
                        blink::mojom::StreamDevicesSetPtr stream_devices_set,
                        bool pan_tilt_zoom_allowed) {});
  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
          MEDIA_REQUEST_STATE_PENDING_APPROVAL));

  const GlobalRenderFrameHostId render_frame_host_id{0, 0};
  const int requester_id = 0;
  const int page_request_id = 0;
  media_stream_manager_->GenerateStreams(
      render_frame_host_id, requester_id, page_request_id, controls,
      MediaDeviceSaltAndOrigin::Empty(), false /* user_gesture */,
      StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
      std::move(generate_stream_callback),
      MediaStreamManager::DeviceStoppedCallback(),
      MediaStreamManager::DeviceChangedCallback(),
      MediaStreamManager::DeviceRequestStateChangeCallback(),
      MediaStreamManager::DeviceCaptureConfigurationChangeCallback(),
      MediaStreamManager::DeviceCaptureHandleChangeCallback(),
      base::DoNothing());
  run_loop_.Run();

  EXPECT_CALL(*media_observer_, OnMediaRequestStateChanged(_, _, _, _, _, _))
      .Times(testing::AtLeast(1));
  media_stream_manager_->CancelAllRequests(render_frame_host_id, 0);
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

  const int requester_id = 1;
  const int page_request_id = 1;

  blink::MediaStreamDevice video_device;
  MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
      base::BindOnce(GenerateStreamsCallback, &run_loop_,
                     /*request_audio=*/false,
                     /*request_video=*/true, /*audio_device=*/nullptr,
                     &video_device,
                     /*audio_share=*/true);
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
  MediaStreamManager::DeviceRequestStateChangeCallback
      request_state_change_callback;
  MediaStreamManager::DeviceCaptureConfigurationChangeCallback
      capture_configuration_change_callback;
  MediaStreamManager::DeviceCaptureHandleChangeCallback
      capture_handle_change_callback;
  MediaStreamManager::ZoomLevelChangeCallback zoom_level_change_callback;

  media_stream_manager_->GenerateStreams(
      kRenderFrameHostId, requester_id, page_request_id, controls,
      MediaDeviceSaltAndOrigin::Empty(), false /* user_gesture */,
      StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
      std::move(generate_stream_callback), std::move(stopped_callback),
      std::move(changed_callback), std::move(request_state_change_callback),
      std::move(capture_configuration_change_callback),
      std::move(capture_handle_change_callback),
      std::move(zoom_level_change_callback));
  run_loop_.Run();
  EXPECT_EQ(controls.video.stream_type, video_device.type);
  EXPECT_NE(DesktopMediaID::TYPE_NONE,
            DesktopMediaID::Parse(video_device.id).type);

  // |request_label| is cached in the |device.name| for testing purpose.
  std::string request_label = video_device.name;
  media_stream_manager_->StopMediaStreamFromBrowser(request_label);

  media_stream_manager_->StopStreamDevice(kRenderFrameHostId, requester_id,
                                          video_device.id,
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
  const int requester_id = 1;
  const int page_request_id = 1;

  blink::MediaStreamDevice video_device;
  MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
      base::BindOnce(GenerateStreamsCallback, &run_loop_,
                     /*request_audio=*/false,
                     /*request_video=*/true, /*audio_device=*/nullptr,
                     &video_device,
                     /*audio_share=*/true);
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
  MediaStreamManager::DeviceRequestStateChangeCallback
      request_state_change_callback;
  MediaStreamManager::DeviceCaptureConfigurationChangeCallback
      capture_configuration_change_callback;
  MediaStreamManager::DeviceCaptureHandleChangeCallback
      capture_handle_change_callback;
  MediaStreamManager::ZoomLevelChangeCallback zoom_level_change_callback;

  media_stream_manager_->GenerateStreams(
      kRenderFrameHostId, requester_id, page_request_id, controls,
      MediaDeviceSaltAndOrigin::Empty(), false /* user_gesture */,
      StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
      std::move(generate_stream_callback), std::move(stopped_callback),
      std::move(changed_callback), std::move(request_state_change_callback),
      std::move(capture_configuration_change_callback),
      std::move(capture_handle_change_callback),
      std::move(zoom_level_change_callback));
  run_loop_.Run();
  EXPECT_EQ(controls.video.stream_type, video_device.type);
  EXPECT_NE(DesktopMediaID::TYPE_NONE,
            DesktopMediaID::Parse(video_device.id).type);

  // |request_label| is cached in the |device.name| for testing purpose.
  std::string request_label = video_device.name;
  media_stream_manager_->ChangeMediaStreamSourceFromBrowser(
      request_label, DesktopMediaID(),
      /*captured_surface_control_active=*/false);

  // Wait to check callbacks before stopping the device.
  base::RunLoop().RunUntilIdle();
  media_stream_manager_->StopStreamDevice(kRenderFrameHostId, requester_id,
                                          video_device.id,
                                          video_device.session_id());
}

TEST_F(MediaStreamManagerTest, MultiCaptureOnMediaStreamUIWindowId) {
  std::vector<media::VideoCaptureSessionId> session_ids;
  VideoCaptureManager::SetDesktopCaptureWindowIdCallback callback =
      base::BindLambdaForTesting(
          [&session_ids](const media::VideoCaptureSessionId& session_id,
                         gfx::NativeViewId window_id) {
            session_ids.push_back(session_id);
          });
  media_stream_manager_->video_capture_manager()
      ->set_desktop_capture_window_id_callback_for_testing(callback);

  gfx::NativeViewId native_view_id = 1;
  blink::mojom::StreamDevicesSetPtr stream_devices_set =
      blink::mojom::StreamDevicesSet::New();
  blink::MediaStreamDevice device_0 = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, "screen:0:0",
      "test_device_0");
  base::UnguessableToken session_id_0 = base::UnguessableToken::Create();
  device_0.set_session_id(session_id_0);
  blink::MediaStreamDevice device_1 = blink::MediaStreamDevice(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, "screen:1:0",
      "test_device_1");
  base::UnguessableToken session_id_1 = base::UnguessableToken::Create();
  device_1.set_session_id(session_id_1);
  stream_devices_set->stream_devices.emplace_back(
      blink::mojom::StreamDevices(std::nullopt, device_0).Clone());
  stream_devices_set->stream_devices.emplace_back(
      blink::mojom::StreamDevices(std::nullopt, device_1).Clone());
  media_stream_manager_->OnMediaStreamUIWindowId(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
      std::move(stream_devices_set), native_view_id);
  ASSERT_EQ(2u, session_ids.size());
  ASSERT_EQ(session_id_0, session_ids[0]);
  ASSERT_EQ(session_id_1, session_ids[1]);
}

TEST_F(MediaStreamManagerTest, MultiCaptureAllDevicesOpened) {
  base::UnguessableToken session = base::UnguessableToken::Create();
  RequestMultiScreenCapture(/*screen_count=*/3u, session);
  const std::vector<base::UnguessableToken>& session_ids =
      stream_provider_listener_->capture_session_ids();
  EXPECT_EQ(3u, session_ids.size());

  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[0]);
  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[1]);

  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
          MEDIA_REQUEST_STATE_DONE));
  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[2]);
}

TEST_F(MediaStreamManagerTest, MultiCaptureNotAllDevicesOpened) {
  base::UnguessableToken session = base::UnguessableToken::Create();
  RequestMultiScreenCapture(/*screen_count=*/3u, session);

  const std::vector<base::UnguessableToken>& session_ids =
      stream_provider_listener_->capture_session_ids();
  EXPECT_EQ(3u, session_ids.size());

  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[0]);
  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[1]);

  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
          MEDIA_REQUEST_STATE_DONE))
      .Times(0);
}

TEST_F(MediaStreamManagerTest, MultiCaptureIntermediateErrorOnOpening) {
  base::UnguessableToken session = base::UnguessableToken::Create();
  RequestMultiScreenCapture(/*screen_count=*/3u, session);

  const std::vector<base::UnguessableToken>& session_ids =
      stream_provider_listener_->capture_session_ids();
  EXPECT_EQ(3u, session_ids.size());

  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
          MEDIA_REQUEST_STATE_ERROR));

  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[0]);
  media_stream_manager_->SetStateForTesting(
      /*request_index=*/0u,
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      MEDIA_REQUEST_STATE_ERROR);
  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[1]);
  media_stream_manager_->Opened(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET, session_ids[2]);

  EXPECT_CALL(
      *media_observer_,
      OnMediaRequestStateChanged(
          _, _, _, _, blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
          MEDIA_REQUEST_STATE_DONE))
      .Times(0);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_CALL(*browser_content_client_,
              NotifyMultiCaptureStateChanged(
                  _, _, ContentBrowserClient::MultiCaptureChanged::kStopped));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

TEST_F(MediaStreamManagerTest, RegisterUnregisterHosts) {
  mojo::Remote<blink::mojom::MediaStreamDispatcherHost> dispatcher_client1;
  media_stream_manager_->RegisterDispatcherHost(
      std::make_unique<TestMediaStreamDispatcherHost>(),
      dispatcher_client1.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_stream_manager_->num_dispatcher_hosts(), 1u);

  mojo::Remote<media::mojom::VideoCaptureHost> video_capture_client1;
  media_stream_manager_->RegisterVideoCaptureHost(
      std::make_unique<TestVideoCaptureHost>(),
      video_capture_client1.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_stream_manager_->num_video_capture_hosts(), 1u);

  mojo::Remote<blink::mojom::MediaStreamDispatcherHost> dispatcher_client2;
  media_stream_manager_->RegisterDispatcherHost(
      std::make_unique<TestMediaStreamDispatcherHost>(),
      dispatcher_client2.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_stream_manager_->num_dispatcher_hosts(), 2u);

  mojo::Remote<media::mojom::VideoCaptureHost> video_capture_client2;
  media_stream_manager_->RegisterVideoCaptureHost(
      std::make_unique<TestVideoCaptureHost>(),
      video_capture_client2.BindNewPipeAndPassReceiver());
  EXPECT_EQ(media_stream_manager_->num_video_capture_hosts(), 2u);

  // Closing a pipe unregisters the corresponding host.
  dispatcher_client1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(media_stream_manager_->num_dispatcher_hosts(), 1u);

  video_capture_client1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(media_stream_manager_->num_video_capture_hosts(), 1u);

  // Shutting down MediaStreamManager disconnects the remaining pipes.
  EXPECT_TRUE(dispatcher_client2.is_connected());
  EXPECT_TRUE(video_capture_client2.is_connected());
  // `video_capture_provider_` is pointing at the MockVideoCaptureProvider
  // owned by MediaDevicesManager and will be destroyed as part of calling
  // WillDestroyCurrentMessageLoop. Clearing it here avoids a dangling
  // pointer.
  video_capture_provider_ = nullptr;
  media_stream_manager_->WillDestroyCurrentMessageLoop();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(dispatcher_client2.is_connected());
  EXPECT_FALSE(video_capture_client2.is_connected());
}

class MediaStreamManagerTestWithWebContents : public MediaStreamManagerTest {
 protected:
  void SetUp() override {
    web_contents_ = CreateWebContents();
    render_frame_host_ = web_contents_->GetPrimaryMainFrame();
    render_frame_host_id_ = render_frame_host_->GetGlobalId();

    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/true);
    }));
  }

  std::unique_ptr<TestWebContents> CreateWebContents() {
    return TestWebContents::Create(&browser_context_,
                                   SiteInstanceImpl::Create(&browser_context_));
  }

  void GenerateStreams(
      GlobalRenderFrameHostId render_frame_host_id,
      const blink::StreamControls& controls,
      MediaDeviceSaltAndOrigin salt_and_origin,
      MediaStreamManager::GenerateStreamsCallback generate_stream_cb) {
    media_stream_manager_->GenerateStreams(
        render_frame_host_id, /*requester_id=*/0, /*page_request_id=*/0,
        controls, salt_and_origin,
        /*user_gesture=*/false,
        /*audio_stream_selection_info_ptr=*/
        NewSearchBySessionId({}), std::move(generate_stream_cb),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_cb*/ base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());
  }

  std::vector<std::string> GetRawDeviceIdsOpenedForFrame(
      RenderFrameHost* render_frame_host,
      blink::mojom::MediaStreamType type) {
    base::test::TestFuture<std::vector<std::string>> future;
    media_stream_manager_->GetRawDeviceIdsOpenedForFrame(
        render_frame_host, type, future.GetCallback());
    return future.Take();
  }

  RenderViewHostTestEnabler rvh_test_enabler_;
  TestBrowserContext browser_context_;
  std::unique_ptr<TestWebContents> web_contents_;
  raw_ptr<RenderFrameHost> render_frame_host_;
  GlobalRenderFrameHostId render_frame_host_id_;
};

TEST_F(MediaStreamManagerTestWithWebContents,
       GetRawDeviceIdsOpenedForFrame_AudioCapture) {
  const std::string kDevice1RawId = GetAudioInputDeviceId(0);
  const std::string kDevice2RawId = GetAudioInputDeviceId(1);

  auto salt_and_origin = MediaDeviceSaltAndOrigin::Empty();
  const std::string kDevice1HMACId =
      GetHMACForRawMediaDeviceID(salt_and_origin, kDevice1RawId);
  const std::string kDevice2HMACId =
      GetHMACForRawMediaDeviceID(salt_and_origin, kDevice2RawId);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                         const std::string&, blink::mojom::StreamDevicesSetPtr,
                         bool>
      streams_generated_future;

  GenerateStreams(render_frame_host_id_, GetAudioStreamControls(kDevice2HMACId),
                  salt_and_origin, streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  // Open device 1 on another render frame id. Device 1 shouldn't be
  // included in the response from `GetRawDeviceIdsOpenedForFrame()`.
  GenerateStreams({42, 38}, GetAudioStreamControls(kDevice1HMACId),
                  salt_and_origin, streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  EXPECT_THAT(GetRawDeviceIdsOpenedForFrame(
                  render_frame_host_,
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE),
              ElementsAre(kDevice2RawId));

  // Querying with a different render frame returns nothing.
  auto other_web_contents = CreateWebContents();
  EXPECT_TRUE(GetRawDeviceIdsOpenedForFrame(
                  other_web_contents->GetPrimaryMainFrame(),
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)
                  .empty());
  // Querying with a different type returns nothing.
  EXPECT_TRUE(GetRawDeviceIdsOpenedForFrame(
                  render_frame_host_,
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
                  .empty());
}

TEST_F(MediaStreamManagerTestWithWebContents,
       GetRawDeviceIdsOpenedForFrame_VideoCapture) {
  const char kDevice1RawId[] = "device_1";
  const char kDevice2RawId[] = "device_2";

  std::vector<media::VideoCaptureDeviceInfo> devices{
      media::VideoCaptureDeviceInfo{media::VideoCaptureDeviceDescriptor{
          /*display_name=*/"Test 1", /*device_id=*/kDevice1RawId}},
      media::VideoCaptureDeviceInfo{media::VideoCaptureDeviceDescriptor{
          /*display_name=*/"Test 2", /*device_id=*/kDevice2RawId}},
  };
  SetVideoCaptureDevices(devices);

  auto salt_and_origin = MediaDeviceSaltAndOrigin::Empty();
  const std::string kDevice1HMACId =
      GetHMACForRawMediaDeviceID(salt_and_origin, kDevice1RawId);
  const std::string kDevice2HMACId =
      GetHMACForRawMediaDeviceID(salt_and_origin, kDevice2RawId);

  base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                         const std::string&, blink::mojom::StreamDevicesSetPtr,
                         bool>
      streams_generated_future;

  GenerateStreams(render_frame_host_id_, GetVideoStreamControls(kDevice2HMACId),
                  salt_and_origin, streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  // Open device 1 on another render frame id. Device 1 shouldn't be
  // included in the response from `GetRawDeviceIdsOpenedForFrame()`.
  GenerateStreams({42, 38}, GetVideoStreamControls(kDevice1HMACId),
                  salt_and_origin, streams_generated_future.GetCallback());

  ASSERT_EQ(std::get<0>(streams_generated_future.Take()),
            blink::mojom::MediaStreamRequestResult::OK)
      << "GenerateStreams() call failed";

  EXPECT_THAT(GetRawDeviceIdsOpenedForFrame(
                  render_frame_host_,
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE),
              ElementsAre(kDevice2RawId));

  // Querying with a different render frame returns nothing.
  auto other_web_contents = CreateWebContents();
  EXPECT_TRUE(GetRawDeviceIdsOpenedForFrame(
                  other_web_contents->GetPrimaryMainFrame(),
                  blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
                  .empty());
  // Querying with a different type returns nothing.
  EXPECT_TRUE(GetRawDeviceIdsOpenedForFrame(
                  render_frame_host_,
                  blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)
                  .empty());
}

class MediaStreamManagerTestForTransfers : public MediaStreamManagerTest {
 public:
  void CustomSetUp(const char* surface_type = "browser") {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaStreamTrackTransfer);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=%s", surface_type));
    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/true);
    }));
  }

  void RequestDeviceCaptureTypeAudioDevice() {
    // Generate stream on first renderer.
    original_device_ =
        CreateOrSearchAudioDeviceStream(NewSearchBySessionId({}));
    existing_device_session_id_ = original_device_.session_id();

    EXPECT_EQ(original_device_.type,
              blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
    EXPECT_NE(transferred_device_.id, original_device_.id);
  }

  void RequestDisplayCaptureTypeDevice(bool request_audio = true,
                                       bool request_video = true,
                                       bool transfer_audio = true) {
    base::RunLoop run_loop;

    blink::StreamControls controls(request_audio, request_video);
    if (request_audio)
      controls.audio.stream_type =
          blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
    if (request_video)
      controls.video.stream_type =
          blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;

    blink::MediaStreamDevice video_device;
    blink::MediaStreamDevice audio_device;
    MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
        base::BindOnce(GenerateStreamsCallback, &run_loop, request_audio,
                       request_video, &audio_device, &video_device,
                       /*audio_share=*/true);

    media_stream_manager_->GenerateStreams(
        render_frame_host_id_, requester_id_,
        /*page_request_id=*/1, controls, MediaDeviceSaltAndOrigin::Empty(),
        /*user_gesture=*/false,
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        std::move(generate_stream_callback),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());
    run_loop.Run();

    original_device_ = transfer_audio ? audio_device : video_device;
    existing_device_session_id_ = original_device_.session_id();
    EXPECT_NE(transferred_device_.id, original_device_.id);
  }

  void GetOpenDevice() {
    MediaStreamManager::GetOpenDeviceCallback get_open_device_cb =
        base::BindLambdaForTesting(
            [&](blink::mojom::MediaStreamRequestResult result,
                blink::mojom::GetOpenDeviceResponsePtr response) {
              result_ = result;
              if (response) {
                transferred_device_ = response->device;
              }
            })
            .Then(run_loop_.QuitClosure());

    // GetOpenDevice is called on second renderer.
    media_stream_manager_->GetOpenDevice(
        existing_device_session_id_, transfer_id_,
        GlobalRenderFrameHostId{3, 4}, /*requester_id=*/2,
        /*page_request_id=*/2, MediaDeviceSaltAndOrigin::Empty(),
        std::move(get_open_device_cb),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());
    run_loop_.Run();
  }

  bool KeepDeviceAlive() {
    // Call to KeepDeviceAlive from the first renderer.
    return media_stream_manager_->KeepDeviceAliveForTransfer(
        render_frame_host_id_, requester_id_, existing_device_session_id_,
        transfer_id_);
  }

  void StopDevice(bool should_stop = true) {
    if (!should_stop) {
      EXPECT_CALL(
          *media_observer_,
          OnMediaRequestStateChanged(
              _, _, _, _, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
              MEDIA_REQUEST_STATE_CLOSING))
          .Times(0);
    }

    // Stop device from the first renderer.
    media_stream_manager_->StopStreamDevice(render_frame_host_id_,
                                            requester_id_, original_device_.id,
                                            existing_device_session_id_);
  }

  const GlobalRenderFrameHostId render_frame_host_id_ = kRenderFrameHostId;
  const int requester_id_ = 1;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::UnguessableToken existing_device_session_id_ =
      base::UnguessableToken::Create();
  const base::UnguessableToken transfer_id_ = base::UnguessableToken::Create();
  blink::MediaStreamDevice original_device_;
  blink::MediaStreamDevice transferred_device_;
  blink::mojom::MediaStreamRequestResult result_ =
      blink::mojom::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS;
};

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceForScreenCaptureTypeStreamFails) {
  CustomSetUp(/*surface_type=*/"monitor");
  RequestDisplayCaptureTypeDevice();
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::INVALID_STATE);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceForWindowCaptureTypeStreamFails) {
  CustomSetUp(/*surface_type=*/"window");
  RequestDisplayCaptureTypeDevice();
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::INVALID_STATE);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceForBrowserCaptureTypeStreamReturnsDevice) {
  CustomSetUp(/*surface_type=*/"browser");
  RequestDisplayCaptureTypeDevice();
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceForDeviceCaptureTypeStreamFails) {
  CustomSetUp();
  RequestDeviceCaptureTypeAudioDevice();
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::INVALID_STATE);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetDisplayMediaAudioAndVideoAndGetOpenDeviceVideoReturnsDevice) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice(/*request_audio=*/true,
                                  /*request_video=*/true,
                                  /*transfer_audio=*/false);
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetDisplayMediaVideoAndGetOpenDeviceVideoReturnsDevice) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice(/*request_audio=*/false,
                                  /*request_video=*/true,
                                  /*transfer_audio=*/false);
  GetOpenDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceWhenKeepAliveAfterStopDoesNotReturnDevice) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice();
  StopDevice();
  EXPECT_FALSE(KeepDeviceAlive());
  GetOpenDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::INVALID_STATE);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceWhenKeepAliveBeforeStopReturnsDevice) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice();
  EXPECT_TRUE(KeepDeviceAlive());
  StopDevice();
  GetOpenDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceWithoutKeepAliveReturnsDeviceButDoesNotStop) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice();
  GetOpenDevice();
  StopDevice(/*should_stop=*/false);

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceWithKeepAliveAfterStopReturnsDevice) {
  CustomSetUp();
  RequestDisplayCaptureTypeDevice();
  GetOpenDevice();
  StopDevice();
  EXPECT_TRUE(KeepDeviceAlive());

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::OK);
  EXPECT_EQ(transferred_device_.id, original_device_.id);
  EXPECT_NE(transferred_device_.session_id(), existing_device_session_id_);
}

TEST_F(MediaStreamManagerTestForTransfers,
       GetOpenDeviceForNonExistentDeviceReturnsInvalidState) {
  CustomSetUp();
  GetOpenDevice();

  EXPECT_EQ(result_, blink::mojom::MediaStreamRequestResult::INVALID_STATE);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class MediaStreamManagerCapturedSurfaceControlTest
    : public MediaStreamManagerTest {
 public:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=browser"));
  }

  void SimulateGetDisplayMedia(
      GlobalRenderFrameHostId rfhid = GlobalRenderFrameHostId{1, 2},
      std::optional<WebContentsMediaCaptureId> captured_tab_id = std::nullopt) {
    media_stream_manager_->UseFakeUIFactoryForTests(
        base::BindRepeating([]() {
          auto fake_ui = std::make_unique<FakeMediaStreamUIProxy>(
              /*tests_use_fake_render_frame_hosts=*/true);
          fake_ui->SetAudioShare(true);
          return std::unique_ptr<FakeMediaStreamUIProxy>(std::move(fake_ui));
        }),
        /*use_for_gum_desktop_capture=*/false, captured_tab_id);

    SetMediaObserverExpectations(/*app_requested_audio=*/true,
                                 /*user_shared_audio=*/true);

    base::RunLoop run_loop;

    blink::StreamControls controls(/*request_audio=*/true,
                                   /*request_video=*/true);
    controls.audio.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
    controls.video.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;

    MediaStreamManager::GenerateStreamsCallback generate_stream_callback =
        base::BindOnce(GenerateStreamsCallback, &run_loop,
                       /*request_audio=*/true,
                       /*request_video=*/true, &audio_device_, &video_device_,
                       /*audio_share=*/true);

    media_stream_manager_->GenerateStreams(
        rfhid, /*requester_id=*/1,
        /*page_request_id=*/1, controls, MediaDeviceSaltAndOrigin::Empty(),
        /*user_gesture=*/true, StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        std::move(generate_stream_callback),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/
        base::BindRepeating(
            &MediaStreamManagerCapturedSurfaceControlTest::OnZoomLevelChange,
            base::Unretained(this)));
    run_loop.Run();
  }

  void OnZoomLevelChange(const std::string& label,
                         const blink::MediaStreamDevice& device,
                         int zoom_level) {
    on_zoom_level_change_device_id_ = device.id;
    on_zoom_level_change_zoom_level_ = zoom_level;
  }

  void SetCapturedSurfaceControllerFactory(
      GlobalRenderFrameHostId gdm_rfhid,
      WebContentsMediaCaptureId captured_wc_id) {
    media_stream_manager_->SetCapturedSurfaceControllerFactoryForTesting(
        base::BindRepeating(
            [](base::RepeatingCallback<void(int)>* received_zoom_level_callback,
               GlobalRenderFrameHostId gdm_rfhid,
               WebContentsMediaCaptureId captured_wc_id,
               base::RepeatingCallback<void(int)> zoom_level_callback) {
              *received_zoom_level_callback = zoom_level_callback;
              auto captured_surface_controller =
                  std::make_unique<MockCapturedSurfaceController>(
                      gdm_rfhid, captured_wc_id);
              captured_surface_controller->SetSendWheelResponse(
                  CapturedSurfaceControlResult::kSuccess);
              captured_surface_controller->SetGetZoomLevelResponse(
                  100, CapturedSurfaceControlResult::kSuccess);
              captured_surface_controller->SetSetZoomLevelResponse(
                  CapturedSurfaceControlResult::kSuccess);
              captured_surface_controller->SetRequestPermissionResponse(
                  CapturedSurfaceControlResult::kSuccess);
              return base::WrapUnique<CapturedSurfaceController>(
                  captured_surface_controller.release());
            },
            &received_zoom_level_callback_));
  }

  base::OnceCallback<void(CapturedSurfaceControlResult)> MakeCallback() {
    return base::BindOnce(
        [](std::optional<CapturedSurfaceControlResult>* result_opt,
           CapturedSurfaceControlResult result) {
          CHECK(result_opt);
          EXPECT_FALSE(result_opt->has_value());
          *result_opt = result;
        },
        &result_);
  }

  void SendWheel(
      GlobalRenderFrameHostId gdm_rfhid,
      std::optional<base::UnguessableToken> session_id = std::nullopt) {
    media_stream_manager_->SendWheel(
        gdm_rfhid, session_id.value_or(video_device_.session_id()),
        MakeCapturedWheelActionPtr(), MakeCallback());
  }

  base::OnceCallback<void(std::optional<int>, CapturedSurfaceControlResult)>
  MakeGetZoomLevelCallback() {
    return base::BindOnce(
        [](std::optional<CapturedSurfaceControlResult>* result_opt,
           std::optional<int>, CapturedSurfaceControlResult result) {
          CHECK(result_opt);
          EXPECT_FALSE(result_opt->has_value());
          *result_opt = result;
        },
        &result_);
  }

  void SetZoomLevel(
      GlobalRenderFrameHostId gdm_rfhid,
      std::optional<base::UnguessableToken> session_id = std::nullopt) {
    media_stream_manager_->SetZoomLevel(
        gdm_rfhid, session_id.value_or(video_device_.session_id()), 100,
        MakeCallback());
  }

  void RequestPermission(
      GlobalRenderFrameHostId gdm_rfhid,
      std::optional<base::UnguessableToken> session_id = std::nullopt) {
    media_stream_manager_->RequestCapturedSurfaceControlPermission(
        gdm_rfhid, session_id.value_or(video_device_.session_id()),
        MakeCallback());
  }

  blink::MediaStreamDevice video_device_;
  blink::MediaStreamDevice audio_device_;

  std::optional<CapturedSurfaceControlResult> result_;
  base::RepeatingCallback<void(int)> received_zoom_level_callback_;
  std::string on_zoom_level_change_device_id_;
  std::optional<int> on_zoom_level_change_zoom_level_;
};

TEST_F(MediaStreamManagerCapturedSurfaceControlTest, ZoomLevelChangeEvent) {
  SCOPED_TRACE("ZoomLevelChangeEvent");
  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  received_zoom_level_callback_.Run(90);
  EXPECT_EQ(on_zoom_level_change_device_id_, video_device_.id);
  EXPECT_EQ(on_zoom_level_change_zoom_level_, 90);
}

class MediaStreamManagerCapturedSurfaceControlActionTest
    : public MediaStreamManagerCapturedSurfaceControlTest,
      public testing::WithParamInterface<CapturedSurfaceControlAPI> {
 public:
  MediaStreamManagerCapturedSurfaceControlActionTest()
      : tested_api_(GetParam()) {}

  void RunTestedAction(
      GlobalRenderFrameHostId gdm_rfhid,
      std::optional<base::UnguessableToken> session_id = std::nullopt) {
    switch (tested_api_) {
      case CapturedSurfaceControlAPI::kSendWheel: {
        SendWheel(gdm_rfhid, session_id);
        return;
      }
      case CapturedSurfaceControlAPI::kSetZoomLevel: {
        SetZoomLevel(gdm_rfhid, session_id);
        return;
      }
      case CapturedSurfaceControlAPI::kRequestPermission: {
        RequestPermission(gdm_rfhid, session_id);
        return;
      }
    }
    NOTREACHED();
  }

  const CapturedSurfaceControlAPI tested_api_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    MediaStreamManagerCapturedSurfaceControlActionTest,
    testing::Values(CapturedSurfaceControlAPI::kSendWheel,
                    CapturedSurfaceControlAPI::kSetZoomLevel,
                    CapturedSurfaceControlAPI::kRequestPermission));

TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest, SuccessfulIfValid) {
  SCOPED_TRACE("SuccessfulIfValid");
  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(gdm_rfhid);
  EXPECT_EQ(result_, CapturedSurfaceControlResult::kSuccess);
}

TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest,
       FailsIfInvalidSessionId) {
  SCOPED_TRACE("FailsIfInvalidSessionId");
  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(gdm_rfhid, base::UnguessableToken::Create());
  EXPECT_EQ(result_,
            CapturedSurfaceControlResult::kCapturedSurfaceNotFoundError);
}

TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest,
       FailsIfNotCapturerRfhId) {
  SCOPED_TRACE("FailsIfNotCapturerRfhId");
  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const GlobalRenderFrameHostId other_rfhid{11, 22};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(other_rfhid);
  EXPECT_EQ(result_, CapturedSurfaceControlResult::kUnknownError);
}

// This test is currently disabled because the code that ensures that Captured
// Surface Control APIs are disallowed for self-capture has not yet been
// authored.
// TODO(crbug.com/41484336): Enable this test.
TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest,
       DISABLED_FailsIfSelfCapture) {
  SCOPED_TRACE("FailsIfSelfCapture");
  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{1, 2};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(gdm_rfhid);
  // TODO(crbug.com/41485502): Use a dedicated error.
  EXPECT_EQ(result_, CapturedSurfaceControlResult::kUnknownError);
}

TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest,
       FailsIfCapturingWindow) {
  SCOPED_TRACE("FailsIfCapturingWindow");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream,
      base::StringPrintf("display-media-type=window"));

  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(gdm_rfhid);
  // TODO(crbug.com/41485502): Use a dedicated error.
  EXPECT_EQ(result_, CapturedSurfaceControlResult::kUnknownError);
}

TEST_P(MediaStreamManagerCapturedSurfaceControlActionTest,
       FailsIfCapturingScreen) {
  SCOPED_TRACE("FailsIfCapturingScreen");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseFakeDeviceForMediaStream,
      base::StringPrintf("display-media-type=monitor"));

  const GlobalRenderFrameHostId gdm_rfhid{1, 2};
  const WebContentsMediaCaptureId captured_wc_id{3, 4};

  SetCapturedSurfaceControllerFactory(gdm_rfhid, captured_wc_id);
  SimulateGetDisplayMedia(gdm_rfhid, captured_wc_id);

  RunTestedAction(gdm_rfhid);
  // TODO(crbug.com/41485502): Use a dedicated error.
  EXPECT_EQ(result_, CapturedSurfaceControlResult::kUnknownError);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace content
