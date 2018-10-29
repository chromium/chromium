// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/texture_virtual_device_mojo_adapter.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::_;

namespace video_capture {

class TextureVirtualDeviceMojoAdapterTest : public ::testing::Test {
 public:
  TextureVirtualDeviceMojoAdapterTest() : ref_factory_(base::DoNothing()) {}

  void SetUp() override {
    mock_receiver_1_ =
        std::make_unique<MockReceiver>(mojo::MakeRequest(&receiver_1_));
    mock_receiver_2_ =
        std::make_unique<MockReceiver>(mojo::MakeRequest(&receiver_2_));
    adapter_ = std::make_unique<TextureVirtualDeviceMojoAdapter>(
        ref_factory_.CreateRef());
  }

 protected:
  void ProducerSharesBufferHandle(int32_t buffer_id) {
    auto dummy_buffer_handle = media::mojom::MailboxBufferHandleSet::New();
    dummy_buffer_handle->mailbox_holder.resize(media::VideoFrame::kMaxPlanes);
    adapter_->OnNewMailboxHolderBufferHandle(buffer_id,
                                             std::move(dummy_buffer_handle));
  }

  void ProducerRetiresBufferHandle(int32_t buffer_id) {
    adapter_->OnBufferRetired(buffer_id);
  }

  void Receiver1Connects() {
    const media::VideoCaptureParams kArbitraryRequestedSettings;
    adapter_->Start(kArbitraryRequestedSettings, std::move(receiver_1_));
  }

  void Receiver2Connects() {
    const media::VideoCaptureParams kArbitraryRequestedSettings;
    adapter_->Start(kArbitraryRequestedSettings, std::move(receiver_2_));
  }

  void Receiver1Disconnects() {
    base::RunLoop wait_loop;
    adapter_->SetReceiverDisconnectedCallback(wait_loop.QuitClosure());
    mock_receiver_1_.reset();
    wait_loop.Run();
  }

  std::unique_ptr<MockReceiver> mock_receiver_1_;
  std::unique_ptr<MockReceiver> mock_receiver_2_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::ServiceContextRefFactory ref_factory_;
  std::unique_ptr<TextureVirtualDeviceMojoAdapter> adapter_;
  mojom::ReceiverPtr receiver_1_;
  mojom::ReceiverPtr receiver_2_;
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
  EXPECT_CALL(*mock_receiver_1_, DoOnNewBuffer(kArbitraryBufferId1, _))
      .WillOnce(InvokeWithoutArgs([&wait_loop, &buffer_received_count]() {
        buffer_received_count++;
        if (buffer_received_count == 2)
          wait_loop.Quit();
      }));
  EXPECT_CALL(*mock_receiver_1_, DoOnNewBuffer(kArbitraryBufferId2, _))
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
  EXPECT_CALL(*mock_receiver_2_, DoOnNewBuffer(kArbitraryBufferId2, _))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  Receiver2Connects();
  wait_loop.Run();
}

}  // namespace video_capture
