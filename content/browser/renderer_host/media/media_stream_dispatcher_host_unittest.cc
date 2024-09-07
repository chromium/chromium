// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_captured_surface_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#endif

using ::blink::mojom::CapturedSurfaceControlResult;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace content {

namespace {

const GlobalRenderFrameHostId kRenderFrameHostId{5, 6};
constexpr int kRequesterId = 7;
constexpr int kPageRequestId = 8;
constexpr const char* kRegularVideoDeviceId1 = "stub_device_1";
constexpr const char* kRegularVideoDeviceId2 = "stub_device_2";
constexpr const char* kDepthVideoDeviceId = "stub_device_1 (depth)";
constexpr media::VideoCaptureApi kStubCaptureApi =
    media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;

bool DoStreamDevicesHaveSameTypes(const blink::mojom::StreamDevices& lhs,
                                  const blink::mojom::StreamDevices& rhs) {
  return lhs.audio_device.has_value() == rhs.audio_device.has_value() &&
         lhs.video_device.has_value() == rhs.video_device.has_value();
}

MATCHER_P(SameTypesAs,
          expected_stream_devices_set_ref,
          "Compares if two StreamDevices objects contain the same number and "
          "type of MediaStreamDevice objects.") {
  const blink::mojom::StreamDevicesSet& stream_devices_set = arg;
  const blink::mojom::StreamDevicesSet& expected_stream_devices_set =
      expected_stream_devices_set_ref.get();

  if (stream_devices_set.stream_devices.size() !=
      expected_stream_devices_set.stream_devices.size()) {
    return false;
  }

  for (size_t i = 0; i < stream_devices_set.stream_devices.size(); ++i) {
    if (!DoStreamDevicesHaveSameTypes(
            *stream_devices_set.stream_devices[i],
            *expected_stream_devices_set.stream_devices[i])) {
      return false;
    }
  }

  return true;
}

void AudioInputDevicesEnumerated(base::OnceClosure quit_closure,
                                 media::AudioDeviceDescriptions* out,
                                 const MediaDeviceEnumeration& enumeration) {
  for (const auto& info : enumeration[static_cast<size_t>(
           blink::mojom::MediaDeviceType::kMediaAudioInput)]) {
    out->emplace_back(info.label, info.device_id, info.group_id);
  }
  std::move(quit_closure).Run();
}

}  // anonymous namespace

class MockMediaStreamDispatcherHost
    : public MediaStreamDispatcherHost,
      public blink::mojom::MediaStreamDeviceObserver {
 public:
  MockMediaStreamDispatcherHost(GlobalRenderFrameHostId render_frame_host_id,
                                MediaStreamManager* manager)
      : MediaStreamDispatcherHost(render_frame_host_id, manager),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}
  ~MockMediaStreamDispatcherHost() override {}

  // A list of mock methods.
  MOCK_METHOD2(OnStreamGenerationSuccess,
               void(int request_id,
                    const blink::mojom::StreamDevicesSet& stream_devices_set));
  MOCK_METHOD2(OnStreamGenerationFailure,
               void(int request_id,
                    blink::mojom::MediaStreamRequestResult result));
  MOCK_METHOD0(OnDeviceStopSuccess, void());
  MOCK_METHOD0(OnDeviceOpenSuccess, void());

  // Accessor to private functions.
  void CancelAllRequests() { MediaStreamDispatcherHost::CancelAllRequests(); }

  void OnGenerateStreams(int page_request_id,
                         const blink::StreamControls& controls) {
    MediaStreamDispatcherHost::GenerateStreams(
        page_request_id, controls, false,
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        base::DoNothing());
  }

  void OnGenerateStreams(int page_request_id,
                         const blink::StreamControls& controls,
                         base::OnceClosure quit_closure) {
    quit_closures_.push(std::move(quit_closure));
    MediaStreamDispatcherHost::GenerateStreams(
        page_request_id, controls, false,
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        base::BindOnce(&MockMediaStreamDispatcherHost::OnStreamsGenerated,
                       base::Unretained(this), page_request_id));
  }

  void OnStopStreamDevice(const std::string& device_id,
                          const base::UnguessableToken& session_id) {
    MediaStreamDispatcherHost::StopStreamDevice(device_id, session_id);
  }

  void OnOpenDevice(int page_request_id,
                    const std::string& device_id,
                    blink::mojom::MediaStreamType type,
                    base::OnceClosure quit_closure) {
    quit_closures_.push(std::move(quit_closure));
    MediaStreamDispatcherHost::OpenDevice(
        page_request_id, device_id, type,
        base::BindOnce(&MockMediaStreamDispatcherHost::OnDeviceOpened,
                       base::Unretained(this)));
  }

  // mojom::MediaStreamDeviceObserver implementation.
  void OnDeviceStopped(const std::string& label,
                       const blink::MediaStreamDevice& device) override {
    OnDeviceStoppedInternal(label, device);
  }
  void OnDeviceChanged(const std::string& label,
                       const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device) override {}
  void OnDeviceCaptureConfigurationChange(
      const std::string& label,
      const blink::MediaStreamDevice& device) override {}
  void OnDeviceCaptureHandleChange(
      const std::string& label,
      const blink::MediaStreamDevice& device) override {}
  void OnZoomLevelChange(const std::string& label,
                         const blink::MediaStreamDevice& device,
                         int zoom_level) override {}
  void OnDeviceRequestStateChange(
      const std::string& label,
      const blink::MediaStreamDevice& device,
      const blink::mojom::MediaStreamStateChange new_state) override {}

  mojo::PendingRemote<blink::mojom::MediaStreamDeviceObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  std::string label_;
  blink::mojom::StreamDevicesSetPtr stream_devices_set_;
  blink::MediaStreamDevice opened_device_;

 private:
  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamsGenerated(int request_id,
                          blink::mojom::MediaStreamRequestResult result,
                          const std::string& label,
                          blink::mojom::StreamDevicesSetPtr stream_devices_set,
                          bool pan_tilt_zoom_allowed) {
    if (result != blink::mojom::MediaStreamRequestResult::OK) {
      DCHECK(!stream_devices_set);
      OnStreamGenerationFailed(request_id, result);
      return;
    }

    OnStreamGenerationSuccess(request_id, *stream_devices_set);

    // Notify that the event have occurred.
    task_runner_->PostTask(FROM_HERE, std::move(quit_closures_.front()));
    quit_closures_.pop();

    label_ = label;
    stream_devices_set_ = std::move(stream_devices_set);
  }

  void OnStreamGenerationFailed(int request_id,
                                blink::mojom::MediaStreamRequestResult result) {
    OnStreamGenerationFailure(request_id, result);
    if (!quit_closures_.empty()) {
      task_runner_->PostTask(FROM_HERE, std::move(quit_closures_.front()));
      quit_closures_.pop();
    }

    label_.clear();
  }

  void OnDeviceStoppedInternal(const std::string& label,
                               const blink::MediaStreamDevice& device) {
    if (blink::IsVideoInputMediaType(device.type)) {
      EXPECT_TRUE(device.IsSameDevice(
          stream_devices_set_->stream_devices[0]->video_device.value()));
    }
    if (blink::IsAudioInputMediaType(device.type)) {
      EXPECT_TRUE(device.IsSameDevice(
          stream_devices_set_->stream_devices[0]->audio_device.value()));
    }

    OnDeviceStopSuccess();
  }

  void OnDeviceOpened(bool success,
                      const std::string& label,
                      const blink::MediaStreamDevice& device) {
    task_runner_->PostTask(FROM_HERE, std::move(quit_closures_.front()));
    quit_closures_.pop();
    if (success) {
      label_ = label;
      opened_device_ = device;
      OnDeviceOpenSuccess();
    }
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::queue<base::OnceClosure> quit_closures_;
  mojo::Receiver<blink::mojom::MediaStreamDeviceObserver> receiver_{this};
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MockMediaStreamUIProxy()
      : FakeMediaStreamUIProxy(/*tests_use_fake_render_frame_hosts=*/true) {}
  void OnStarted(
      base::OnceClosure stop,
      content::MediaStreamUI::SourceCallback source,
      MediaStreamUIProxy::WindowIdCallback window_id_callback,
      const std::string& label,
      std::vector<DesktopMediaID> screen_share_ids,
      MediaStreamUI::StateChangeCallback state_change_callback) override {
    // gmock cannot handle move-only types, so no std::move().
    MockOnStarted(stop);
  }

  MOCK_METHOD1(MockOnStarted, void(base::OnceClosure& stop));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        salt_and_origin_(CreateRandomMediaDeviceIDSalt(),
                         url::Origin::Create(GURL("https://test.com"))) {
    scoped_feature_list_
        .InitFromCommandLine(/*enable_features=*/
                             "UserMediaCaptureOnFocus,GetAllScreensMedia",
                             /*disable_features=*/"");
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    ResetMediaDeviceIDSalt();
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUseFakeDeviceForMediaStream,
        base::StringPrintf("display-media-type=browser"));
    auto mock_video_capture_provider =
        std::make_unique<MockVideoCaptureProvider>();
    mock_video_capture_provider_ = mock_video_capture_provider.get();
    // Create our own MediaStreamManager.
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::move(mock_video_capture_provider));
    salt_and_origin_.set_has_focus(true);
    salt_and_origin_.set_is_background(false);
    host_ = std::make_unique<MockMediaStreamDispatcherHost>(
        kRenderFrameHostId, media_stream_manager_.get());
    host_->set_get_salt_and_origin_cb_for_testing(
        base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                            base::Unretained(this)));
    host_->SetMediaStreamDeviceObserverForTesting(
        host_->BindNewPipeAndPassRemote());
    host_->SetBadMessageCallbackForTesting(
        base::BindRepeating(&MediaStreamDispatcherHostTest::MockOnBadMessage,
                            base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::CrasAudioClient::InitializeFake();
    ash::CrasAudioHandler::InitializeForTesting();
#endif
  }

  ~MediaStreamDispatcherHostTest() override {
    audio_manager_->Shutdown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::CrasAudioHandler::Shutdown();
    ash::CrasAudioClient::Shutdown();
#endif
  }

  void SetUp() override {
    stub_video_device_ids_.emplace_back(kRegularVideoDeviceId1);
    stub_video_device_ids_.emplace_back(kDepthVideoDeviceId);
    ON_CALL(*mock_video_capture_provider_, GetDeviceInfosAsync(_))
        .WillByDefault(Invoke(
            [this](
                VideoCaptureProvider::GetDeviceInfosCallback result_callback) {
              std::vector<media::VideoCaptureDeviceInfo> result;
              for (const auto& device_id : stub_video_device_ids_) {
                media::VideoCaptureDeviceInfo info;
                info.descriptor.device_id = device_id;
                info.descriptor.capture_api = kStubCaptureApi;
                result.push_back(info);
              }
              std::move(result_callback)
                  .Run(media::mojom::DeviceEnumerationResult::kSuccess, result);
            }));

    base::RunLoop run_loop;
    MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[static_cast<size_t>(
        blink::mojom::MediaDeviceType::kMediaAudioInput)] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&AudioInputDevicesEnumerated, run_loop.QuitClosure(),
                       &audio_device_descriptions_));
    run_loop.Run();

    ASSERT_GT(audio_device_descriptions_.size(), 0u);
  }

  void TearDown() override {
    host_->CancelAllRequests();
    host_.reset();
  }

  void GetSaltAndOrigin(GlobalRenderFrameHostId,
                        MediaDeviceSaltAndOriginCallback callback) {
    std::move(callback).Run(salt_and_origin_);
  }

  MOCK_METHOD2(MockOnBadMessage, void(int, bad_message::BadMessageReason));

 protected:
  std::unique_ptr<FakeMediaStreamUIProxy> CreateMockUI(bool expect_started) {
    std::unique_ptr<MockMediaStreamUIProxy> fake_ui =
        std::make_unique<MockMediaStreamUIProxy>();

    if (expect_started) {
      EXPECT_CALL(*fake_ui, MockOnStarted(_));
    }
    return fake_ui;
  }

  virtual void SetupFakeUI(
      bool expect_started,
      const std::vector<std::pair<std::string, std::string>>&
          devices_ids_to_select = {}) {
    media_stream_manager_->UseFakeUIFactoryForTests(
        base::BindRepeating(&MediaStreamDispatcherHostTest::CreateMockUI,
                            base::Unretained(this), expect_started));
  }

  void GenerateStreamAndWaitForResult(
      int page_request_id,
      const blink::StreamControls& controls,
      const blink::mojom::StreamDevicesSet& expectation) {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_,
                OnStreamGenerationSuccess(page_request_id,
                                          SameTypesAs(std::ref(expectation))));
    host_->OnGenerateStreams(page_request_id, controls, run_loop.QuitClosure());
    run_loop.Run();
    for (const blink::mojom::StreamDevicesPtr& stream_devices :
         host_->stream_devices_set_->stream_devices) {
      EXPECT_FALSE(DoesContainRawIds(stream_devices->audio_device));
      EXPECT_FALSE(DoesContainRawIds(stream_devices->video_device));
      EXPECT_TRUE(DoesEveryDeviceMapToRawId(stream_devices->audio_device));
      EXPECT_TRUE(DoesEveryDeviceMapToRawId(stream_devices->video_device));
    }
  }

  void GenerateStreamAndWaitForFailure(
      int page_request_id,
      const blink::StreamControls& controls,
      blink::mojom::MediaStreamRequestResult expected_result) {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_,
                OnStreamGenerationFailure(page_request_id, expected_result));
    host_->OnGenerateStreams(page_request_id, controls, run_loop.QuitClosure());
    run_loop.Run();
  }

  void OpenVideoDeviceAndWaitForResult(int page_request_id,
                                       const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess());
    base::RunLoop run_loop;
    host_->OnOpenDevice(page_request_id, device_id,
                        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(video_device(/*stream_index=*/0u).value()));
    EXPECT_TRUE(
        DoesEveryDeviceMapToRawId(video_device(/*stream_index=*/0u).value()));
  }

  void OpenVideoDeviceAndWaitForFailure(int page_request_id,
                                        const std::string& device_id) {
    EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
    base::RunLoop run_loop;
    host_->OnOpenDevice(page_request_id, device_id,
                        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(video_device(/*stream_index=*/0u).value()));
    EXPECT_FALSE(
        DoesEveryDeviceMapToRawId(video_device(/*stream_index=*/0u).value()));
  }

  bool DoesContainRawIds(
      const std::optional<blink::MediaStreamDevice>& optional_device) {
    if (!optional_device.has_value()) {
      return false;
    }
    const blink::MediaStreamDevice& device = optional_device.value();
    if (device.id != media::AudioDeviceDescription::kDefaultDeviceId &&
        device.id != media::AudioDeviceDescription::kCommunicationsDeviceId) {
      for (const auto& audio_device : audio_device_descriptions_) {
        if (audio_device.unique_id == device.id) {
          return true;
        }
      }
    }
    for (const std::string& device_id : stub_video_device_ids_) {
      if (device_id == device.id) {
        return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(
      const std::optional<blink::MediaStreamDevice>& optional_device) {
    if (!optional_device.has_value()) {
      return true;
    }
    const blink::MediaStreamDevice& device = optional_device.value();
    if (device.type != blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
        device.type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
      return true;
    }
    bool found_match = false;
    media::AudioDeviceDescriptions::const_iterator audio_it =
        audio_device_descriptions_.begin();
    for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
      if (DoesRawMediaDeviceIDMatchHMAC(salt_and_origin_, device.id,
                                        audio_it->unique_id)) {
        EXPECT_FALSE(found_match) << "Multiple matches found.";
        found_match = true;
      }
    }
    for (const std::string& device_id : stub_video_device_ids_) {
      if (DoesRawMediaDeviceIDMatchHMAC(salt_and_origin_, device.id,
                                        device_id)) {
        EXPECT_FALSE(found_match) << "Multiple matches found.";
        found_match = true;
      }
    }
    return found_match;
  }

  void GetOpenDevice(
      int32_t request_id,
      const base::UnguessableToken& session_id,
      const base::UnguessableToken& transfer_id,
      MediaStreamDispatcherHost::GetOpenDeviceCallback callback) {
    host_->GetOpenDevice(request_id, session_id, transfer_id,
                         std::move(callback));
  }

  void KeepDeviceAliveForTransfer(
      const base::UnguessableToken& session_id,
      const base::UnguessableToken& transfer_id,
      MediaStreamDispatcherHost::KeepDeviceAliveForTransferCallback callback) {
    host_->KeepDeviceAliveForTransfer(session_id, transfer_id,
                                      std::move(callback));
  }

  const std::optional<blink::MediaStreamDevice>& audio_device(
      size_t stream_index) const {
    DCHECK_LT(stream_index, host_->stream_devices_set_->stream_devices.size());
    return host_->stream_devices_set_->stream_devices[stream_index]
        ->audio_device;
  }

  const std::optional<blink::MediaStreamDevice>& video_device(
      size_t stream_index) const {
    DCHECK_LT(stream_index, host_->stream_devices_set_->stream_devices.size());
    return host_->stream_devices_set_->stream_devices[stream_index]
        ->video_device;
  }

  void ResetMediaDeviceIDSalt() {
    salt_and_origin_.set_device_id_salt(CreateRandomMediaDeviceIDSalt());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockMediaStreamDispatcherHost> host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  BrowserTaskEnvironment task_environment_;
  MediaDeviceSaltAndOrigin salt_and_origin_;
  media::AudioDeviceDescriptions audio_device_descriptions_;
  std::vector<std::string> stub_video_device_ids_;
  raw_ptr<MockVideoCaptureProvider> mock_video_capture_provider_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithVideoOnly) {
  stub_video_device_ids_.emplace_back(kRegularVideoDeviceId2);
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioOnly) {
  blink::StreamControls controls(true, false);

  SetupFakeUI(true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), std::nullopt));
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_FALSE(video_device(/*stream_index=*/0u).has_value());
}

