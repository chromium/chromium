// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_gpu_video_accelerator_factories.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "content/public/common/gpu_stream_constants.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  explicit TestGpuChannelHost(gpu::mojom::GpuChannel& gpu_channel)
      : GpuChannelHost(/*channel_id=*/0,
                       viz::mojom::GpuClientType::kBrowser,
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  gpu::mojom::GpuChannel& GetGpuChannel() override { return *gpu_channel_; }

 protected:
  ~TestGpuChannelHost() override = default;

 private:
  const raw_ref<gpu::mojom::GpuChannel> gpu_channel_;
};

class MockContextProviderCommandBuffer
    : public viz::ContextProviderCommandBuffer {
 public:
  explicit MockContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel)
      : viz::ContextProviderCommandBuffer(std::move(channel)) {}

  MOCK_METHOD(gpu::CommandBufferProxyImpl*,
              GetCommandBufferProxy,
              (),
              (override));

 protected:
  ~MockContextProviderCommandBuffer() override = default;
};

}  // namespace

class BrowserGpuVideoAcceleratorFactoriesTest : public testing::Test {
 public:
  BrowserGpuVideoAcceleratorFactoriesTest()
      : gpu_channel_host_(
            base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_)),
        context_provider_(
            base::MakeRefCounted<NiceMock<MockContextProviderCommandBuffer>>(
                gpu_channel_host_)) {}

  void SetUp() override {
    ON_CALL(mock_gpu_channel_, CreateCommandBuffer(_, _, _, _, _, _, _, _))
        .WillByDefault(
            [](gpu::mojom::CreateCommandBufferParamsPtr params,
               int32_t routing_id, base::UnsafeSharedMemoryRegion shared_state,
               mojo::PendingAssociatedReceiver<gpu::mojom::CommandBuffer>
                   receiver,
               mojo::PendingAssociatedRemote<gpu::mojom::CommandBufferClient>
                   client,
               gpu::ContextResult* result, gpu::Capabilities* capabilities,
               gpu::GLCapabilities* gl_capabilities) -> bool {
              receiver.EnableUnassociatedUsage();
              *result = gpu::ContextResult::kSuccess;
              return true;
            });

    command_buffer_proxy_ = std::make_unique<gpu::CommandBufferProxyImpl>(
        gpu_channel_host_, kGpuStreamIdDefault,
        task_environment_.GetMainThreadTaskRunner());
    command_buffer_proxy_->Initialize(
        kGpuStreamPriorityDefault,
        gpu::mojom::ContextCreationAttribs::NewGles(
            gpu::mojom::GLESCreationAttribs::New()),
        GURL());
    ON_CALL(*context_provider_, GetCommandBufferProxy())
        .WillByDefault(Return(command_buffer_proxy_.get()));
  }

  void TearDown() override {
    command_buffer_proxy_.reset();
    context_provider_.reset();
    gpu_channel_host_.reset();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NiceMock<gpu::MockGpuChannel> mock_gpu_channel_;
  scoped_refptr<TestGpuChannelHost> gpu_channel_host_;
  scoped_refptr<NiceMock<MockContextProviderCommandBuffer>> context_provider_;
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_proxy_;
};

// The GpuChannel reply may arrive after the factories are destroyed, for
// example when ThumbnailMediaParserImpl times out while the token request is
// pending.
TEST_F(BrowserGpuVideoAcceleratorFactoriesTest,
       ChannelTokenReplyAfterDestruction) {
  gpu::MockGpuChannel::GetChannelTokenCallback pending_reply;
  EXPECT_CALL(mock_gpu_channel_, GetChannelToken(_))
      .WillOnce([&](gpu::MockGpuChannel::GetChannelTokenCallback callback) {
        pending_reply = std::move(callback);
      });

  auto factories =
      std::make_unique<BrowserGpuVideoAcceleratorFactories>(context_provider_);
  static_cast<media::GpuVideoAcceleratorFactories*>(factories.get())
      ->GetChannelToken(
          base::BindOnce([](const base::UnguessableToken&) { FAIL(); }));
  ASSERT_TRUE(pending_reply);

  factories.reset();
  std::move(pending_reply).Run(base::UnguessableToken::Create());
}

}  // namespace content
