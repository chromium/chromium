// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/mock_video_capture_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::StrEq;

namespace content {
namespace {

struct ControllerIDAndSize {
  ControllerIDAndSize(VideoCaptureControllerID id, gfx::Size size)
      : id(id), size(std::move(size)) {}

  VideoCaptureControllerID id;
  gfx::Size size;
};

bool operator==(const ControllerIDAndSize& x, const ControllerIDAndSize& y) {
  return x.id == y.id && x.size == y.size;
}

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller) {}
  ~MockVideoCaptureControllerEventHandler() override {}
  void set_enable_auto_return_buffer_on_buffer_ready(bool enable) {
    enable_auto_return_buffer_on_buffer_ready_ = enable;
  }

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD2(DoBufferCreated,
               void(const VideoCaptureControllerID&, int buffer_id));
  MOCK_METHOD2(DoBufferDestroyed,
               void(const VideoCaptureControllerID&, int buffer_id));
  MOCK_METHOD1(DoBufferReady, void(ControllerIDAndSize buffer));
  MOCK_METHOD1(OnCaptureConfigurationChanged,
               void(const VideoCaptureControllerID&));
  MOCK_METHOD1(OnFrameWithEmptyRegionCapture,
               void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoEnded, void(const VideoCaptureControllerID&));
  MOCK_METHOD2(DoError,
               void(const VideoCaptureControllerID&, media::VideoCaptureError));
  MOCK_METHOD1(OnStarted, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(OnStartedUsingGpuDecode, void(const VideoCaptureControllerID&));
  MOCK_METHOD2(OnNewSubCaptureTargetVersion,
               void(const VideoCaptureControllerID&, uint32_t));
  MOCK_METHOD2(OnFrameDropped,
               void(const VideoCaptureControllerID&,
                    media::VideoCaptureFrameDropReason));

  void OnError(const VideoCaptureControllerID& id,
               media::VideoCaptureError error) override {
    DoError(id, error);
  }
  void OnNewBuffer(const VideoCaptureControllerID& id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override {
    DoBufferCreated(id, buffer_id);
  }
  void OnBufferDestroyed(const VideoCaptureControllerID& id,
                         int buffer_id) override {
    DoBufferDestroyed(id, buffer_id);
  }
  void OnBufferReady(const VideoCaptureControllerID& id,
                     const ReadyBuffer& buffer) override {
    EXPECT_EQ(expected_pixel_format_, buffer.frame_info->pixel_format);
    EXPECT_EQ(expected_color_space_, buffer.frame_info->color_space);
    EXPECT_TRUE(buffer.frame_info->metadata.reference_time.has_value());
    DoBufferReady(ControllerIDAndSize(id, buffer.frame_info->coded_size));
    if (enable_auto_return_buffer_on_buffer_ready_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&VideoCaptureController::ReturnBuffer,
                                    base::Unretained(controller_), id, this,
                                    buffer.buffer_id, feedback_));
    }
  }
  void OnEnded(const VideoCaptureControllerID& id) override {
    DoEnded(id);
    // OnEnded() must respond by (eventually) unregistering the client.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(
                                      &VideoCaptureController::RemoveClient),
                                  base::Unretained(controller_), id, this));
  }

  raw_ptr<VideoCaptureController, DanglingUntriaged> controller_;
  media::VideoPixelFormat expected_pixel_format_ = media::PIXEL_FORMAT_I420;
  gfx::ColorSpace expected_color_space_ = gfx::ColorSpace::CreateREC709();
  media::VideoCaptureFeedback feedback_;
  bool enable_auto_return_buffer_on_buffer_ready_ = true;
};