TEST_F(MediaStreamDispatcherHostTest,
       BadMessageIfAudioNotRequestedAndSuppressLocalAudioPlayback) {
  using blink::mojom::MediaStreamType;

  blink::StreamControls controls;
  controls.audio.stream_type = MediaStreamType::NO_SERVICE;
  controls.video.stream_type = MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  controls.suppress_local_audio_playback = true;

  SetupFakeUI(true);

  EXPECT_CALL(
      *this,
      MockOnBadMessage(
          kRenderFrameHostId.child_id,
          bad_message::
              MSDH_SUPPRESS_LOCAL_AUDIO_PLAYBACK_BUT_AUDIO_NOT_REQUESTED))
      .Times(1);
  host_->OnGenerateStreams(kPageRequestId, controls);
}

TEST_F(MediaStreamDispatcherHostTest,
       BadMessageIfAudioNotRequestedAndHotwordEnabled) {
  using blink::mojom::MediaStreamType;

  blink::StreamControls controls;
  controls.audio.stream_type = MediaStreamType::NO_SERVICE;
  controls.video.stream_type = MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  controls.hotword_enabled = true;

  SetupFakeUI(true);

  EXPECT_CALL(*this,
              MockOnBadMessage(
                  kRenderFrameHostId.child_id,
                  bad_message::MSDH_HOTWORD_ENABLED_BUT_AUDIO_NOT_REQUESTED))
      .Times(1);
  host_->OnGenerateStreams(kPageRequestId, controls);
}

