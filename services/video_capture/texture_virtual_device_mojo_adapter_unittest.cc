// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/texture_virtual_device_mojo_adapter.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::_;

namespace video_capture {

class TextureVirtualDeviceMojoAdapterTest : public ::testing::Test {
 public:
  TextureVirtualDeviceMojoAdapterTest() = default;

  void SetUp() override {
    mock_video_frame_handler_1_ = std::make_unique<MockVideoFrameHandler>(
        video_frame_handler_1_.InitWithNewPipeAndPassReceiver());
    mock_video_frame_handler_2_ = std::make_unique<MockVideoFrameHandler>(
        video_frame_handler_2_.InitWithNewPipeAndPassReceiver());
    adapter_ = std::make_unique<TextureVirtualDeviceMojoAdapter>();
  }

 protected:
  void ProducerSharesBufferHandle(int32_t buffer_id) {
    auto shared_image = gpu::ClientSharedImage::CreateForTesting();
    auto dummy_buffer_handle = media::mojom::SharedImageBufferHandleSet::New(
        shared_image->Export(), gpu::SyncToken());
    adapter_->OnNewSharedImageBufferHandle(buffer_id,
                                           std::move(dummy_buffer_handle));
  }

  void ProducerRetiresBufferHandle(int32_t buffer_id) {
    adapter_->OnBufferRetired(buffer_id);
  }

  void Receiver1Connects() {
    const media::VideoCaptureParams kArbitraryRequestedSettings;
    adapter_->Start(kArbitraryRequestedSettings,
                    std::move(video_frame_handler_1_));
  }

  void Receiver2Connects() {
    const media::VideoCaptureParams kArbitraryRequestedSettings;
    adapter_->Start(kArbitraryRequestedSettings,
                    std::move(video_frame_handler_2_));
  }

  void Receiver1Disconnects() {
    base::RunLoop wait_loop;
    adapter_->SetReceiverDisconnectedCallback(wait_loop.QuitClosure());
    mock_video_frame_handler_1_.reset();
    wait_loop.Run();
  }

  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_1_;
  std::unique_ptr<MockVideoFrameHandler> mock_video_frame_handler_2_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TextureVirtualDeviceMojoAdapter> adapter_;
  mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_1_;
  mojo::PendingRemote<mojom::VideoFrameHandler> video_frame_handler_2_;
};

// Tests that when buffer handles are shared by the producer before a receiver
// has connected, these buffer handles get shared with the receiver as soon as
// it connects.
TEST_F(TextureVirtualDeviceMojoAdapterTest,
       BufferHandlesAreSharedWithReceiverConnectingLate) {
  const int kArbitraryBufferId1 = 1;
  const int kArbitraryBufferId2 = 2;
  ProducerSharesBufferHandle(kArbitraryBufferId1);
  ProducerSharesBufferHandle(kArbitraryBufferId2);

  base::RunLoop wait_loop;
  int buffer_received_count = 0;
  EXPECT_CALL(*mock_video_frame_handler_1_,
              DoOnNewBuffer(kArbitraryBufferId1, _))
      .WillOnce(InvokeWithoutArgs([&wait_loop, &buffer_received_count]() {
        buffer_received_count++;
        if (buffer_received_count == 2)
          wait_loop.Quit();
      }));
  EXPECT_CALL(*mock_video_frame_handler_1_,
              DoOnNewBuffer(kArbitraryBufferId2, _))
      .WillOnce(InvokeWithoutArgs([&wait_loop, &buffer_received_count]() {
        buffer_received_count++;
        if (buffer_received_count == 2)
          wait_loop.Quit();
      }));
  Receiver1Connects();
  wait_loop.Run();
}

// Tests that when a receiver disconnects and a new receiver connects, the
// virtual device adapter shares all valid buffer handles with it.
TEST_F(TextureVirtualDeviceMojoAdapterTest,
       BufferHandlesAreSharedWithSecondReceiver) {
  const int kArbitraryBufferId1 = 1;
  const int kArbitraryBufferId2 = 2;

  Receiver1Connects();
  ProducerSharesBufferHandle(kArbitraryBufferId1);
  ProducerSharesBufferHandle(kArbitraryBufferId2);
  Receiver1Disconnects();

  ProducerRetiresBufferHandle(kArbitraryBufferId1);

  base::RunLoop wait_loop;
  EXPECT_CALL(*mock_video_frame_handler_2_,
              DoOnNewBuffer(kArbitraryBufferId2, _))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  Receiver2Connects();
  wait_loop.Run();
}

}  // namespace video_capture