// Test fixture for testing a unit consisting of an instance of
// VideoCaptureController connected to an instance of VideoCaptureDeviceClient,
// an instance of VideoCaptureBufferPoolImpl, as well as related threading glue
// that replicates how it is used in production.
class VideoCaptureControllerTest
    : public testing::Test,
      public testing::WithParamInterface<media::VideoPixelFormat> {
 public:
  VideoCaptureControllerTest()
      : arbitrary_format_(gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420),
        arbitrary_color_space_(gfx::ColorSpace::CreateREC709()),
        arbitrary_route_id_(base::UnguessableToken::Create()),
        arbitrary_session_id_(base::UnguessableToken::Create()) {}

  VideoCaptureControllerTest(const VideoCaptureControllerTest&) = delete;
  VideoCaptureControllerTest& operator=(const VideoCaptureControllerTest&) =
      delete;

  ~VideoCaptureControllerTest() override {}

 protected:
  static const int kPoolSize = 3;

  void SetUp() override {
    const std::string arbitrary_device_id = "arbitrary_device_id";
    const blink::mojom::MediaStreamType arbitrary_stream_type =
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    const media::VideoCaptureParams arbitrary_params;
    auto device_launcher = std::make_unique<MockVideoCaptureDeviceLauncher>();
    controller_ = new VideoCaptureController(
        arbitrary_device_id, arbitrary_stream_type, arbitrary_params,
        std::move(device_launcher), base::DoNothing());
    InitializeNewDeviceClientAndBufferPoolInstances();
    auto mock_launched_device =
        std::make_unique<MockLaunchedVideoCaptureDevice>();
    mock_launched_device_ = mock_launched_device.get();
    controller_->OnDeviceLaunched(std::move(mock_launched_device));
    client_a_ = std::make_unique<MockVideoCaptureControllerEventHandler>(
        controller_.get());
    client_b_ = std::make_unique<MockVideoCaptureControllerEventHandler>(
        controller_.get());
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  void InitializeNewDeviceClientAndBufferPoolInstances() {
    buffer_pool_ = new media::VideoCaptureBufferPoolImpl(
        media::VideoCaptureBufferType::kSharedMemory, kPoolSize);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    device_client_ = std::make_unique<media::VideoCaptureDeviceClient>(
        std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
            controller_->GetWeakPtrForIOThread(), GetIOThreadTaskRunner({})),
        buffer_pool_, media::VideoCaptureJpegDecoderFactoryCB());
#else
    device_client_ = std::make_unique<media::VideoCaptureDeviceClient>(
        std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
            controller_->GetWeakPtrForIOThread(), GetIOThreadTaskRunner({})),
        buffer_pool_, media::VideoEffectsContext({}));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SendStubFrameToDeviceClient(const media::VideoCaptureFormat format,
                                   const gfx::ColorSpace& color_space) {
    auto stub_frame = media::VideoFrame::CreateZeroInitializedFrame(
        format.pixel_format, format.frame_size,
        gfx::Rect(format.frame_size.width(), format.frame_size.height()),
        format.frame_size, base::TimeDelta());
    const int rotation = 0;
    const int frame_feedback_id = 0;
    device_client_->OnIncomingCapturedData(
        stub_frame->data(0),
        media::VideoFrame::AllocationSize(stub_frame->format(),
                                          stub_frame->coded_size()),
        format, color_space, rotation, false /* flip_y */, base::TimeTicks(),
        base::TimeDelta(), std::nullopt, frame_feedback_id);
  }

  BrowserTaskEnvironment task_environment_;
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
  std::unique_ptr<MockVideoCaptureControllerEventHandler> client_a_;
  std::unique_ptr<MockVideoCaptureControllerEventHandler> client_b_;
  scoped_refptr<VideoCaptureController> controller_;
  std::unique_ptr<media::VideoCaptureDevice::Client> device_client_;
  raw_ptr<MockLaunchedVideoCaptureDevice> mock_launched_device_;
  const float arbitrary_frame_rate_ = 10.0f;
  const base::TimeTicks arbitrary_reference_time_ = base::TimeTicks();
  const base::TimeDelta arbitrary_timestamp_ = base::TimeDelta();
  const media::VideoCaptureFormat arbitrary_format_;
  const gfx::ColorSpace arbitrary_color_space_;
  const VideoCaptureControllerID arbitrary_route_id_;
  const media::VideoCaptureSessionId arbitrary_session_id_;
};

