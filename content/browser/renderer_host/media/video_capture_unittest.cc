// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/fake_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {

namespace {

void VideoInputDevicesEnumerated(
    base::OnceClosure quit_closure,
    const MediaDeviceSaltAndOrigin& salt_and_origin,
    blink::WebMediaDeviceInfoArray* out,
    const MediaDeviceEnumeration& enumeration) {
  for (const auto& info : enumeration[static_cast<size_t>(
           blink::mojom::MediaDeviceType::kMediaVideoInput)]) {
    std::string device_id =
        GetHMACForRawMediaDeviceID(salt_and_origin, info.device_id);
    out->emplace_back(device_id, info.label, std::string());
  }
  std::move(quit_closure).Run();
}

// Id used to identify the capture session between renderer and
// video_capture_host. This is an arbitrary value.
const base::UnguessableToken& DeviceId() {
  static const base::UnguessableToken device_id(
      base::UnguessableToken::CreateForTesting(555, 555));
  return device_id;
}

}  // namespace

ACTION_P2(ExitMessageLoop, task_runner, quit_closure) {
  task_runner->PostTask(FROM_HERE, quit_closure);
}

class MockRenderFrameHostDelegate
    : public VideoCaptureHost::RenderFrameHostDelegate {
 public:
  MOCK_METHOD0(NotifyStreamAdded, void());
  MOCK_METHOD0(NotifyStreamRemoved, void());
  MOCK_CONST_METHOD0(GetRenderFrameHostId, GlobalRenderFrameHostId());
};

