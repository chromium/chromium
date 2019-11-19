// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for WebGPUImplementation.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/client/mock_transfer_buffer.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::AnyNumber;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Mock;
using testing::Sequence;
using testing::StrictMock;
using testing::Return;
using testing::ReturnRef;

namespace gpu {
namespace webgpu {

class WebGPUImplementationTest : public testing::Test {
 protected:
  static const uint8_t kInitialValue = 0xBD;
  static const uint32_t kNumCommandEntries = 500;
  static const uint32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static const uint32_t kTransferBufferSize = 512;

  static const GLint kMaxCombinedTextureImageUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxTextureSize = 128;
  static const GLint kNumCompressedTextureFormats = 0;

  WebGPUImplementationTest() {}

  bool Initialize() {
    SharedMemoryLimits limits = SharedMemoryLimitsForTesting();
    command_buffer_.reset(new StrictMock<MockClientCommandBuffer>());

    transfer_buffer_.reset(
        new MockTransferBuffer(command_buffer_.get(), kTransferBufferSize,
                               ImplementationBase::kStartingOffset,
                               ImplementationBase::kAlignment, false));

    helper_.reset(new WebGPUCmdHelper(command_buffer_.get()));
    helper_->Initialize(limits.command_buffer_size);
    gpu_control_.reset(new StrictMock<MockClientGpuControl>());

    EXPECT_CALL(*gpu_control_, GetCapabilities())
        .WillOnce(ReturnRef(capabilities_));

    {
      InSequence sequence;

      gl_.reset(new WebGPUImplementation(helper_.get(), transfer_buffer_.get(),
                                         gpu_control_.get()));
    }

    // The client should be set to something non-null.
    EXPECT_CALL(*gpu_control_, SetGpuControlClient(gl_.get())).Times(1);

    if (gl_->Initialize(limits) != gpu::ContextResult::kSuccess)
      return false;

    helper_->CommandBufferHelper::Finish();
    Mock::VerifyAndClearExpectations(gl_.get());

    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    commands_ = static_cast<CommandBufferEntry*>(ring_buffer->memory()) +
                command_buffer_->GetServicePutOffset();
    ClearCommands();
    EXPECT_TRUE(transfer_buffer_->InSync());

    Mock::VerifyAndClearExpectations(command_buffer_.get());
    return true;
  }

  void ClearCommands() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    memset(ring_buffer->memory(), kInitialValue, ring_buffer->size());
  }

  void SetUp() override { ASSERT_TRUE(Initialize()); }

  void TearDown() override {
    gl_->Flush();
    Mock::VerifyAndClear(gl_.get());
    EXPECT_CALL(*command_buffer_, OnFlush()).Times(AnyNumber());
    // For command buffer.
    EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_)).Times(AtLeast(1));
    // The client should be unset.
    EXPECT_CALL(*gpu_control_, SetGpuControlClient(nullptr)).Times(1);
    gl_.reset();
  }

  static SharedMemoryLimits SharedMemoryLimitsForTesting() {
    SharedMemoryLimits limits;
    limits.command_buffer_size = kCommandBufferSizeBytes;
    limits.start_transfer_buffer_size = kTransferBufferSize;
    limits.min_transfer_buffer_size = kTransferBufferSize;
    limits.max_transfer_buffer_size = kTransferBufferSize;
    limits.mapped_memory_reclaim_limit = SharedMemoryLimits::kNoLimit;
    return limits;
  }

  std::unique_ptr<MockClientCommandBuffer> command_buffer_;
  std::unique_ptr<MockClientGpuControl> gpu_control_;
  std::unique_ptr<WebGPUCmdHelper> helper_;
  std::unique_ptr<MockTransferBuffer> transfer_buffer_;
  std::unique_ptr<WebGPUImplementation> gl_;
  CommandBufferEntry* commands_ = nullptr;
  Capabilities capabilities_;
};

#include "base/macros.h"
#include "gpu/command_buffer/client/webgpu_implementation_unittest_autogen.h"

}  // namespace webgpu
}  // namespace gpu