// A simple test of VideoCaptureController's ability to add, remove, and keep
// track of clients.
TEST_F(VideoCaptureControllerTest, AddAndRemoveClients) {
  media::VideoCaptureParams session_params_1;
  session_params_1.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureParams session_params_2 = session_params_1;
  media::VideoCaptureParams session_params_3 = session_params_1;
  media::VideoCaptureParams session_params_4 = session_params_1;

  // Intentionally use the same route ID for two of the clients: the device_ids
  // are a per-VideoCaptureHost namespace, and can overlap across hosts.
  const VideoCaptureControllerID client_a_route_1 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_a_route_2 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_b_route_1 = client_a_route_2;
  const VideoCaptureControllerID client_b_route_2 =
      base::UnguessableToken::Create();

  const media::VideoCaptureSessionId session_id_1 =
      base::UnguessableToken::Create();
  const media::VideoCaptureSessionId session_id_2 =
      base::UnguessableToken::Create();
  const media::VideoCaptureSessionId session_id_3 =
      base::UnguessableToken::Create();
  const media::VideoCaptureSessionId session_id_4 =
      base::UnguessableToken::Create();

  // Clients in controller: []
  ASSERT_EQ(0u, controller_->GetClientCount())
      << "Client count should initially be zero.";
  controller_->AddClient(client_a_route_1, client_a_.get(), session_id_1,
                         session_params_1, std::nullopt);
  // Clients in controller: [A/1]
  ASSERT_EQ(1u, controller_->GetClientCount())
      << "Adding client A/1 should bump client count.";
  controller_->AddClient(client_a_route_2, client_a_.get(), session_id_2,
                         session_params_2, std::nullopt);
  // Clients in controller: [A/1, A/2]
  ASSERT_EQ(2u, controller_->GetClientCount())
      << "Adding client A/2 should bump client count.";
  controller_->AddClient(client_b_route_1, client_b_.get(), session_id_3,
                         session_params_3, std::nullopt);
  // Clients in controller: [A/1, A/2, B/1]
  ASSERT_EQ(3u, controller_->GetClientCount())
      << "Adding client B/1 should bump client count.";
  ASSERT_EQ(session_id_2,
            controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing client A/1 should return its session_id.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2u, controller_->GetClientCount());
  ASSERT_TRUE(
      controller_->RemoveClient(client_a_route_2, client_a_.get()).is_empty())
      << "Removing a nonexistant client should fail.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2u, controller_->GetClientCount());
  ASSERT_EQ(session_id_3,
            controller_->RemoveClient(client_b_route_1, client_b_.get()))
      << "Removing client B/1 should return its session_id.";
  // Clients in controller: [A/1]
  ASSERT_EQ(1u, controller_->GetClientCount());
  controller_->AddClient(client_b_route_2, client_b_.get(), session_id_4,
                         session_params_4, std::nullopt);
  // Clients in controller: [A/1, B/2]

  EXPECT_CALL(*client_a_, DoEnded(client_a_route_1)).Times(1);
  controller_->StopSession(session_id_1);  // Session 100 == client A/1
  Mock::VerifyAndClearExpectations(client_a_.get());
  ASSERT_EQ(2u, controller_->GetClientCount())
      << "Client should be closed but still exist after StopSession.";
  // Clients in controller: [A/1 (closed, removal pending), B/2]
  base::RunLoop().RunUntilIdle();
  // Clients in controller: [B/2]
  ASSERT_EQ(1u, controller_->GetClientCount())
      << "Client A/1 should be deleted by now.";
  controller_->StopSession(session_id_2);  // Session 200 does not exist anymore
  // Clients in controller: [B/2]
  ASSERT_EQ(1u, controller_->GetClientCount())
      << "Stopping non-existent session 200 should be a no-op.";
  controller_->StopSession(
      base::UnguessableToken::Create());  // Session never existed.
  // Clients in controller: [B/2]
  ASSERT_EQ(1u, controller_->GetClientCount())
      << "Stopping non-existent session should be a no-op.";
  ASSERT_TRUE(
      controller_->RemoveClient(client_a_route_1, client_a_.get()).is_empty())
      << "Removing already-removed client A/1 should fail.";
  // Clients in controller: [B/2]
  ASSERT_EQ(1u, controller_->GetClientCount())
      << "Removing non-existent session 200 should be a no-op.";
  ASSERT_EQ(session_id_4,
            controller_->RemoveClient(client_b_route_2, client_b_.get()))
      << "Removing client B/2 should return its session_id.";
  // Clients in controller: []
  ASSERT_EQ(0u, controller_->GetClientCount())
      << "Client count should return to zero after all clients are gone.";
}

