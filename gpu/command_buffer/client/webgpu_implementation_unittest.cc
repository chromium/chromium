// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for WebGPUImplementation.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <dawn/wire/client/webgpu_cpp.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
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
  static constexpr uint8_t kInitialValue = 0xBD;
  static constexpr uint32_t kNumCommandEntries = 500;
  static constexpr uint32_t kCommandBufferSizeBytes =
      kNumCommandEntries * sizeof(CommandBufferEntry);
  static constexpr uint32_t kTransferBufferSize = 512;

  static constexpr GLint kMaxCombinedTextureImageUnits = 8;
  static constexpr GLint kMaxTextureImageUnits = 8;
  static constexpr GLint kMaxTextureSize = 128;
  static constexpr GLint kNumCompressedTextureFormats = 0;

  WebGPUImplementationTest() {}

  bool Initialize() {
    SharedMemoryLimits limits = SharedMemoryLimitsForTesting();
    command_buffer_ = std::make_unique<StrictMock<MockClientCommandBuffer>>();

    transfer_buffer_ = std::make_unique<MockTransferBuffer>(
        command_buffer_.get(), kTransferBufferSize,
        ImplementationBase::kStartingOffset, ImplementationBase::kAlignment,
        false);

    helper_ = std::make_unique<WebGPUCmdHelper>(command_buffer_.get());
    helper_->Initialize(limits.command_buffer_size);
    gpu_control_ = std::make_unique<StrictMock<MockClientGpuControl>>();

    EXPECT_CALL(*gpu_control_, GetCapabilities())
        .WillOnce(ReturnRef(capabilities_));

    {
      InSequence sequence;

      gl_ = std::make_unique<WebGPUImplementation>(
          helper_.get(), transfer_buffer_.get(), gpu_control_.get());
    }

    // The client should be set to something non-null.
    EXPECT_CALL(*gpu_control_, SetGpuControlClient(gl_.get())).Times(1);

    if (gl_->Initialize(limits) != gpu::ContextResult::kSuccess)
      return false;

    helper_->CommandBufferHelper::Finish();
    Mock::VerifyAndClearExpectations(gl_.get());

    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    commands_ =
        UNSAFE_TODO(static_cast<CommandBufferEntry*>(ring_buffer->memory()) +
                    command_buffer_->GetServicePutOffset());
    ClearCommands();
    EXPECT_TRUE(transfer_buffer_->InSync());

    Mock::VerifyAndClearExpectations(command_buffer_.get());
    return true;
  }

  void ClearCommands() {
    scoped_refptr<Buffer> ring_buffer = helper_->get_ring_buffer();
    UNSAFE_TODO(
        memset(ring_buffer->memory(), kInitialValue, ring_buffer->size()));
  }

  void SetUp() override { ASSERT_TRUE(Initialize()); }

  void TearDown() override {
    if (!gl_) {
      return;
    }
    gl_->FlushCommands();
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

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MockClientCommandBuffer> command_buffer_;
  std::unique_ptr<MockClientGpuControl> gpu_control_;
  std::unique_ptr<WebGPUCmdHelper> helper_;
  std::unique_ptr<MockTransferBuffer> transfer_buffer_;
  std::unique_ptr<WebGPUImplementation> gl_;
  raw_ptr<CommandBufferEntry> commands_ = nullptr;
  Capabilities capabilities_;
};

#include "gpu/command_buffer/client/webgpu_implementation_unittest_autogen.h"

TEST_F(WebGPUImplementationTest,
       OnGpuControlLostContextDefersWireClientCallbacks) {
  base::RunLoop run_loop;
  bool callback_called = false;
  wgpu::Instance(gl_->GetAPIChannel()->GetWGPUInstance())
      .RequestAdapter(
          nullptr, wgpu::CallbackMode::AllowProcessEvents,
          [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
              wgpu::StringView) {
            EXPECT_EQ(status, wgpu::RequestAdapterStatus::CallbackCancelled);
            EXPECT_EQ(adapter, nullptr);
            callback_called = true;
            EXPECT_CALL(*gpu_control_, SetGpuControlClient(nullptr)).Times(1);
            gl_.reset();
            run_loop.Quit();
          });

  gl_->OnGpuControlLostContext();

  // Wire client callbacks must not run synchronously inside
  // `OnGpuControlLostContext()`, where `this` is still on the call stack.
  EXPECT_FALSE(callback_called);
  EXPECT_NE(gl_, nullptr);

  EXPECT_CALL(*command_buffer_, OnFlush()).Times(AnyNumber());
  EXPECT_CALL(*command_buffer_, DestroyTransferBuffer(_)).Times(AtLeast(1));
  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(gl_, nullptr);
}

}  // namespace webgpu
}  // namespace gpu