TEST_F(MediaStreamDispatcherHostTest,
       BadMessageIfAudioNotRequestedAndDisableLocalEcho) {
  using blink::mojom::MediaStreamType;

  blink::StreamControls controls;
  controls.audio.stream_type = MediaStreamType::NO_SERVICE;
  controls.video.stream_type = MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  controls.disable_local_echo = true;

  SetupFakeUI(true);

  EXPECT_CALL(*this,
              MockOnBadMessage(
                  kRenderFrameHostId.child_id,
                  bad_message::MSDH_DISABLE_LOCAL_ECHO_BUT_AUDIO_NOT_REQUESTED))
      .Times(1);
  host_->OnGenerateStreams(kPageRequestId, controls);
}

// This test simulates a shutdown scenario: we don't setup a fake UI proxy for
// MediaStreamManager, so it will create an ordinary one which will not find
// a RenderFrameHostDelegate. This normally should only be the case at shutdown.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithNothing) {
  blink::StreamControls controls(false, false);

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioAndVideo) {
  blink::StreamControls controls(true, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithDepthVideo) {
  // We specify to generate both audio and video stream.
  blink::StreamControls controls(true, true);
  std::string source_id =
      GetHMACForRawMediaDeviceID(salt_and_origin_, kDepthVideoDeviceId);
  // |source_id| corresponds to the depth device. As we can generate only one
  // video stream using GenerateStreamAndWaitForResult, we use
  // controls.video.source_id to specify that the stream is depth video.
  // See also MediaStreamManager::GenerateStream and other tests here.
  controls.video.device_ids = {source_id};

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  // We specified the generation and expect to get
  // one audio and one depth video stream.
  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

// This test generates two streams with video only using the same render frame
// id. The same capture device with the same device and session id is expected
// to be used.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsFromSameRenderId) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  // Check the latest generated stream.
  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label1 = host_->label_;
  const std::string device_id1 = video_device(/*stream_index=*/0u).value().id;
  const base::UnguessableToken session_id1 =
      host_->stream_devices_set_->stream_devices[0]
          ->video_device.value()
          .session_id();

  // Generate second stream.
  GenerateStreamAndWaitForResult(kPageRequestId + 1, controls, expectation);

  // Check the latest generated stream.
  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label2 = host_->label_;
  const std::string device_id2 = video_device(/*stream_index=*/0u).value().id;
  const base::UnguessableToken session_id2 =
      host_->stream_devices_set_->stream_devices[0]
          ->video_device.value()
          .session_id();
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamAndOpenDeviceFromSameRenderFrame) {
  SetupFakeUI(true);
  blink::StreamControls controls(false, true);

  // Generate first stream.
  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label1 = host_->label_;
  const std::string device_id1 = video_device(/*stream_index=*/0u).value().id;
  const base::UnguessableToken session_id1 =
      host_->stream_devices_set_->stream_devices[0]
          ->video_device.value()
          .session_id();

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, device_id1);

  const std::string device_id2 = host_->opened_device_.id;
  const base::UnguessableToken session_id2 = host_->opened_device_.session_id();
  const std::string label2 = host_->label_;

  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test generates two streams with video only using two separate render
// frame ids. The same device id but different session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsDifferentRenderId) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  // Check the latest generated stream.
  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label1 = host_->label_;
  const std::string device_id1 = video_device(/*stream_index=*/0u).value().id;
  const base::UnguessableToken session_id1 =
      video_device(/*stream_index=*/0u).value().session_id();

  // Generate second stream from another render frame.
  host_ = std::make_unique<MockMediaStreamDispatcherHost>(
      GlobalRenderFrameHostId{kRenderFrameHostId.child_id,
                              kRenderFrameHostId.frame_routing_id + 1},
      media_stream_manager_.get());
  host_->set_get_salt_and_origin_cb_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));
  host_->SetMediaStreamDeviceObserverForTesting(
      host_->BindNewPipeAndPassRemote());

  GenerateStreamAndWaitForResult(kPageRequestId + 1, controls, expectation);

  // Check the latest generated stream.
  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label2 = host_->label_;
  const std::string device_id2 = video_device(/*stream_index=*/0u).value().id;
  const base::UnguessableToken session_id2 =
      host_->stream_devices_set_->stream_devices[0]
          ->video_device.value()
          .session_id();
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest, WebContentsNotFocused) {
  blink::StreamControls controls(true, false);

  salt_and_origin_.set_has_focus(false);
  host_->set_get_salt_and_origin_cb_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));

  base::RunLoop run_loop;
  EXPECT_CALL(
      *host_,
      OnStreamGenerationFailure(
          kPageRequestId,
          blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN));
  host_->OnGenerateStreams(kPageRequestId, controls, run_loop.QuitClosure());
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, WebContentsNotFocusedInBackgroundPage) {
  blink::StreamControls controls(true, true);

  SetupFakeUI(true);

  salt_and_origin_.set_has_focus(false);
  salt_and_origin_.set_is_background(true);
  host_->set_get_salt_and_origin_cb_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));

  base::RunLoop run_loop;
  host_->OnGenerateStreams(kPageRequestId, controls, run_loop.QuitClosure());

  std::optional<blink::MediaStreamDevice> expected_audio_device;
  if (controls.audio.requested() && !audio_device_descriptions_.empty()) {
    expected_audio_device = blink::MediaStreamDevice();
  }

  std::optional<blink::MediaStreamDevice> expected_video_device;
  if (controls.video.requested() && !stub_video_device_ids_.empty()) {
    expected_video_device = blink::MediaStreamDevice();
  }

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      expected_audio_device, expected_video_device));
  EXPECT_CALL(*host_, OnStreamGenerationSuccess(
                          kPageRequestId, SameTypesAs(std::ref(expectation))))
      .Times(1);

  run_loop.Run();
  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