// This test will connect and disconnect several clients while simulating an
// active capture device being started and generating frames. It runs on one
// thread and is intended to behave deterministically.
TEST_P(VideoCaptureControllerTest, NormalCaptureMultipleClients) {
  media::VideoCaptureParams session_params_1;
  const media::VideoPixelFormat format = GetParam();
  client_a_->expected_pixel_format_ = format;
  client_b_->expected_pixel_format_ = format;
  // OnIncomingCapturedBuffer keeps the color space unset. If needed use
  // OnIncomingCapturedBufferExt.
  client_a_->expected_color_space_ = gfx::ColorSpace();
  client_b_->expected_color_space_ = gfx::ColorSpace();

  session_params_1.requested_format =
      media::VideoCaptureFormat(gfx::Size(320, 240), 30, format);

  media::VideoCaptureParams session_params_2 = session_params_1;

  media::VideoCaptureParams session_params_3 = session_params_1;

  media::VideoCaptureParams session_params_4 = session_params_1;

  media::VideoCaptureFormat device_format(gfx::Size(444, 200), 25, format);

  const VideoCaptureControllerID client_a_route_1 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_a_route_2 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_b_route_1 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_b_route_2 =
      base::UnguessableToken::Create();

  const media::VideoCaptureSessionId session_id_1 =
      base::UnguessableToken::Create();
  const media::VideoCaptureSessionId session_id_2 =
      base::UnguessableToken::Create();
  const media::VideoCaptureSessionId session_id_3 =
      base::UnguessableToken::Create();

  controller_->AddClient(client_a_route_1, client_a_.get(), session_id_1,
                         session_params_1, std::nullopt);
  controller_->AddClient(client_b_route_1, client_b_.get(), session_id_3,
                         session_params_3, std::nullopt);
  controller_->AddClient(client_a_route_2, client_a_.get(), session_id_2,
                         session_params_2, std::nullopt);
  ASSERT_EQ(3u, controller_->GetClientCount());

  // Now, simulate an incoming captured buffer from the capture device. As a
  // side effect this will cause the first buffer to be shared with clients.
  uint8_t buffer_no = 1;
  const int arbitrary_frame_feedback_id = 101;
  ASSERT_EQ(0.0, device_client_->GetBufferPoolUtilization());
  media::VideoCaptureDevice::Client::Buffer buffer;
  const auto result_code = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id, &buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            result_code);
  auto buffer_access = buffer.handle_provider->GetHandleForInProcessAccess();
  ASSERT_EQ(1.0 / kPoolSize, device_client_->GetBufferPoolUtilization());
  memset(buffer_access->data(), buffer_no++, buffer_access->mapped_size());
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1, _));
    EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                                client_a_route_1, device_format.frame_size)));
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1, _));
    EXPECT_CALL(*client_b_, DoBufferReady(ControllerIDAndSize(
                                client_b_route_1, device_format.frame_size)));
  }
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2, _));
    EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                                client_a_route_2, device_format.frame_size)));
  }
  client_a_->feedback_.resource_utilization = 0.5;
  client_b_->feedback_.resource_utilization = -1.0;
  // Expect VideoCaptureController to call the load observer with a
  // resource utilization of 0.5 (the largest of all reported values).
  media::VideoCaptureFeedback kExpectedFeedback =
      media::VideoCaptureFeedback(0.5);
  kExpectedFeedback.frame_id = arbitrary_frame_feedback_id;
  EXPECT_CALL(*mock_launched_device_, OnUtilizationReport(kExpectedFeedback));

  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  Mock::VerifyAndClearExpectations(mock_launched_device_);

  // Second buffer which ought to use the same shared memory buffer. In this
  // case pretend that the Buffer pointer is held by the device for a long
  // delay. This shouldn't affect anything.
  const int arbitrary_frame_feedback_id_2 = 102;
  media::VideoCaptureDevice::Client::Buffer buffer2;
  const auto result_code_2 = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id_2, &buffer2,
      /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            result_code_2);
  auto buffer2_access = buffer2.handle_provider->GetHandleForInProcessAccess();
  memset(buffer2_access->data(), buffer_no++, buffer2_access->mapped_size());

  client_a_->feedback_ = media::VideoCaptureFeedback(0.5, 60, 1000);
  client_a_->feedback_.frame_id = arbitrary_frame_feedback_id_2;
  client_b_->feedback_ = media::VideoCaptureFeedback(3.14, 30);
  client_b_->feedback_.frame_id = arbitrary_frame_feedback_id_2;

  // Expect VideoCaptureController to call the load observer with a
  // resource utilization of 3.14 (the largest of all reported values) and
  // sink constraints being the minimum of all reported values.
  auto expected_feedback_2 = media::VideoCaptureFeedback(3.14, 30, 1000);
  expected_feedback_2.frame_id = arbitrary_frame_feedback_id_2;
  EXPECT_CALL(*mock_launched_device_, OnUtilizationReport(expected_feedback_2));

  device_client_->OnIncomingCapturedBuffer(std::move(buffer2), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);

  // The frame should be delivered to the clients in any order.
  EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                              client_a_route_1, device_format.frame_size)));
  EXPECT_CALL(*client_b_, DoBufferReady(ControllerIDAndSize(
                              client_b_route_1, device_format.frame_size)));
  EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                              client_a_route_2, device_format.frame_size)));
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  Mock::VerifyAndClearExpectations(mock_launched_device_);

  // Add a fourth client now that some buffers have come through.
  controller_->AddClient(client_b_route_2, client_b_.get(),
                         base::UnguessableToken::Create(), session_params_4,
                         std::nullopt);
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Third, fourth, and fifth buffers. Pretend they all arrive at the same time.
  for (int i = 0; i < kPoolSize; i++) {
    const int arbitrary_frame_feedback_id_3 = 200 + i;
    media::VideoCaptureDevice::Client::Buffer buffer3;
    const auto result_code_3 = device_client_->ReserveOutputBuffer(
        device_format.frame_size, device_format.pixel_format,
        arbitrary_frame_feedback_id_3, &buffer3,
        /*require_new_buffer_id=*/nullptr,
        /*retire_old_buffer_id=*/nullptr);
    ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
              result_code_3);
    auto buffer3_access =
        buffer3.handle_provider->GetHandleForInProcessAccess();
    memset(buffer3_access->data(), buffer_no++, buffer3_access->mapped_size());
    device_client_->OnIncomingCapturedBuffer(
        std::move(buffer3), device_format, arbitrary_reference_time_,
        arbitrary_timestamp_, std::nullopt);
  }
  // ReserveOutputBuffer ought to fail now, because the pool is depleted.
  media::VideoCaptureDevice::Client::Buffer buffer_fail;
  ASSERT_EQ(
      media::VideoCaptureDevice::Client::ReserveResult::kMaxBufferCountExceeded,
      device_client_->ReserveOutputBuffer(
          device_format.frame_size, device_format.pixel_format,
          arbitrary_frame_feedback_id, &buffer_fail,
          /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr));

  // The new client needs to be notified of the creation of |kPoolSize| buffers;
  // the old clients only |kPoolSize - 1|.
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_2, _))
      .Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferReady(ControllerIDAndSize(
                              client_b_route_2, device_format.frame_size)))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1, _))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                              client_a_route_1, device_format.frame_size)))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2, _))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                              client_a_route_2, device_format.frame_size)))
      .Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1, _))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_b_, DoBufferReady(ControllerIDAndSize(
                              client_b_route_1, device_format.frame_size)))
      .Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Now test the interaction of client shutdown and buffer delivery.
  // Kill A1 via renderer disconnect (synchronous).
  controller_->RemoveClient(client_a_route_1, client_a_.get());
  // Kill B1 via session close (posts a task to disconnect).
  EXPECT_CALL(*client_b_, DoEnded(client_b_route_1)).Times(1);
  controller_->StopSession(session_id_3);
  // Queue up another buffer.
  media::VideoCaptureDevice::Client::Buffer buffer3;
  const auto result_code_3 = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id, &buffer3, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            result_code_3);
  auto buffer3_access = buffer3.handle_provider->GetHandleForInProcessAccess();
  memset(buffer3_access->data(), buffer_no++, buffer3_access->mapped_size());
  device_client_->OnIncomingCapturedBuffer(std::move(buffer3), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);

  media::VideoCaptureDevice::Client::Buffer buffer4;
  const auto result_code_4 = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id, &buffer4, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  {
    // Kill A2 via session close (posts a task to disconnect, but A2 must not
    // be sent either of these two buffers).
    EXPECT_CALL(*client_a_, DoEnded(client_a_route_2)).Times(1);
    controller_->StopSession(session_id_2);
  }
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            result_code_4);
  auto buffer4_access = buffer4.handle_provider->GetHandleForInProcessAccess();
  memset(buffer4_access->data(), buffer_no++, buffer4_access->mapped_size());
  device_client_->OnIncomingCapturedBuffer(std::move(buffer4), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);
  // B2 is the only client left, and is the only one that should
  // get the buffer.
  EXPECT_CALL(*client_b_, DoBufferReady(ControllerIDAndSize(
                              client_b_route_2, device_format.frame_size)))
      .Times(2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoCaptureControllerTest,
                         ::testing::Values(media::PIXEL_FORMAT_I420,
                                           media::PIXEL_FORMAT_Y16));

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorBeforeDeviceCreation) {
  media::VideoCaptureParams session_params_1;
  session_params_1.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_params_2 = session_params_1;

  const gfx::Size capture_resolution(320, 240);

  const VideoCaptureControllerID route_id(base::UnguessableToken::Create());

  const base::UnguessableToken session_id_1 = base::UnguessableToken::Create();
  const base::UnguessableToken session_id_2 = base::UnguessableToken::Create();
  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(), session_id_1,
                         session_params_1, std::nullopt);
  device_client_->OnError(
      media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest, FROM_HERE,
      "Test Error");
  EXPECT_CALL(
      *client_a_,
      DoError(route_id,
              media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest))
      .Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(
      *client_b_,
      DoError(route_id, media::VideoCaptureError::
                            kVideoCaptureControllerIsAlreadyInErrorState))
      .Times(1);
  controller_->AddClient(route_id, client_b_.get(), session_id_2,
                         session_params_2, std::nullopt);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_b_.get());

  media::VideoCaptureFormat device_format(
      capture_resolution, arbitrary_frame_rate_, media::PIXEL_FORMAT_I420);
  const int arbitrary_frame_feedback_id = 101;
  media::VideoCaptureDevice::Client::Buffer buffer;
  const auto reserve_result = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id, &buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            reserve_result);
  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);

  base::RunLoop().RunUntilIdle();
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorAfterDeviceCreation) {
  media::VideoCaptureParams session_params_1;
  session_params_1.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_params_2 = session_params_1;

  const VideoCaptureControllerID route_id(base::UnguessableToken::Create());

  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(),
                         base::UnguessableToken::Create(), session_params_1,
                         std::nullopt);

  // Start the device. Then, before the first buffer, signal an error and
  // deliver the buffer. The error should be propagated to clients; the buffer
  // should not be.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), arbitrary_frame_rate_, media::PIXEL_FORMAT_I420);
  const int arbitrary_frame_feedback_id = 101;
  media::VideoCaptureDevice::Client::Buffer buffer;
  const auto result_code = device_client_->ReserveOutputBuffer(
      device_format.frame_size, device_format.pixel_format,
      arbitrary_frame_feedback_id, &buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
            result_code);

  device_client_->OnError(
      media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest, FROM_HERE,
      "Test Error");
  device_client_->OnIncomingCapturedBuffer(std::move(buffer), device_format,
                                           arbitrary_reference_time_,
                                           arbitrary_timestamp_, std::nullopt);

  EXPECT_CALL(
      *client_a_,
      DoError(route_id,
              media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest))
      .Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(
      *client_b_,
      DoError(route_id, media::VideoCaptureError::
                            kVideoCaptureControllerIsAlreadyInErrorState))
      .Times(1);
  controller_->AddClient(route_id, client_b_.get(),
                         base::UnguessableToken::Create(), session_params_2,
                         std::nullopt);
  Mock::VerifyAndClearExpectations(client_b_.get());
}