// This is an integration test of VideoCaptureHost in conjunction with
// MediaStreamManager, VideoCaptureManager, VideoCaptureController, and
// VideoCaptureDevice.
class VideoCaptureTest : public testing::Test,
                         public media::mojom::VideoCaptureObserver {
 public:
  VideoCaptureTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

  VideoCaptureTest(const VideoCaptureTest&) = delete;
  VideoCaptureTest& operator=(const VideoCaptureTest&) = delete;

  ~VideoCaptureTest() override { audio_manager_->Shutdown(); }

  void SetUp() override {
    SetBrowserClientForTesting(&browser_client_);
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());

    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::make_unique<FakeVideoCaptureProvider>());
    media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
        &VideoCaptureTest::CreateFakeUI, base::Unretained(this)));

    // Create a Host and connect it to a simulated IPC channel.
    host_ = std::make_unique<VideoCaptureHost>(
        GlobalRenderFrameHostId() /* render_frame_host_id */,
        media_stream_manager_.get());

    OpenSession();
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(host_.get());
    EXPECT_TRUE(host_->controllers_.empty());

    CloseSession();

    host_.reset();
  }

  void OpenSession() {
    const GlobalRenderFrameHostId render_frame_host_id{1, 1};
    const int requester_id = 1;
    const int page_request_id = 1;
    const url::Origin security_origin =
        url::Origin::Create(GURL("http://test.com"));

    ASSERT_TRUE(opened_device_label_.empty());

    // Enumerate video devices.
    blink::WebMediaDeviceInfoArray video_devices;
    {
      base::RunLoop run_loop;
      MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
      devices_to_enumerate[static_cast<size_t>(
          blink::mojom::MediaDeviceType::kMediaVideoInput)] = true;
      base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
      GetMediaDeviceSaltAndOrigin(render_frame_host_id, future.GetCallback());
      MediaDeviceSaltAndOrigin salt_and_origin = future.Get();
      media_stream_manager_->media_devices_manager()->EnumerateDevices(
          devices_to_enumerate,
          base::BindOnce(&VideoInputDevicesEnumerated, run_loop.QuitClosure(),
                         std::move(salt_and_origin), &video_devices));
      run_loop.Run();
    }
    ASSERT_FALSE(video_devices.empty());

    // Open the first device.
    {
      base::test::TestFuture<const MediaDeviceSaltAndOrigin&> future;
      GetMediaDeviceSaltAndOrigin(render_frame_host_id, future.GetCallback());
      MediaDeviceSaltAndOrigin salt_and_origin = future.Get();

      base::RunLoop run_loop;
      media_stream_manager_->OpenDevice(
          render_frame_host_id, requester_id, page_request_id,
          video_devices[0].device_id,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt_and_origin,
          base::BindOnce(&VideoCaptureTest::OnDeviceOpened,
                         base::Unretained(this), run_loop.QuitClosure()),
          MediaStreamManager::DeviceStoppedCallback());
      run_loop.Run();
    }
    ASSERT_FALSE(opened_session_id_.is_empty());
  }

  void CloseSession() {
    if (opened_device_label_.empty())
      return;
    media_stream_manager_->CancelRequest(opened_device_label_);
    opened_device_label_.clear();
    opened_session_id_ = base::UnguessableToken();
  }

 protected:
  // media::mojom::VideoCaptureObserver implementation.
  void OnStateChanged(media::mojom::VideoCaptureResultPtr result) override {
    if (result->which() == media::mojom::VideoCaptureResult::Tag::kState)
      DoOnStateChanged(result->get_state());
    else
      DoOnVideoCaptureError(result->get_error_code());
  }
  MOCK_METHOD1(DoOnStateChanged, void(media::mojom::VideoCaptureState));
  MOCK_METHOD1(DoOnVideoCaptureError, void(media::VideoCaptureError));

  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    DoOnNewBuffer(buffer_id);
  }
  MOCK_METHOD1(DoOnNewBuffer, void(int32_t));
  void OnBufferReady(media::mojom::ReadyBufferPtr buffer) override {
    DoOnBufferReady(buffer->buffer_id);
  }
  MOCK_METHOD1(DoOnBufferReady, void(int32_t));
  MOCK_METHOD1(OnBufferDestroyed, void(int32_t));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason));
  MOCK_METHOD1(OnNewSubCaptureTargetVersion, void(uint32_t));

  void StartCapture() {
    base::RunLoop run_loop;
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::STARTED));
    EXPECT_CALL(*this, DoOnNewBuffer(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return());
    EXPECT_CALL(*this, DoOnBufferReady(_))
        .Times(AnyNumber())
        .WillRepeatedly(ExitMessageLoop(task_runner_, run_loop.QuitClosure()));

    host_->Start(DeviceId(), opened_session_id_, params,
                 observer_receiver_.BindNewPipeAndPassRemote());

    // Ensure that the browser context has been retrevied and the observer is
    // connected.
    observer_receiver_.FlushForTesting();

    run_loop.Run();
  }

  void StartCaptureWithInvalidSession() {
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this,
                DoOnVideoCaptureError(
                    media::VideoCaptureError::kVideoCaptureControllerInvalid))
        .Times(1);
    host_->Start(DeviceId(), base::UnguessableToken(), params,
                 observer_receiver_.BindNewPipeAndPassRemote());

    // Ensure that the browser context has been retrevied and the observer is
    // connected.
    observer_receiver_.FlushForTesting();
  }

  void StartAndImmediateStopCapture() {
    // Quickly start and then stop capture, without giving much chance for
    // asynchronous capture operations to produce frames.
    InSequence s;
    base::RunLoop run_loop;

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    // |STARTED| is reported asynchronously, which may not be received if
    // capture is stopped immediately.
    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::STARTED))
        .Times(AtMost(1));
    host_->Start(DeviceId(), opened_session_id_, params,
                 observer_receiver_.BindNewPipeAndPassRemote());

    // Ensure that the browser context has been retrevied and the observer is
    // connected.
    observer_receiver_.FlushForTesting();

    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::STOPPED));
    host_->Stop(DeviceId());
    run_loop.RunUntilIdle();
  }

  void PauseResumeCapture() {
    InSequence s;
    base::RunLoop run_loop;

    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::PAUSED));
    host_->Pause(DeviceId());

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::RESUMED));
    host_->Resume(DeviceId(), opened_session_id_, params);
    run_loop.RunUntilIdle();
  }

  void StopCapture() {
    base::RunLoop run_loop;

    EXPECT_CALL(*this,
                DoOnStateChanged(media::mojom::VideoCaptureState::STOPPED))
        .WillOnce(ExitMessageLoop(task_runner_, run_loop.QuitClosure()));
    host_->Stop(DeviceId());

    run_loop.Run();

    EXPECT_TRUE(host_->controllers_.empty());
  }

  void WaitForOneCapturedBuffer() {
    base::RunLoop run_loop;

    EXPECT_CALL(*this, DoOnBufferReady(_))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(task_runner_, run_loop.QuitClosure()))
        .RetiresOnSaturation();
    run_loop.Run();
  }

  void SimulateError() {
    EXPECT_CALL(
        *this,
        DoOnVideoCaptureError(
            media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest));
    host_->OnError(DeviceId(),
                   media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest);
    base::RunLoop().RunUntilIdle();
  }

  MediaStreamManager* media_stream_manager() const {
    return media_stream_manager_.get();
  }

 private:
  std::unique_ptr<FakeMediaStreamUIProxy> CreateFakeUI() {
    return std::make_unique<FakeMediaStreamUIProxy>(
        /*tests_use_fake_render_frame_hosts=*/true);
  }

  void OnDeviceOpened(base::OnceClosure quit_closure,
                      bool success,
                      const std::string& label,
                      const blink::MediaStreamDevice& opened_device) {
    if (success) {
      opened_device_label_ = label;
      opened_session_id_ = opened_device.session_id();
    }
    std::move(quit_closure).Run();
  }

  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  // |media_stream_manager_| needs to outlive |task_environment_| because it is
  // a CurrentThread::DestructionObserver.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  const content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestContentBrowserClient browser_client_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::UnguessableToken opened_session_id_;
  std::string opened_device_label_;

  std::unique_ptr<VideoCaptureHost> host_;
  mojo::Receiver<media::mojom::VideoCaptureObserver> observer_receiver_{this};
};