TEST_F(MediaStreamDispatcherHostTest, WebContentsFocused) {
  blink::StreamControls controls(true, true);

  SetupFakeUI(true);

  salt_and_origin_.set_has_focus(false);
  host_->set_get_salt_and_origin_cb_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));

  base::RunLoop run_loop;
  host_->OnGenerateStreams(kPageRequestId, controls, run_loop.QuitClosure());
  run_loop.RunUntilIdle();

  std::optional<blink::MediaStreamDevice> expected_audio_device;
  if (controls.audio.requested() && !audio_device_descriptions_.empty()) {
    expected_audio_device = blink::MediaStreamDevice();
  }

  std::optional<blink::MediaStreamDevice> expected_video_device;
  if (controls.video.requested() && !stub_video_device_ids_.empty()) {
    expected_video_device = blink::MediaStreamDevice();
  }

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      expected_audio_device, expected_video_device));
  EXPECT_CALL(*host_, OnStreamGenerationSuccess(
                          kPageRequestId, SameTypesAs(std::ref(expectation))))
      .Times(1);

  salt_and_origin_.set_has_focus(true);
  host_->set_get_salt_and_origin_cb_for_testing(
      base::BindRepeating(&MediaStreamDispatcherHostTest::GetSaltAndOrigin,
                          base::Unretained(this)));
  host_->OnWebContentsFocused();

  run_loop.Run();
  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