// Tests that frame feedback provided by consumers is correctly reported back
// to the producing device for a sequence of frames that is longer than the
// number of buffers shared between the device and consumer.
TEST_F(VideoCaptureControllerTest, FrameFeedbackIsReportedForSequenceOfFrames) {
  const int kTestFrameSequenceLength = 10;
  media::VideoCaptureFormat arbitrary_format(
      gfx::Size(320, 240), arbitrary_frame_rate_, media::PIXEL_FORMAT_I420);
  // OnIncomingCapturedBuffer keeps the color space unset. If needed use
  // OnIncomingCapturedBufferExt.
  client_a_->expected_color_space_ = gfx::ColorSpace();

  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams session_params_1;
  session_params_1.requested_format = arbitrary_format;
  const VideoCaptureControllerID route_id = base::UnguessableToken::Create();
  controller_->AddClient(route_id, client_a_.get(),
                         base::UnguessableToken::Create(), session_params_1,
                         std::nullopt);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  for (int frame_index = 0; frame_index < kTestFrameSequenceLength;
       frame_index++) {
    const int stub_frame_feedback_id = frame_index;
    media::VideoCaptureFeedback stub_consumer_feedback =
        media::VideoCaptureFeedback(static_cast<float>(frame_index) /
                                    kTestFrameSequenceLength);
    stub_consumer_feedback.frame_id = stub_frame_feedback_id;

    client_a_->feedback_ = stub_consumer_feedback;

    EXPECT_CALL(*client_a_, DoBufferReady(ControllerIDAndSize(
                                route_id, arbitrary_format.frame_size)))
        .Times(1);
    EXPECT_CALL(*mock_launched_device_,
                OnUtilizationReport(stub_consumer_feedback))
        .Times(1);

    // Device prepares and pushes a frame.
    // The frame is expected to arrive at |client_a_|.DoBufferReady(), which
    // automatically notifies |controller_| that it has finished consuming it.
    media::VideoCaptureDevice::Client::Buffer buffer;
    const auto result_code = device_client_->ReserveOutputBuffer(
        arbitrary_format.frame_size, arbitrary_format.pixel_format,
        stub_frame_feedback_id, &buffer, /*require_new_buffer_id=*/nullptr,
        /*retire_old_buffer_id=*/nullptr);
    ASSERT_EQ(media::VideoCaptureDevice::Client::ReserveResult::kSucceeded,
              result_code);
    device_client_->OnIncomingCapturedBuffer(
        std::move(buffer), arbitrary_format, arbitrary_reference_time_,
        arbitrary_timestamp_, std::nullopt);

    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(client_a_.get());
    Mock::VerifyAndClearExpectations(mock_launched_device_);
  }
}