// Construct and destruct all objects. This is a non trivial sequence.
TEST_F(VideoCaptureTest, ConstructAndDestruct) {}

TEST_F(VideoCaptureTest, StartAndImmediateStop) {
  StartAndImmediateStopCapture();
}

TEST_F(VideoCaptureTest, StartAndCaptureAndStop) {
  StartCapture();
  WaitForOneCapturedBuffer();
  WaitForOneCapturedBuffer();
  StopCapture();
}

TEST_F(VideoCaptureTest, StartAndErrorAndStop) {
  StartCapture();
  SimulateError();
  StopCapture();
}

TEST_F(VideoCaptureTest, StartWithInvalidSessionId) {
  StartCaptureWithInvalidSession();
  StopCapture();
}

TEST_F(VideoCaptureTest, StartAndCaptureAndError) {
  EXPECT_CALL(*this, DoOnStateChanged(media::mojom::VideoCaptureState::STOPPED))
      .Times(0);
  StartCapture();
  WaitForOneCapturedBuffer();
  SimulateError();
  base::PlatformThread::Sleep(base::Milliseconds(200));
}

TEST_F(VideoCaptureTest, StartAndPauseAndResumeAndStop) {
  StartCapture();
  PauseResumeCapture();
  StopCapture();
}

TEST_F(VideoCaptureTest, CloseSessionWithoutStopping) {
  StartCapture();

  // When the session is closed via the stream without stopping capture, the
  // ENDED event is sent.
  EXPECT_CALL(*this, DoOnStateChanged(media::mojom::VideoCaptureState::ENDED));
  CloseSession();
  base::RunLoop().RunUntilIdle();
}

// Tests if RenderProcessHostDelegate methods are called as often as as
// expected.
TEST_F(VideoCaptureTest, IncrementMatchesDecrementCalls) {
  std::unique_ptr<MockRenderFrameHostDelegate> mock_delegate =
      std::make_unique<MockRenderFrameHostDelegate>();
  MockRenderFrameHostDelegate* const mock_delegate_ptr = mock_delegate.get();
  std::unique_ptr<VideoCaptureHost> host =
      std::make_unique<VideoCaptureHost>(std::move(mock_delegate), nullptr);

  const int kNumNotifyCalls = 3;
  EXPECT_CALL(*mock_delegate_ptr, NotifyStreamAdded()).Times(kNumNotifyCalls);
  EXPECT_CALL(*mock_delegate_ptr, NotifyStreamRemoved()).Times(kNumNotifyCalls);

  EXPECT_EQ(0u, host->number_of_active_streams_);
  for (int i = 0; i < kNumNotifyCalls; ++i)
    host->NotifyStreamAdded();
  EXPECT_EQ(kNumNotifyCalls, static_cast<int>(host->number_of_active_streams_));
  host->NotifyStreamRemoved();
  host->NotifyAllStreamsRemoved();
  EXPECT_EQ(0u, host->number_of_active_streams_);
}

TEST_F(VideoCaptureTest, RegisterAndUnregisterWithMediaStreamManager) {
  {
    mojo::Remote<media::mojom::VideoCaptureHost> client;
    VideoCaptureHost::Create(
        GlobalRenderFrameHostId() /* render_frame_host_id */,
        media_stream_manager(), client.BindNewPipeAndPassReceiver());
    EXPECT_TRUE(client.is_bound());
    EXPECT_EQ(media_stream_manager()->num_video_capture_hosts(), 1u);
  }

  base::RunLoop().RunUntilIdle();
  // At this point, the pipe is closed and the VideoCaptureHost should be
  // removed from MediaStreamManager.
  EXPECT_EQ(media_stream_manager()->num_video_capture_hosts(), 0u);
}

}  // namespace content