// This test request two streams with video only without waiting for the first
// stream to be generated before requesting the second.
// The same device id and session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithoutWaiting) {
  blink::StreamControls controls(false, true);

  // Generate first stream.
  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  SetupFakeUI(true);
  {
    InSequence s;
    EXPECT_CALL(*host_,
                OnStreamGenerationSuccess(kPageRequestId,
                                          SameTypesAs(std::ref(expectation))));

    // Generate second stream.
    EXPECT_CALL(*host_,
                OnStreamGenerationSuccess(kPageRequestId + 1,
                                          SameTypesAs(std::ref(expectation))));
  }
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  host_->OnGenerateStreams(kPageRequestId, controls, run_loop1.QuitClosure());
  host_->OnGenerateStreams(kPageRequestId + 1, controls,
                           run_loop2.QuitClosure());

  run_loop1.Run();
  run_loop2.Run();
}

// Test that we can generate streams where a sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithSourceId) {
  ASSERT_GE(audio_device_descriptions_.size(), 1u);
  ASSERT_GE(stub_video_device_ids_.size(), 1u);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), blink::MediaStreamDevice()));

  media::AudioDeviceDescriptions::const_iterator audio_it =
      audio_device_descriptions_.begin();
  for (; audio_it != audio_device_descriptions_.end(); ++audio_it) {
    std::string source_id =
        GetHMACForRawMediaDeviceID(salt_and_origin_, audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    blink::StreamControls controls(true, true);
    controls.audio.device_ids = {source_id};

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
    EXPECT_EQ(audio_device(/*stream_index=*/0u).value().id, source_id);
  }

  for (const std::string& device_id : stub_video_device_ids_) {
    std::string source_id =
        GetHMACForRawMediaDeviceID(salt_and_origin_, device_id);
    ASSERT_FALSE(source_id.empty());
    blink::StreamControls controls(true, true);
    controls.video.device_ids = {source_id};

    GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
    EXPECT_EQ(video_device(/*stream_index=*/0u).value().id, source_id);
  }
}

// Test that generating a stream with an invalid video source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidVideoSourceId) {
  blink::StreamControls controls(true, true);
  controls.video.device_ids = {"invalid source id"};

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

// Test that generating a stream with an invalid audio source id fail.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithInvalidAudioSourceId) {
  blink::StreamControls controls(true, true);
  controls.audio.device_ids = {"invalid source id"};

  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsNoAvailableVideoDevice) {
  stub_video_device_ids_.clear();
  blink::StreamControls controls(true, true);

  SetupFakeUI(false);
  GenerateStreamAndWaitForFailure(
      kPageRequestId, controls,
      blink::mojom::MediaStreamRequestResult::NO_HARDWARE);
}

// Test that if a OnStopStreamDevice message is received for a device that has
// been opened in a MediaStream and by pepper, the device is only stopped for
// the MediaStream.
TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStream) {
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  std::string stream_request_label = host_->label_;
  blink::MediaStreamDevice current_video_device =
      video_device(/*stream_index=*/0u).value();
  ASSERT_EQ(
      1u, media_stream_manager_->GetDevicesOpenedByRequest(stream_request_label)
              .size());

  // Open the same device by Pepper.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, current_video_device.id);
  std::string open_device_request_label = host_->label_;

  // Stop the device in the MediaStream.
  host_->OnStopStreamDevice(current_video_device.id,
                            current_video_device.session_id());

  EXPECT_EQ(
      0u, media_stream_manager_->GetDevicesOpenedByRequest(stream_request_label)
              .size());
  EXPECT_EQ(1u, media_stream_manager_
                    ->GetDevicesOpenedByRequest(open_device_request_label)
                    .size());
}

TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStreamAndRestart) {
  blink::StreamControls controls(true, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), blink::MediaStreamDevice()));
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  std::string request_label1 = host_->label_;
  blink::MediaStreamDevice current_video_device =
      video_device(/*stream_index=*/0u).value();
  // Expect that 1 audio and 1 video device has been opened.
  EXPECT_EQ(
      2u,
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1).size());

  host_->OnStopStreamDevice(current_video_device.id,
                            current_video_device.session_id());
  EXPECT_EQ(
      1u,
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1).size());

  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
  std::string request_label2 = host_->label_;

  blink::MediaStreamDevices request1_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1);
  blink::MediaStreamDevices request2_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label2);

  ASSERT_EQ(1u, request1_devices.size());
  ASSERT_EQ(2u, request2_devices.size());

  // Test that the same audio device has been opened in both streams.
  EXPECT_TRUE(request1_devices[0].IsSameDevice(request2_devices[0]) ||
              request1_devices[0].IsSameDevice(request2_devices[1]));
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateTwoStreamsAndStopDeviceWhileWaitingForSecondStream) {
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());

  // Generate a second stream.
  EXPECT_CALL(*host_,
              OnStreamGenerationSuccess(kPageRequestId + 1,
                                        SameTypesAs(std::ref(expectation))));

  base::RunLoop run_loop1;
  host_->OnGenerateStreams(kPageRequestId + 1, controls,
                           run_loop1.QuitClosure());

  // Stop the video stream device from stream 1 while waiting for the
  // second stream to be generated.
  host_->OnStopStreamDevice(video_device(/*stream_index=*/0u).value().id,
                            host_->stream_devices_set_->stream_devices[0]
                                ->video_device.value()
                                .session_id());
  run_loop1.Run();

  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreams) {
  blink::StreamControls controls(false, true);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStreams(kPageRequestId + i, controls,
                             run_loop.QuitClosure());
  }

  media_stream_manager_->CancelAllRequests(kRenderFrameHostId, kRequesterId);
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreams) {
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));

  SetupFakeUI(true);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i) {
    GenerateStreamAndWaitForResult(kPageRequestId + i, controls, expectation);
  }

  media_stream_manager_->CancelAllRequests(kRenderFrameHostId, kRequesterId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));

  base::OnceClosure close_callback;
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
      [](base::OnceClosure* close_callback) {
        std::unique_ptr<FakeMediaStreamUIProxy> stream_ui =
            std::make_unique<MockMediaStreamUIProxy>();
        EXPECT_CALL(*static_cast<MockMediaStreamUIProxy*>(stream_ui.get()),
                    MockOnStarted(_))
            .WillOnce(MoveArg<0>(close_callback));
        return stream_ui;
      },
      &close_callback));

  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);

  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());

  ASSERT_TRUE(close_callback);
  EXPECT_CALL(*host_, OnDeviceStopSuccess());
  std::move(close_callback).Run();
  base::RunLoop().RunUntilIdle();
}

// Test that the observer is notified if a video device that is in use is
// being unplugged.
TEST_F(MediaStreamDispatcherHostTest, VideoDeviceUnplugged) {
  blink::StreamControls controls(true, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      blink::MediaStreamDevice(), blink::MediaStreamDevice()));

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
  EXPECT_TRUE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());

  stub_video_device_ids_.clear();

  base::RunLoop run_loop;
  EXPECT_CALL(*host_, OnDeviceStopSuccess())
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  media_stream_manager_->media_devices_manager()->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  run_loop.Run();
}

// Test that changing the salt invalidates device IDs. Attempts to open an
// invalid device ID result in failure.
TEST_F(MediaStreamDispatcherHostTest, Salt) {
  SetupFakeUI(true);
  blink::StreamControls controls(false, true);

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));

  // Generate first stream.
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
  EXPECT_FALSE(audio_device(/*stream_index=*/0u).has_value());
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  const std::string label1 = host_->label_;
  const std::string device_id1 = video_device(/*stream_index=*/0u).value().id;
  EXPECT_TRUE(host_->stream_devices_set_->stream_devices[0]
                  ->video_device.value()
                  .group_id.has_value());
  const std::string group_id1 = *host_->stream_devices_set_->stream_devices[0]
                                     ->video_device.value()
                                     .group_id;
  EXPECT_FALSE(group_id1.empty());
  const base::UnguessableToken session_id1 =
      host_->stream_devices_set_->stream_devices[0]
          ->video_device.value()
          .session_id();

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kPageRequestId, device_id1);
  const std::string device_id2 = host_->opened_device_.id;
  EXPECT_TRUE(host_->opened_device_.group_id.has_value());
  const std::string group_id2 = *host_->opened_device_.group_id;
  EXPECT_FALSE(group_id2.empty());
  const base::UnguessableToken session_id2 = host_->opened_device_.session_id();
  const std::string label2 = host_->label_;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(group_id1, group_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);

  // Reset salt and try to generate third stream with the invalidated device ID.
  ResetMediaDeviceIDSalt();
  EXPECT_CALL(*host_, OnDeviceOpenSuccess()).Times(0);
  OpenVideoDeviceAndWaitForFailure(kPageRequestId, device_id1);
  // Last open device ID and session are from the second stream.
  EXPECT_EQ(session_id2, host_->opened_device_.session_id());
  EXPECT_EQ(device_id2, host_->opened_device_.id);
  EXPECT_EQ(group_id2, host_->opened_device_.group_id);
}

TEST_F(MediaStreamDispatcherHostTest, GetOpenDeviceWithoutFeatureFails) {
  EXPECT_CALL(
      *this,
      MockOnBadMessage(kRenderFrameHostId.child_id,
                       bad_message::MSDH_GET_OPEN_DEVICE_USE_WITHOUT_FEATURE));

  base::RunLoop loop;
  GetOpenDevice(/*request_id=*/0,
                /*session_id=*/base::UnguessableToken(),
                /*transfer_id=*/base::UnguessableToken(),
                base::BindOnce([](blink::mojom::MediaStreamRequestResult result,
                                  blink::mojom::GetOpenDeviceResponsePtr ptr) {
                  EXPECT_EQ(
                      blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                      result);
                  EXPECT_FALSE(ptr);
                }).Then(loop.QuitClosure()));
  loop.Run();
}