TEST_F(VideoCaptureControllerTest,
       DeviceClientIsReleasedBeforeAnyBufferWasShared) {
  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = arbitrary_format_;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // |device_client_| is released by the device.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(arbitrary_route_id_, _)).Times(0);
  device_client_.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
}

TEST_F(VideoCaptureControllerTest,
       DeviceClientIsReleasedAfterFrameHasBeenConsumed) {
  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = arbitrary_format_;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // Device sends a frame to |device_client_| and |client_a_| reports to
  // |controller_| that it has finished consuming the frame.
  int buffer_id_reported_to_client = media::VideoCaptureBufferPool::kInvalidId;
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(_, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&buffer_id_reported_to_client));
    EXPECT_CALL(*client_a_, DoBufferReady(_)).Times(1);
  }
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, _)).Times(0);
  SendStubFrameToDeviceClient(arbitrary_format_, arbitrary_color_space_);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |device_client_| is released by the device.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, buffer_id_reported_to_client));
  device_client_.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
}

TEST_F(VideoCaptureControllerTest,
       DeviceClientIsReleasedWhileFrameIsBeingConsumed) {
  client_a_->set_enable_auto_return_buffer_on_buffer_ready(false);
  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = arbitrary_format_;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // Device sends a frame to |device_client_|.
  int buffer_id_reported_to_client = media::VideoCaptureBufferPool::kInvalidId;
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(_, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&buffer_id_reported_to_client));
    EXPECT_CALL(*client_a_, DoBufferReady(_)).Times(1);
  }
  SendStubFrameToDeviceClient(arbitrary_format_, arbitrary_color_space_);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |device_client_| is released by the device.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, _)).Times(0);
  device_client_.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |client_a_| signals to |controller_| that it has finished consuming the
  // frame.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, _)).Times(1);
  const media::VideoCaptureFeedback arbitrary_feedback =
      media::VideoCaptureFeedback();
  controller_->ReturnBuffer(arbitrary_route_id_, client_a_.get(),
                            buffer_id_reported_to_client, arbitrary_feedback);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
}

