// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/fake_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kTestUrl[] = "https://google.com";
constexpr char kAttackerUrl[] = "https://attacker.example";

class StubVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  void OnCaptureConfigurationChanged(
      const VideoCaptureControllerID& id) override {}
  void OnError(const VideoCaptureControllerID& id,
               media::VideoCaptureError error) override {}
  void OnNewBuffer(const VideoCaptureControllerID& id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override {}
  void OnBufferDestroyed(const VideoCaptureControllerID& id,
                         int buffer_id) override {}
  void OnBufferReady(const VideoCaptureControllerID& id,
                     const ReadyBuffer& buffer) override {}
  void OnFrameDropped(const VideoCaptureControllerID& id,
                      media::VideoCaptureFrameDropReason reason) override {}
  void OnNewCaptureVersion(const VideoCaptureControllerID& id,
                           media::CaptureVersion capture_version) override {}
  void OnFrameWithEmptyRegionCapture(
      const VideoCaptureControllerID& id) override {}
  void OnEnded(const VideoCaptureControllerID& id) override {}
  void OnStarted(const VideoCaptureControllerID& id) override {
    if (started_closure_) {
      std::move(started_closure_).Run();
    }
  }
  void OnStartedUsingGpuDecode(const VideoCaptureControllerID& id) override {}

  void WaitForStart() {
    base::RunLoop run_loop;
    started_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  base::OnceClosure started_closure_;
};

}  // namespace

class ImageCaptureImplTest : public RenderViewHostTestHarness {
 public:
  ImageCaptureImplTest() = default;
  ImageCaptureImplTest(const ImageCaptureImplTest&) = delete;
  ImageCaptureImplTest& operator=(const ImageCaptureImplTest&) = delete;
  ~ImageCaptureImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    origin_ = url::Origin::Create(GURL(kTestUrl));
    NavigateAndCommit(origin_.GetURL());
    auto receiver = image_capture_remote_.BindNewPipeAndPassReceiver();
    content::ImageCaptureImpl::Create(
        main_rfh(), std::move(receiver),
        /*skip_connecting_to_media_stream_manager_for_testing=*/true);
  }

  void TearDown() override {
    if (audio_manager_) {
      audio_manager_->Shutdown();
    }
    RenderViewHostTestHarness::TearDown();
  }

  void SetUpMediaStreamManager() {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>(/*use_real_thread=*/false));
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::make_unique<FakeVideoCaptureProvider>());
    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating([]() {
      return std::make_unique<FakeMediaStreamUIProxy>(
          /*tests_use_fake_render_frame_hosts=*/true);
    }));
    base::RunLoop().RunUntilIdle();
  }

  std::pair<std::string, blink::MediaStreamDevice> OpenVideoStreamForFrame(
      RenderFrameHost* rfh) {
    blink::StreamControls controls(/*request_audio=*/false,
                                   /*request_video=*/true);
    MediaDeviceSaltAndOrigin salt_and_origin(
        /*device_id_salt=*/"test_device_salt", rfh->GetLastCommittedOrigin(),
        /*group_id_salt=*/"test_group_salt");
    base::test::TestFuture<blink::mojom::MediaStreamRequestResult,
                           const std::string&,
                           blink::mojom::StreamDevicesSetPtr, bool>
        future;
    media_stream_manager_->GenerateStreams(
        rfh->GetGlobalId(), /*requester_id=*/1, /*page_request_id=*/1, controls,
        std::move(salt_and_origin), /*user_gesture=*/false,
        blink::mojom::StreamSelectionInfo::NewSearchOnlyByDeviceId({}),
        future.GetCallback(),
        /*device_stopped_callback=*/base::DoNothing(),
        /*device_changed_callback=*/base::DoNothing(),
        /*device_request_state_change_callback=*/base::DoNothing(),
        /*device_capture_configuration_change_callback=*/base::DoNothing(),
        /*device_capture_handle_change_callback=*/base::DoNothing(),
        /*zoom_level_change_callback=*/base::DoNothing());
    auto [result, label, stream_devices_set, pan_tilt_zoom_allowed] =
        future.Take();
    EXPECT_EQ(result, blink::mojom::MediaStreamRequestResult::OK);
    CHECK_EQ(stream_devices_set->stream_devices.size(), 1u);
    CHECK(stream_devices_set->stream_devices[0]->video_device.has_value());
    return {label, *stream_devices_set->stream_devices[0]->video_device};
  }

  MediaStreamManager* media_stream_manager() {
    return media_stream_manager_.get();
  }

  void HideView() { web_contents()->WasHidden(); }

  void SetPermissionForPTZ(blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetBrowserContext()->GetPermissionController();
    DCHECK(permission_controller);
    SetPermissionControllerOverride(permission_controller, origin_, origin_,
                                    blink::PermissionType::CAMERA_PAN_TILT_ZOOM,
                                    status);
  }

  void SetPhotoOptions(media::mojom::PhotoSettingsPtr photo_settings,
                       base::OnceCallback<void(bool)> callback) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    image_capture_remote_->SetPhotoOptions("", std::move(photo_settings),
                                           std::move(callback));
  }

 private:
  // Must outlive `RenderViewHostTestHarness`'s `task_environment_` (which is
  // reset in `RenderViewHostTestHarness::TearDown()`), because
  // `MediaStreamManager` observes IO message loop destruction.
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystemImpl> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;

  mojo::Remote<media::mojom::ImageCapture> image_capture_remote_;
  url::Origin origin_;
};