TEST_F(MediaStreamDispatcherHostTest, GetOpenDeviceSucceeds) {
  scoped_feature_list_.Reset();
  scoped_feature_list_
      .InitFromCommandLine(/*enable_features=*/
                           "UserMediaCaptureOnFocus,GetAllScreensMedia,"
                           "MediaStreamTrackTransfer",
                           /*disable_features=*/"");
  base::RunLoop loop;

  // Generate stream.
  SetupFakeUI(true);
  blink::StreamControls controls(false, true);
  controls.video.stream_type =
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;

  blink::mojom::StreamDevicesSet expectation;
  expectation.stream_devices.emplace_back(blink::mojom::StreamDevices::New(
      std::nullopt, blink::MediaStreamDevice()));
  GenerateStreamAndWaitForResult(kPageRequestId, controls, expectation);
  EXPECT_TRUE(video_device(/*stream_index=*/0u).has_value());
  blink::MediaStreamDevice current_video_device =
      video_device(/*stream_index=*/0u).value();
  const base::UnguessableToken session_id = current_video_device.session_id();

  // Get stream generated above.
  base::UnguessableToken transfer_id = base::UnguessableToken::Create();
  GetOpenDevice(/*request_id=*/1, session_id, transfer_id,
                base::BindOnce(
                    [](const std::string device_id,
                       const base::UnguessableToken& session_id,
                       blink::mojom::MediaStreamRequestResult result,
                       blink::mojom::GetOpenDeviceResponsePtr ptr) {
                      EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK,
                                result);
                      EXPECT_TRUE(ptr);
                      EXPECT_EQ(ptr->device.id, device_id);
                      EXPECT_NE(ptr->device.session_id(), session_id);
                    },
                    current_video_device.id, session_id)
                    .Then(loop.QuitClosure()));

  // Keep MediaStreamDevice alive for GetOpenDevice to complete.
  KeepDeviceAliveForTransfer(
      session_id, transfer_id, base::BindOnce([](bool device_found) {
                                 EXPECT_TRUE(device_found);
                               }).Then(base::BindLambdaForTesting([&]() {
        host_->OnStopStreamDevice(current_video_device.id, session_id);
      })));
  loop.Run();
}

TEST_F(MediaStreamDispatcherHostTest,
       RegisterAndUnregisterWithMediaStreamManager) {
  {
    mojo::Remote<blink::mojom::MediaStreamDispatcherHost> client;
    MediaStreamDispatcherHost::Create(kRenderFrameHostId,
                                      media_stream_manager_.get(),
                                      client.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(client.is_bound());
    EXPECT_EQ(media_stream_manager_->num_dispatcher_hosts(), 1u);
  }

  task_environment_.RunUntilIdle();
  // At this point, the pipe is closed and the MediaStreamDispatcherHost should
  // be removed from MediaStreamManager.
  EXPECT_EQ(media_stream_manager_->num_dispatcher_hosts(), 0u);
}

// TODO(crbug.com/40216442): Add test cases for multi stream generation.

class MediaStreamDispatcherHostStreamTypeCombinationTest
    : public MediaStreamDispatcherHostTest,
      public ::testing::WithParamInterface<std::tuple<int, int>> {};

TEST_P(MediaStreamDispatcherHostStreamTypeCombinationTest,
       GenerateStreamWithStreamTypeCombination) {
  using blink::mojom::MediaStreamType;

  std::set<std::tuple<MediaStreamType, MediaStreamType>> kValidCombinations = {
      {MediaStreamType::NO_SERVICE, MediaStreamType::NO_SERVICE},
      {MediaStreamType::NO_SERVICE, MediaStreamType::DEVICE_VIDEO_CAPTURE},
      {MediaStreamType::NO_SERVICE, MediaStreamType::GUM_TAB_VIDEO_CAPTURE},
      {MediaStreamType::NO_SERVICE, MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE},
      {MediaStreamType::NO_SERVICE, MediaStreamType::DISPLAY_VIDEO_CAPTURE},
      {MediaStreamType::NO_SERVICE, MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET},
      {MediaStreamType::NO_SERVICE,
       MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB},
      {MediaStreamType::DEVICE_AUDIO_CAPTURE, MediaStreamType::NO_SERVICE},
      {MediaStreamType::DEVICE_AUDIO_CAPTURE,
       MediaStreamType::DEVICE_VIDEO_CAPTURE},
      {MediaStreamType::GUM_TAB_AUDIO_CAPTURE, MediaStreamType::NO_SERVICE},
      {MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
       MediaStreamType::GUM_TAB_VIDEO_CAPTURE},
      {MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
       MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE},
      {MediaStreamType::DISPLAY_AUDIO_CAPTURE, MediaStreamType::NO_SERVICE},
      {MediaStreamType::DISPLAY_AUDIO_CAPTURE,
       MediaStreamType::DISPLAY_VIDEO_CAPTURE},
      {MediaStreamType::DISPLAY_AUDIO_CAPTURE,
       MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB}};

  blink::StreamControls controls;

  controls.audio.stream_type =
      static_cast<MediaStreamType>(std::get<0>(GetParam()));

  controls.video.stream_type =
      static_cast<MediaStreamType>(std::get<1>(GetParam()));

  SetupFakeUI(true);
  EXPECT_CALL(*this, MockOnBadMessage(
                         kRenderFrameHostId.child_id,
                         bad_message::MSDH_INVALID_STREAM_TYPE_COMBINATION))
      .Times(!kValidCombinations.count(std::make_tuple(
          controls.audio.stream_type, controls.video.stream_type)));
  host_->OnGenerateStreams(kPageRequestId, controls);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MediaStreamDispatcherHostStreamTypeCombinationTest,
    ::testing::Combine(
        ::testing::Range(
            static_cast<int>(blink::mojom::MediaStreamType::NO_SERVICE),
            static_cast<int>(blink::mojom::MediaStreamType::NUM_MEDIA_TYPES)),
        ::testing::Range(
            static_cast<int>(blink::mojom::MediaStreamType::NO_SERVICE),
            static_cast<int>(blink::mojom::MediaStreamType::NUM_MEDIA_TYPES))));

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MOCK_METHOD(void,
              CheckGetAllScreensMediaAllowed,
              (content::RenderFrameHost * render_frame_host,
               base::OnceCallback<void(bool)> callback),
              (override));
};