TEST_F(VideoCaptureControllerTest,
       NewDeviceClientSendsNewBufferWhileRetiredBufferStillBeingConsumed) {
  client_a_->set_enable_auto_return_buffer_on_buffer_ready(false);
  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = arbitrary_format_;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // Device sends a frame to |device_client_|.
  int first_buffer_id = media::VideoCaptureBufferPool::kInvalidId;
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(_, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&first_buffer_id));
    EXPECT_CALL(*client_a_, DoBufferReady(_)).Times(1);
  }
  SendStubFrameToDeviceClient(arbitrary_format_, arbitrary_color_space_);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |device_client_| is released by the device.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, _)).Times(0);
  device_client_.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // A new |device_client_| is created with a new buffer pool.
  InitializeNewDeviceClientAndBufferPoolInstances();

  // Device sends a frame to the new |device_client_|.
  int second_buffer_id = media::VideoCaptureBufferPool::kInvalidId;
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(_, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&second_buffer_id));
    EXPECT_CALL(*client_a_, DoBufferReady(_)).Times(1);
  }
  SendStubFrameToDeviceClient(arbitrary_format_, arbitrary_color_space_);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  EXPECT_NE(first_buffer_id, second_buffer_id);

  // |client_a_| signals to |controller_| that it has finished consuming the
  // first frame.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, first_buffer_id)).Times(1);
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, second_buffer_id)).Times(0);
  const media::VideoCaptureFeedback arbitrary_feedback =
      media::VideoCaptureFeedback();
  controller_->ReturnBuffer(arbitrary_route_id_, client_a_.get(),
                            first_buffer_id, arbitrary_feedback);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |client_a_| signals to |controller_| that it has finished consuming the
  // second frame. Since the new |device_client_| is still alive, the second
  // buffer is expected to stay alive.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, first_buffer_id)).Times(0);
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, second_buffer_id)).Times(0);
  controller_->ReturnBuffer(arbitrary_route_id_, client_a_.get(),
                            second_buffer_id, arbitrary_feedback);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
}

