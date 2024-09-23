// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using testing::_;
using testing::AtLeast;
using testing::Bool;
using testing::Combine;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Values;

namespace content {

static const char kFakeDeviceFactoryConfigString[] = "device-count=3";
static const float kFrameRateToRequest = 15.0f;

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  MOCK_METHOD3(DoOnNewBuffer,
               void(const VideoCaptureControllerID& id,
                    media::mojom::VideoBufferHandlePtr* buffer_handle,
                    int buffer_id));
  MOCK_METHOD2(OnBufferDestroyed,
               void(const VideoCaptureControllerID&, int buffer_id));
  MOCK_METHOD1(OnCaptureConfigurationChanged,
               void(const VideoCaptureControllerID&));
  MOCK_METHOD2(OnNewSubCaptureTargetVersion,
               void(const VideoCaptureControllerID&,
                    uint32_t sub_capture_target_version));
  MOCK_METHOD2(OnBufferReady,
               void(const VideoCaptureControllerID& id,
                    const ReadyBuffer& fullsized_buffer));
  MOCK_METHOD2(OnFrameDropped,
               void(const VideoCaptureControllerID& id,
                    media::VideoCaptureFrameDropReason reason));
  MOCK_METHOD1(OnFrameWithEmptyRegionCapture,
               void(const VideoCaptureControllerID&));
  MOCK_METHOD1(OnStarted, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(OnEnded, void(const VideoCaptureControllerID&));
  MOCK_METHOD2(OnError,
               void(const VideoCaptureControllerID&, media::VideoCaptureError));
  MOCK_METHOD1(OnStartedUsingGpuDecode, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(OnStoppedUsingGpuDecode, void(const VideoCaptureControllerID&));

  void OnNewBuffer(const VideoCaptureControllerID& id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override {
    DoOnNewBuffer(id, &buffer_handle, buffer_id);
  }
};

class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MOCK_METHOD2(Opened,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Closed,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Aborted,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
};

using DeviceIndex = size_t;
using Resolution = gfx::Size;
using ExerciseAcceleratedJpegDecoding = bool;

// For converting the std::tuple<> used as test parameters back to something
// human-readable.
struct TestParams {
  TestParams() : device_index_to_use(0u) {}
  TestParams(const std::tuple<DeviceIndex,
                              Resolution,
                              ExerciseAcceleratedJpegDecoding>& params)
      : device_index_to_use(std::get<0>(params)),
        resolution_to_use(std::get<1>(params)),
        exercise_accelerated_jpeg_decoding(std::get<2>(params)) {}

  media::VideoPixelFormat GetPixelFormatToUse() {
    return (device_index_to_use == 1u) ? media::PIXEL_FORMAT_Y16
                                       : media::PIXEL_FORMAT_I420;
  }

  size_t device_index_to_use;
  gfx::Size resolution_to_use;
  bool exercise_accelerated_jpeg_decoding;
};

struct FrameInfo {
  gfx::Size size;
  media::VideoPixelFormat pixel_format;
  base::TimeDelta timestamp;
};

// Integration test that exercises the VideoCaptureManager instance running in
// the Browser process.
class VideoCaptureBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::
              tuple<DeviceIndex, Resolution, ExerciseAcceleratedJpegDecoding>> {
 public:
  VideoCaptureBrowserTest() {
    params_ = TestParams(GetParam());
  }

  VideoCaptureBrowserTest(const VideoCaptureBrowserTest&) = delete;
  VideoCaptureBrowserTest& operator=(const VideoCaptureBrowserTest&) = delete;

  ~VideoCaptureBrowserTest() override {}

  void SetUpAndStartCaptureDeviceOnIOThread(base::OnceClosure continuation) {
    video_capture_manager_ = media_stream_manager_->video_capture_manager();
    ASSERT_TRUE(video_capture_manager_);
    video_capture_manager_->RegisterListener(&mock_stream_provider_listener_);
    video_capture_manager_->EnumerateDevices(
        base::BindOnce(&VideoCaptureBrowserTest::OnDeviceDescriptorsReceived,
                       base::Unretained(this), std::move(continuation)));
  }

  void TearDownCaptureDeviceOnIOThread(base::OnceClosure continuation,
                                       bool post_to_end_of_message_queue) {
    // DisconnectClient() must not be called synchronously from either the
    // |done_cb| passed to StartCaptureForClient() nor any callback made to a
    // VideoCaptureControllerEventHandler. To satisfy this, we have to post our
    // invocation to the end of the IO message queue.
    if (post_to_end_of_message_queue) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
              base::Unretained(this), std::move(continuation), false));
      return;
    }

    video_capture_manager_->DisconnectClient(controller_.get(), stub_client_id_,
                                             &mock_controller_event_handler_,
                                             media::VideoCaptureError::kNone);

    // Store the |continuation| so it is not lost when we go out of scope, since
    // we can't store it in a lambda as gmock does not place nice and
    // base::test::RunOnceClosure() doesn't work for this scenario.
    close_callback_ = std::move(continuation);
    EXPECT_CALL(mock_stream_provider_listener_, Closed(_, _))
        .WillOnce(
            InvokeWithoutArgs([&]() { std::move(close_callback_).Run(); }));

    video_capture_manager_->Close(session_id_);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    kFakeDeviceFactoryConfigString);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    if (params_.exercise_accelerated_jpeg_decoding) {
      command_line->AppendSwitch(switches::kUseFakeMjpegDecodeAccelerator);
    } else {
      command_line->AppendSwitch(switches::kDisableAcceleratedMjpegDecode);
    }
  }

  // This cannot be part of an override of SetUp(), because at the time when
  // SetUp() is invoked, the BrowserMainLoop does not exist yet.
  void SetUpRequiringBrowserMainLoopOnMainThread() {
    BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
    ASSERT_TRUE(browser_main_loop);
    media_stream_manager_ = browser_main_loop->media_stream_manager();
    ASSERT_TRUE(media_stream_manager_);
  }

  void OnDeviceDescriptorsReceived(
      base::OnceClosure continuation,
      media::mojom::DeviceEnumerationResult result,
      const media::VideoCaptureDeviceDescriptors& descriptors) {
    ASSERT_EQ(media::mojom::DeviceEnumerationResult::kSuccess, result);
    ASSERT_TRUE(params_.device_index_to_use < descriptors.size());
    const auto& descriptor = descriptors[params_.device_index_to_use];
    blink::MediaStreamDevice media_stream_device(
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
        descriptor.device_id, descriptor.display_name(),
        media::VideoCaptureControlSupport(), descriptor.facing);
    session_id_ = video_capture_manager_->Open(media_stream_device);
    media::VideoCaptureParams capture_params;
    capture_params.requested_format = media::VideoCaptureFormat(
        params_.resolution_to_use, kFrameRateToRequest,
        params_.GetPixelFormatToUse());
    video_capture_manager_->ConnectClient(
        session_id_, capture_params, stub_client_id_,
        &mock_controller_event_handler_, std::nullopt,
        base::BindOnce(
            &VideoCaptureBrowserTest::OnConnectClientToControllerAnswer,
            base::Unretained(this), std::move(continuation)),
        /*browser_context=*/nullptr);
  }

  void OnConnectClientToControllerAnswer(
      base::OnceClosure continuation,
      const base::WeakPtr<VideoCaptureController>& controller) {
    ASSERT_TRUE(controller.get());
    controller_ = controller;
    std::move(continuation).Run();
  }

 protected:
  TestParams params_;
  raw_ptr<MediaStreamManager, DanglingUntriaged> media_stream_manager_ =
      nullptr;
  raw_ptr<VideoCaptureManager, DanglingUntriaged> video_capture_manager_ =
      nullptr;
  base::UnguessableToken session_id_;
  const VideoCaptureControllerID stub_client_id_ =
      base::UnguessableToken::Create();
  base::OnceClosure close_callback_;
  MockMediaStreamProviderListener mock_stream_provider_listener_;
  MockVideoCaptureControllerEventHandler mock_controller_event_handler_;
  base::WeakPtr<VideoCaptureController> controller_;
};