class MediaStreamDispatcherHostMultiCaptureTest
    : public RenderViewHostTestHarness {
 public:
  MediaStreamDispatcherHostMultiCaptureTest() {
    SetBrowserClientForTesting(&content_browser_client_);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
  }

 protected:
  GlobalRenderFrameHostId global_rfh_id() {
    return static_cast<RenderFrameHostImpl*>(main_rfh())->GetGlobalId();
  }

  MockContentBrowserClient content_browser_client_;
};

TEST_F(MediaStreamDispatcherHostMultiCaptureTest,
       NoRenderFrameHostMultiCaptureNotAllowed) {
  GlobalRenderFrameHostId main_rfh_global_id = global_rfh_id();
  // Use a wrong id
  int main_render_process_id = main_rfh_global_id.child_id - 1;
  int render_frame_id = main_rfh_global_id.frame_routing_id - 1;

  base::test::TestFuture<
      MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>
      future;
  MediaStreamDispatcherHost::CheckRequestAllScreensAllowed(
      /*get_salt_and_origin_cb=*/
      base::BindOnce([](MediaDeviceSaltAndOriginCallback callback) {
        std::move(callback).Run(
            MediaDeviceSaltAndOrigin(/*device_id_salt=*/"", url::Origin()));
      }),
      future.GetCallback(), {main_render_process_id, render_frame_id});
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(
      future
          .Get<MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>()
          .request_allowed);
}

TEST_F(MediaStreamDispatcherHostMultiCaptureTest,
       RenderFrameHostExistsButNoPolicySetMultiCaptureNotAllowed) {
  GlobalRenderFrameHostId main_rfh_global_id = global_rfh_id();
  int main_render_process_id = main_rfh_global_id.child_id;
  int render_frame_id = main_rfh_global_id.frame_routing_id;
  EXPECT_CALL(content_browser_client_, CheckGetAllScreensMediaAllowed(_, _))
      .WillOnce(testing::Invoke([](content::RenderFrameHost* render_frame_host,
                                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      }));

  base::test::TestFuture<
      MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>
      future;
  MediaStreamDispatcherHost::CheckRequestAllScreensAllowed(
      /*get_salt_and_origin_cb=*/
      base::BindOnce([](MediaDeviceSaltAndOriginCallback callback) {
        std::move(callback).Run(
            MediaDeviceSaltAndOrigin(/*device_id_salt=*/"", url::Origin()));
      }),
      future.GetCallback(), {main_render_process_id, render_frame_id});
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(
      future
          .Get<MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>()
          .request_allowed);
}

TEST_F(MediaStreamDispatcherHostMultiCaptureTest,
       PolicySetMultiCaptureAllowed) {
  GlobalRenderFrameHostId main_rfh_global_id = global_rfh_id();
  int main_render_process_id = main_rfh_global_id.child_id;
  int render_frame_id = main_rfh_global_id.frame_routing_id;
  EXPECT_CALL(content_browser_client_, CheckGetAllScreensMediaAllowed(_, _))
      .WillOnce(testing::Invoke([](content::RenderFrameHost* render_frame_host,
                                   base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      }));

  base::test::TestFuture<
      MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>
      future;
  MediaStreamDispatcherHost::CheckRequestAllScreensAllowed(
      /*get_salt_and_origin_cb=*/
      base::BindOnce([](MediaDeviceSaltAndOriginCallback callback) {
        std::move(callback).Run(
            MediaDeviceSaltAndOrigin(/*device_id_salt=*/"", url::Origin()));
      }),
      future.GetCallback(), {main_render_process_id, render_frame_id});
  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(
      future
          .Get<MediaStreamDispatcherHost::GenerateStreamsUIThreadCheckResult>()
          .request_allowed);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class MediaStreamDispatcherHostCapturedSurfaceControlTest
    : public MediaStreamDispatcherHostTest {
 public:
  MediaStreamDispatcherHostCapturedSurfaceControlTest() {
    media_stream_manager_->SetCapturedSurfaceControllerFactoryForTesting(
        base::BindRepeating(
            [](GlobalRenderFrameHostId gdm_rfhid,
               WebContentsMediaCaptureId captured_wc_id,
               base::RepeatingCallback<void(int)> zoom_level_callback)
                -> std::unique_ptr<CapturedSurfaceController> {
              auto captured_surface_controller =
                  std::make_unique<MockCapturedSurfaceController>(
                      gdm_rfhid, captured_wc_id);
              captured_surface_controller->SetRequestPermissionResponse(
                  CapturedSurfaceControlResult::kSuccess);
              return captured_surface_controller;
            }));
  }

 protected:
  base::UnguessableToken SimulateGetDisplayMedia() {
    media_stream_manager_->UseFakeUIFactoryForTests(
        base::BindRepeating([]() {
          auto fake_ui = std::make_unique<FakeMediaStreamUIProxy>(
              /*tests_use_fake_render_frame_hosts=*/true);
          return std::unique_ptr<FakeMediaStreamUIProxy>(std::move(fake_ui));
        }),
        /*use_for_gum_desktop_capture=*/false,
        /*captured_tab_id=*/std::nullopt);

    blink::StreamControls controls(/*request_audio=*/false,
                                   /*request_video=*/true);
    controls.video.stream_type =
        blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;

    base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                           const std::string&,
                           blink::mojom::StreamDevicesSetPtr, bool>
        future;
    media_stream_manager_->GenerateStreams(
        kRenderFrameHostId, /*requester_id=*/1,
        /*page_request_id=*/1, controls, MediaDeviceSaltAndOrigin::Empty(),
        /*user_gesture=*/true,
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        future.GetCallback(),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());

    return future.Get<2>()->stream_devices[0]->video_device->session_id();
  }
};

TEST_F(MediaStreamDispatcherHostCapturedSurfaceControlTest,
       RequestPermissionIsForwarded) {
  base::UnguessableToken session_id = SimulateGetDisplayMedia();
  blink::mojom::MediaStreamDispatcherHost* msdh = host_.get();
  base::test::TestFuture<CapturedSurfaceControlResult> future;
  msdh->RequestCapturedSurfaceControlPermission(session_id,
                                                future.GetCallback());
  EXPECT_EQ(future.Get(), CapturedSurfaceControlResult::kSuccess);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace content