// Tests that the VideoCaptureController reports OnStarted() to all clients,
// even if they connect after VideoCaptureController::OnStarted() has been
// invoked.
TEST_F(VideoCaptureControllerTest, OnStartedForMultipleClients) {
  media::VideoCaptureParams session_params_1;
  session_params_1.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureParams session_params_2 = session_params_1;
  media::VideoCaptureParams session_params_3 = session_params_1;

  const VideoCaptureControllerID client_a_route_1 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_a_route_2 =
      base::UnguessableToken::Create();
  const VideoCaptureControllerID client_b_route_1 =
      base::UnguessableToken::Create();

  controller_->AddClient(client_a_route_1, client_a_.get(),
                         base::UnguessableToken::Create(), session_params_1,
                         std::nullopt);
  controller_->AddClient(client_b_route_1, client_b_.get(),
                         base::UnguessableToken::Create(), session_params_3,
                         std::nullopt);
  ASSERT_EQ(2u, controller_->GetClientCount());

  {
    InSequence s;
    // Simulate the OnStarted event from device.
    EXPECT_CALL(*client_a_, OnStarted(_));
    EXPECT_CALL(*client_b_, OnStarted(_));
    device_client_->OnStarted();

    // VideoCaptureController will take care of the OnStarted event for the
    // clients who join later.
    EXPECT_CALL(*client_a_, OnStarted(_));
    controller_->AddClient(client_a_route_2, client_a_.get(),
                           base::UnguessableToken::Create(), session_params_2,
                           std::nullopt);
  }
}

TEST_F(VideoCaptureControllerTest, OnFrameDroppedIsForwarded) {
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = arbitrary_format_;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);

  EXPECT_CALL(*client_a_, OnFrameDropped(_, _)).Times(1);
  controller_->OnFrameDropped(
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded);
  Mock::VerifyAndClearExpectations(client_a_.get());
}

TEST_F(VideoCaptureControllerTest, DeviceClientWithColorSpace) {
  // Register |client_a_| at |controller_|.
  media::VideoCaptureParams requested_params;
  requested_params.requested_format = media::VideoCaptureFormat(
      gfx::Size(128, 80), 30, media::PIXEL_FORMAT_ARGB);
  const gfx::ColorSpace data_color_space =
      gfx::ColorSpace::CreateDisplayP3D65();
  const gfx::ColorSpace overriden_color_space =
      data_color_space.GetWithMatrixAndRange(
          gfx::ColorSpace::MatrixID::SMPTE170M,
          gfx::ColorSpace::RangeID::LIMITED);
  client_a_->expected_color_space_ = overriden_color_space;
  controller_->AddClient(arbitrary_route_id_, client_a_.get(),
                         arbitrary_session_id_, requested_params, std::nullopt);
  base::RunLoop().RunUntilIdle();

  // Device sends a frame to |device_client_| and |client_a_| reports to
  // |controller_| that it has finished consuming the frame.
  int buffer_id_reported_to_client = media::VideoCaptureBufferPool::kInvalidId;
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(_, _))
        .Times(1)
        .WillOnce(SaveArg<1>(&buffer_id_reported_to_client));
    EXPECT_CALL(*client_a_, DoBufferReady(_)).Times(1);
  }
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, _)).Times(0);
  SendStubFrameToDeviceClient(requested_params.requested_format,
                              data_color_space);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // |device_client_| is released by the device.
  EXPECT_CALL(*client_a_, DoBufferDestroyed(_, buffer_id_reported_to_client));
  device_client_.reset();
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
}

}  // namespace
}  // namespace content