IN_PROC_BROWSER_TEST_P(VideoCaptureBrowserTest, StartAndImmediatelyStop) {
  SetUpRequiringBrowserMainLoopOnMainThread();
  base::RunLoop run_loop;
  base::OnceClosure quit_run_loop_on_current_thread_cb =
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure());
  base::OnceClosure after_start_continuation =
      base::BindOnce(&VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
                     base::Unretained(this),
                     std::move(quit_run_loop_on_current_thread_cb), true);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoCaptureBrowserTest::SetUpAndStartCaptureDeviceOnIOThread,
          base::Unretained(this), std::move(after_start_continuation)));
  run_loop.Run();
}

// Flaky on MSAN. https://crbug.com/840294
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_ReceiveFramesFromFakeCaptureDevice \
  DISABLED_ReceiveFramesFromFakeCaptureDevice
#else
#define MAYBE_ReceiveFramesFromFakeCaptureDevice \
  ReceiveFramesFromFakeCaptureDevice
#endif
IN_PROC_BROWSER_TEST_P(VideoCaptureBrowserTest,
                       MAYBE_ReceiveFramesFromFakeCaptureDevice) {
  // Only fake device with index 2 delivers MJPEG.
  if (params_.exercise_accelerated_jpeg_decoding &&
      params_.device_index_to_use != 2) {
    return;
  }

  SetUpRequiringBrowserMainLoopOnMainThread();

  std::vector<FrameInfo> received_frame_infos;
  static const size_t kMinFramesToReceive = 5;
  static const size_t kMaxFramesToReceive = 300;
  base::RunLoop run_loop;

  base::OnceClosure quit_run_loop_on_current_thread_cb =
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure());
  base::OnceClosure finish_test_cb =
      base::BindOnce(&VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
                     base::Unretained(this),
                     std::move(quit_run_loop_on_current_thread_cb), true);

  bool must_wait_for_gpu_decode_to_start = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (params_.exercise_accelerated_jpeg_decoding) {
    // Since the GPU jpeg decoder is created asynchronously while decoding
    // in software is ongoing, we have to keep pushing frames until a message
    // arrives that tells us that the GPU decoder is being used. Otherwise,
    // it may happen that all test frames are decoded using the non-GPU
    // decoding path before the GPU decoder has started getting used.
    must_wait_for_gpu_decode_to_start = true;
    EXPECT_CALL(mock_controller_event_handler_, OnStartedUsingGpuDecode(_))
        .WillOnce(InvokeWithoutArgs([&must_wait_for_gpu_decode_to_start]() {
          must_wait_for_gpu_decode_to_start = false;
        }));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_CALL(mock_controller_event_handler_, DoOnNewBuffer(_, _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_controller_event_handler_, OnBufferReady(_, _))
      .WillRepeatedly(Invoke(
          [this, &received_frame_infos, &must_wait_for_gpu_decode_to_start,
           &finish_test_cb](const VideoCaptureControllerID& id,
                            const ReadyBuffer& buffer) {
            FrameInfo received_frame_info;
            received_frame_info.pixel_format = buffer.frame_info->pixel_format;
            received_frame_info.size = buffer.frame_info->coded_size;
            received_frame_info.timestamp = buffer.frame_info->timestamp;
            received_frame_infos.emplace_back(received_frame_info);

            const media::VideoCaptureFeedback kArbitraryFeedback =
                media::VideoCaptureFeedback(0.5, 60.0,
                                            std::numeric_limits<int>::max());
            controller_->ReturnBuffer(id, &mock_controller_event_handler_,
                                      buffer.buffer_id, kArbitraryFeedback);

            if ((received_frame_infos.size() >= kMinFramesToReceive &&
                 !must_wait_for_gpu_decode_to_start) ||
                (received_frame_infos.size() == kMaxFramesToReceive)) {
              std::move(finish_test_cb).Run();
            }
          }));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoCaptureBrowserTest::SetUpAndStartCaptureDeviceOnIOThread,
          base::Unretained(this), base::DoNothing()));
  run_loop.Run();

  EXPECT_FALSE(must_wait_for_gpu_decode_to_start);
  EXPECT_GE(received_frame_infos.size(), kMinFramesToReceive);
  EXPECT_LT(received_frame_infos.size(), kMaxFramesToReceive);
  base::TimeDelta previous_timestamp;
  bool first_frame = true;
  for (const auto& frame_info : received_frame_infos) {
    EXPECT_EQ(params_.GetPixelFormatToUse(), frame_info.pixel_format);
    EXPECT_EQ(params_.resolution_to_use, frame_info.size);
    // Timestamps are expected to increase
    if (!first_frame)
      EXPECT_GT(frame_info.timestamp, previous_timestamp);
    first_frame = false;
    previous_timestamp = frame_info.timestamp;
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoCaptureBrowserTest,
                         Combine(Values(0, 1, 2),             // DeviceIndex
                                 Values(gfx::Size(640, 480),  // Resolution
                                        gfx::Size(1280, 720)),
                                 Bool()));  // ExerciseAcceleratedJpegDecoding

}  // namespace content