TEST_F(ImageCaptureImplTest, SetPhotoOptionsDefault) {
  base::test::TestFuture<bool> future;
  SetPhotoOptions(media::mojom::PhotoSettings::New(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithHiddenVisibility) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::GRANTED);

  HideView();

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_FALSE(future.Get());

  future.Clear();
  SetPhotoOptions(media::mojom::PhotoSettings::New(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// PTZ permission is always granted on Android (see
// MediaDevicesPermissionChecker::HasPanTiltZoomPermissionGrantedOnUIThread),
// overriding it will have no effect.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithPTZNoPermission) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::DENIED);

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif

TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithPTZWithPermission) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::GRANTED);

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ImageCaptureImplTest, PhotoOperationsScopedToRequestingFrame) {
  SetUpMediaStreamManager();

  // Open and start a live camera session owned by `main_rfh()`.
  auto [owner_label, owner_device] = OpenVideoStreamForFrame(main_rfh());
  ASSERT_FALSE(owner_device.id.empty());
  ASSERT_FALSE(owner_device.session_id().is_empty());

  StubVideoCaptureControllerEventHandler event_handler;
  const VideoCaptureControllerID client_id = base::UnguessableToken::Create();
  media::VideoCaptureParams capture_params;
  capture_params.requested_format = media::VideoCaptureFormat(
      gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420);
  base::test::TestFuture<const base::WeakPtr<VideoCaptureController>&>
      connect_future;
  media_stream_manager()->video_capture_manager()->ConnectClient(
      owner_device.session_id(), capture_params, client_id,
      main_rfh()->GetGlobalId(), &event_handler,
      /*is_allowed_on_lock_screen=*/false, connect_future.GetCallback());
  base::WeakPtr<VideoCaptureController> controller = connect_future.Get();
  ASSERT_TRUE(controller);
  event_handler.WaitForStart();

  const auto bind_image_capture = [](RenderFrameHost* rfh) {
    mojo::Remote<media::mojom::ImageCapture> remote;
    ImageCaptureImpl::Create(
        rfh, remote.BindNewPipeAndPassReceiver(),
        /*skip_connecting_to_media_stream_manager_for_testing=*/false);
    return remote;
  };

  mojo::Remote<media::mojom::ImageCapture> owner_remote =
      bind_image_capture(main_rfh());

  double initial_exposure_time = 0.0;
  {
    base::test::TestFuture<media::mojom::PhotoStatePtr> state_future;
    owner_remote->GetPhotoState(owner_device.id, state_future.GetCallback());
    media::mojom::PhotoStatePtr state = state_future.Take();
    EXPECT_GT(state->height->max, 0.0);
    initial_exposure_time = state->exposure_time->current;

    base::test::TestFuture<bool> options_future;
    owner_remote->SetPhotoOptions(owner_device.id,
                                  media::mojom::PhotoSettings::New(),
                                  options_future.GetCallback());
    EXPECT_TRUE(options_future.Get());

    base::test::TestFuture<media::mojom::BlobPtr> photo_future;
    owner_remote->TakePhoto(owner_device.id, photo_future.GetCallback());
    EXPECT_FALSE(photo_future.Take()->data.empty());
  }

  // A cross-origin subframe supplying `main_rfh()`'s `source_id` must not be
  // able to read photo state, alter camera settings, or capture photos.
  RenderFrameHost* cross_origin_subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kAttackerUrl),
          RenderFrameHostTester::For(main_rfh())->AppendChild("attacker"));
  mojo::Remote<media::mojom::ImageCapture> attacker_remote =
      bind_image_capture(cross_origin_subframe);

  const auto expect_photo_ops_rejected =
      [&](mojo::Remote<media::mojom::ImageCapture>& remote,
          const std::string& source_id) {
        base::test::TestFuture<media::mojom::PhotoStatePtr> state_future;
        remote->GetPhotoState(source_id, state_future.GetCallback());
        EXPECT_EQ(state_future.Take()->height->max, 0.0);

        auto settings = media::mojom::PhotoSettings::New();
        settings->has_exposure_time = true;
        settings->exposure_time = initial_exposure_time + 500.0;
        base::test::TestFuture<bool> options_future;
        remote->SetPhotoOptions(source_id, std::move(settings),
                                options_future.GetCallback());
        EXPECT_FALSE(options_future.Get());

        base::test::TestFuture<media::mojom::BlobPtr> photo_future;
        remote->TakePhoto(source_id, photo_future.GetCallback());
        EXPECT_TRUE(photo_future.Take()->data.empty());
      };

  expect_photo_ops_rejected(attacker_remote, owner_device.id);

  // Verify the owner's camera settings were not modified by the rejected call.
  {
    base::test::TestFuture<media::mojom::PhotoStatePtr> state_future;
    owner_remote->GetPhotoState(owner_device.id, state_future.GetCallback());
    EXPECT_EQ(state_future.Take()->exposure_time->current,
              initial_exposure_time);
  }

  // A same-origin subframe without its own open session is also rejected, even
  // though its origin's HMAC `source_id` matches `owner_device.id`.
  RenderFrameHost* same_origin_subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(
          GURL(kTestUrl),
          RenderFrameHostTester::For(main_rfh())->AppendChild("same_origin"));
  mojo::Remote<media::mojom::ImageCapture> same_origin_remote =
      bind_image_capture(same_origin_subframe);
  expect_photo_ops_rejected(same_origin_remote, owner_device.id);

  // Once the same-origin subframe opens its own stream for the camera, its
  // `ImageCapture` operations succeed; once it closes its stream, it is
  // rejected again while `main_rfh()`'s session remains usable.
  auto [subframe_label, subframe_device] =
      OpenVideoStreamForFrame(same_origin_subframe);
  EXPECT_EQ(subframe_device.id, owner_device.id);
  EXPECT_NE(subframe_device.session_id(), owner_device.session_id());
  {
    base::test::TestFuture<media::mojom::BlobPtr> photo_future;
    same_origin_remote->TakePhoto(subframe_device.id,
                                  photo_future.GetCallback());
    EXPECT_FALSE(photo_future.Take()->data.empty());
  }

  media_stream_manager()->CancelRequest(subframe_label);
  expect_photo_ops_rejected(same_origin_remote, subframe_device.id);
  {
    base::test::TestFuture<media::mojom::BlobPtr> photo_future;
    owner_remote->TakePhoto(owner_device.id, photo_future.GetCallback());
    EXPECT_FALSE(photo_future.Take()->data.empty());
  }

  media_stream_manager()->video_capture_manager()->DisconnectClient(
      controller.get(), client_id, &event_handler,
      media::VideoCaptureError::kNone);
  media_stream_manager()->CancelRequest(owner_label);
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
