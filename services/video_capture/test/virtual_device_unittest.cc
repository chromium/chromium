// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/shared_memory_virtual_device_mojo_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace video_capture {

namespace {

const std::string kTestDeviceId = "/test/device";
const std::string kTestDeviceName = "Test Device";
const gfx::Size kTestFrameSize = {640 /* width */, 480 /* height */};
const media::VideoPixelFormat kTestPixelFormat =
    media::VideoPixelFormat::PIXEL_FORMAT_I420;

}  // anonymous namespace

class VirtualDeviceTest : public ::testing::Test {
 public:
  VirtualDeviceTest() = default;
  ~VirtualDeviceTest() override = default;

  void SetUp() override {
    device_info_.descriptor.device_id = kTestDeviceId;
    device_info_.descriptor.set_display_name(kTestDeviceName);
    mojo::Remote<mojom::Producer> producer;
    producer_ =
        std::make_unique<MockProducer>(producer.BindNewPipeAndPassReceiver());
    device_adapter_ = std::make_unique<SharedMemoryVirtualDeviceMojoAdapter>(
        std::move(producer));
  }

  void OnFrameBufferReceived(bool valid_buffer_expected, int32_t buffer_id) {
    if (!valid_buffer_expected) {
      EXPECT_EQ(-1, buffer_id);
      return;
    }

    EXPECT_NE(-1, buffer_id);
    received_buffer_ids_.push_back(buffer_id);
  }

  void VerifyAndGetMaxFrameBuffers() {
    base::RunLoop wait_loop;
    EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _))
        .Times(SharedMemoryVirtualDeviceMojoAdapter::
                   max_buffer_pool_buffer_count())
        .WillRepeatedly(Invoke(
            [](int32_t buffer_id, media::mojom::VideoBufferHandlePtr* handle,
               mojom::Producer::OnNewBufferCallback& callback) {
              std::move(callback).Run();
            }));
    // Should receive valid buffer for up to the maximum buffer count.
    for (int i = 0;
         i <
         SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count();
         i++) {
      device_adapter_->RequestFrameBuffer(
          kTestFrameSize, kTestPixelFormat, nullptr,
          base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                         base::Unretained(this),
                         true /* valid_buffer_expected */));
    }

    // No more buffer available. Invalid buffer id should be returned.
    device_adapter_->RequestFrameBuffer(
        kTestFrameSize, kTestPixelFormat, nullptr,
        base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                       base::Unretained(this),
                       false /* valid_buffer_expected */));

    wait_loop.RunUntilIdle();
    Mock::VerifyAndClearExpectations(producer_.get());
    EXPECT_EQ(
        SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count(),
        static_cast<int>(received_buffer_ids_.size()));
  }

 protected:
  std::unique_ptr<SharedMemoryVirtualDeviceMojoAdapter> device_adapter_;
  // ID of buffers received and owned by the producer.
  std::vector<int> received_buffer_ids_;
  std::unique_ptr<MockProducer> producer_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  media::VideoCaptureDeviceInfo device_info_;
};

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithoutReceiver) {
  // Obtain maximum number of buffers.
  VerifyAndGetMaxFrameBuffers();

  base::RunLoop wait_loop;

  // Release one buffer back to the pool, no consumer hold since there is no
  // receiver.
  device_adapter_->OnFrameReadyInBuffer(received_buffer_ids_.at(0),
                                        media::mojom::VideoFrameInfo::New());

  // Verify there is a buffer available now, without creating a new
  // buffer.
  EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _)).Times(0);
  device_adapter_->RequestFrameBuffer(
      kTestFrameSize, kTestPixelFormat, nullptr,
      base::BindOnce(&VirtualDeviceTest::OnFrameBufferReceived,
                     base::Unretained(this), true /* valid_buffer_expected */));

  wait_loop.RunUntilIdle();
}

TEST_F(VirtualDeviceTest, OnFrameReadyInBufferWithReceiver) {
  // Obtain maximum number of buffers for the producer.
  VerifyAndGetMaxFrameBuffers();

  // Release all buffers back to consumer, then back to the pool
  // after |Receiver::OnFrameReadyInBuffer| is invoked.
  base::RunLoop wait_loop;
  mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote;
  MockVideoFrameHandler video_frame_handler(
      handler_remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(video_frame_handler, OnStarted());
  EXPECT_CALL(video_frame_handler, DoOnNewBuffer(_, _))
      .Times(
          SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  EXPECT_CALL(video_frame_handler, DoOnFrameReadyInBuffer(_, _, _, _))
      .Times(
          SharedMemoryVirtualDeviceMojoAdapter::max_buffer_pool_buffer_count());
  device_adapter_->Start(media::VideoCaptureParams(),
                         std::move(handler_remote));
  for (auto buffer_id : received_buffer_ids_) {
    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->metadata = base::Value(base::Value::Type::DICTIONARY);
    device_adapter_->OnFrameReadyInBuffer(buffer_id, std::move(info));
  }
  wait_loop.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&video_frame_handler);

  // Verify that requesting a buffer doesn't create a new one, will reuse
  // the available buffer in the pool.
  base::RunLoop wait_loop2;
  EXPECT_CALL(*producer_, DoOnNewBuffer(_, _, _)).Times(0);
  base::MockCallback<
      mojom::SharedMemoryVirtualDevice::RequestFrameBufferCallback>
      request_frame_buffer_callback;
  EXPECT_CALL(request_frame_buffer_callback, Run(_))
      .Times(1)
      .WillOnce(Invoke([this](int32_t buffer_id) {
        // Verify that the returned |buffer_id| is a known buffer ID.
        EXPECT_TRUE(base::Contains(received_buffer_ids_, buffer_id));
      }));
  device_adapter_->RequestFrameBuffer(kTestFrameSize, kTestPixelFormat, nullptr,
                                      request_frame_buffer_callback.Get());
  wait_loop2.RunUntilIdle();

  // Verify that when stopping the device, the receiver receives calls to
  // OnBufferRetired() followed by a single call to OnStopped().
  base::RunLoop wait_for_stopped_loop;
  {
    testing::InSequence s;
    EXPECT_CALL(video_frame_handler, DoOnBufferRetired(_))
        .Times(SharedMemoryVirtualDeviceMojoAdapter::
                   max_buffer_pool_buffer_count());
    EXPECT_CALL(video_frame_handler, OnStopped())
        .WillOnce(Invoke(
            [&wait_for_stopped_loop]() { wait_for_stopped_loop.Quit(); }));
  }
  device_adapter_->Stop();
  wait_for_stopped_loop.Run();
}

}  // namespace video_capture
