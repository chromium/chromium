// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/mock_command_buffer.h"
#include "gpu/ipc/common/mock_gpu_channel.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Matcher;
using ::testing::Return;

namespace gpu {
namespace {

// GpuChannelHost is expected to be created on the IO thread, and posts tasks to
// setup its IPC listener, so it must be created after the thread task runner
// handle is set.  It expects Send to be called on any thread except IO thread,
// and posts tasks to the IO thread to ensure IPCs are sent in order, which is
// important for sync IPCs.  But we override Send, so we can't test sync IPC
// behavior with this setup.
class TestGpuChannelHost : public GpuChannelHost {
 public:
  explicit TestGpuChannelHost(mojom::GpuChannel& gpu_channel)
      : GpuChannelHost(0 /* channel_id */,
                       GPUInfo(),
                       GpuFeatureInfo(),
                       SharedImageCapabilities(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  mojom::GpuChannel& GetGpuChannel() override { return *gpu_channel_; }

 protected:
  ~TestGpuChannelHost() override = default;

  const raw_ref<mojom::GpuChannel> gpu_channel_;
};

class MockGpuControlClient : public GpuControlClient {
 public:
  MockGpuControlClient() = default;
  virtual ~MockGpuControlClient() = default;

  MOCK_METHOD0(OnGpuControlLostContext, void());
  MOCK_METHOD0(OnGpuControlLostContextMaybeReentrant, void());
  MOCK_METHOD2(OnGpuControlErrorMessage, void(const char*, int32_t));
  MOCK_METHOD1(OnGpuSwitched, void(gl::GpuPreference));
  MOCK_METHOD1(OnGpuControlReturnData, void(base::span<const uint8_t>));
};

class CommandBufferProxyImplTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public testing::Test {
 public:
  CommandBufferProxyImplTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    (std::get<0>(GetParam()) ? enabled_features : disabled_features)
        .push_back(features::kConditionallySkipGpuChannelFlush);

    if (std::get<1>(GetParam())) {
      enabled_features.push_back(features::kSyncPointGraphValidation);
    } else {
      disabled_features.push_back(features::kSyncPointGraphValidation);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    skip_flush_if_possible_ = base::FeatureList::IsEnabled(
        features::kConditionallySkipGpuChannelFlush);
    channel_ = base::MakeRefCounted<TestGpuChannelHost>(mock_gpu_channel_);
  }

  ~CommandBufferProxyImplTest() override {
    // Release channel, and run any cleanup tasks it posts.
    channel_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<CommandBufferProxyImpl> CreateAndInitializeProxy(
      MockCommandBuffer* mock_command_buffer = nullptr) {
    auto proxy = std::make_unique<CommandBufferProxyImpl>(
        channel_, 0 /* stream_id */,
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // The Initialize() call below synchronously requests a new CommandBuffer
    // using the channel's GpuControl interface.  Simulate success, since we're
    // not actually talking to the service in these tests.
    EXPECT_CALL(mock_gpu_channel_, CreateCommandBuffer(_, _, _, _, _, _, _, _))
        .Times(1)
        .WillOnce(Invoke(
            [&](mojom::CreateCommandBufferParamsPtr params, int32_t routing_id,
                base::UnsafeSharedMemoryRegion shared_state,
                mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
                mojo::PendingAssociatedRemote<mojom::CommandBufferClient>
                    client,
                ContextResult* result, Capabilities* capabilities,
                GLCapabilities* gl_capabilities) -> bool {
              // There's no real GpuChannel pipe for this endpoint to use, so
              // give it its own dedicated pipe for these tests. This allows the
              // CommandBufferProxyImpl to make calls on its CommandBuffer
              // endpoint, which will send them to `mock_command_buffer` if
              // provided by the test.
              receiver.EnableUnassociatedUsage();
              clients_.push_back(std::move(client));
              if (mock_command_buffer)
                mock_command_buffer->Bind(std::move(receiver));
              *result = ContextResult::kSuccess;
              return true;
            }));

    proxy->Initialize(kNullSurfaceHandle, nullptr, SchedulingPriority::kNormal,
                      ContextCreationAttribs(), GURL());
    // Use an arbitrary valid shm_id. The command buffer doesn't use this
    // directly, but not setting it triggers DCHECKs.
    proxy->SetGetBuffer(1 /* shm_id */);
    return proxy;
  }

  void ExpectOrderingBarrier(const mojom::DeferredRequest& request,
                             int32_t route_id,
                             int32_t put_offset) {
    ASSERT_TRUE(request.params->is_command_buffer_request());

    const auto& command_buffer_request =
        *request.params->get_command_buffer_request();
    ASSERT_TRUE(command_buffer_request.params->is_async_flush());
    EXPECT_EQ(command_buffer_request.routing_id, route_id);

    const auto& flush_request =
        *command_buffer_request.params->get_async_flush();
    EXPECT_EQ(flush_request.put_offset, put_offset);
  }

  void ExpectFlush(int count) {
    if (skip_flush_if_possible_) {
      // Under kConditionallySkipGpuChannelFlush the first flush call will be
      // replaced by GetSharedMemoryForFlushId and later completely avoided
      // using shared memory. In unit tests proper shared memory channels are
      // never established so GetSharedMemory() is retried and fully replaces
      // Flush()
      EXPECT_CALL(mock_gpu_channel_,
                  GetSharedMemoryForFlushId(
                      Matcher<::base::ReadOnlySharedMemoryRegion*>(_)))
          .Times(count);
    } else {
      EXPECT_CALL(mock_gpu_channel_, Flush())
          .Times(count)
          .WillRepeatedly(Return(true));
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockGpuChannel mock_gpu_channel_;
  bool skip_flush_if_possible_ = false;
  scoped_refptr<TestGpuChannelHost> channel_;
  std::vector<mojo::PendingAssociatedRemote<mojom::CommandBufferClient>>
      clients_;
};

TEST_P(CommandBufferProxyImplTest, OrderingBarriersAreCoalescedWithFlush) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  EXPECT_CALL(mock_gpu_channel_, FlushDeferredRequests(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [&](std::vector<mojom::DeferredRequestPtr> requests, int32_t id) {
            EXPECT_EQ(3u, requests.size());
            ExpectOrderingBarrier(*requests[0], proxy1->route_id(), 10);
            ExpectOrderingBarrier(*requests[1], proxy2->route_id(), 20);
            ExpectOrderingBarrier(*requests[2], proxy1->route_id(), 50);
          }));

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);
  proxy1->OrderingBarrier(40);
  proxy1->Flush(50);

  // Once for each proxy.
  EXPECT_CALL(mock_gpu_channel_, DestroyCommandBuffer(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  if (!features::IsSyncPointGraphValidationEnabled()) {
    // Each proxy sends a sync GpuControl flush on disconnect.
    ExpectFlush(2);
  }
}

TEST_P(CommandBufferProxyImplTest, FlushPendingWorkFlushesOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  EXPECT_CALL(mock_gpu_channel_, FlushDeferredRequests(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [&](std::vector<mojom::DeferredRequestPtr> requests, int32_t id) {
            EXPECT_EQ(3u, requests.size());
            ExpectOrderingBarrier(*requests[0], proxy1->route_id(), 10);
            ExpectOrderingBarrier(*requests[1], proxy2->route_id(), 20);
            ExpectOrderingBarrier(*requests[2], proxy1->route_id(), 30);
          }));

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);
  proxy2->FlushPendingWork();

  // Once for each proxy.
  EXPECT_CALL(mock_gpu_channel_, DestroyCommandBuffer(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  if (!features::IsSyncPointGraphValidationEnabled()) {
    // Each proxy sends a sync GpuControl flush on disconnect.
    ExpectFlush(2);
  }
}

TEST_P(CommandBufferProxyImplTest, EnsureWorkVisibleFlushesOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();
  auto proxy2 = CreateAndInitializeProxy();

  // Ordering of these flush operations must be preserved.
  {
    ::testing::InSequence in_sequence;

    // First we expect to see a FlushDeferredRequests call.
    EXPECT_CALL(mock_gpu_channel_, FlushDeferredRequests(_, _))
        .Times(1)
        .WillOnce(Invoke(
            [&](std::vector<mojom::DeferredRequestPtr> requests, int32_t id) {
              EXPECT_EQ(3u, requests.size());
              ExpectOrderingBarrier(*requests[0], proxy1->route_id(), 10);
              ExpectOrderingBarrier(*requests[1], proxy2->route_id(), 20);
              ExpectOrderingBarrier(*requests[2], proxy1->route_id(), 30);
            }));

    if (!features::IsSyncPointGraphValidationEnabled()) {
      // Next we expect a full `Flush()`.
      ExpectFlush(1);
    }
  }

  proxy1->OrderingBarrier(10);
  proxy2->OrderingBarrier(20);
  proxy1->OrderingBarrier(30);

  proxy2->EnsureWorkVisible();

  // Once for each proxy.
  EXPECT_CALL(mock_gpu_channel_, DestroyCommandBuffer(_))
      .Times(2)
      .WillRepeatedly(Return(true));

  if (!features::IsSyncPointGraphValidationEnabled()) {
    // Each proxy sends a sync GpuControl flush on disconnect.
    ExpectFlush(2);
  }
}

TEST_P(CommandBufferProxyImplTest,
       EnqueueDeferredMessageEnqueuesPendingOrderingBarriers) {
  auto proxy1 = CreateAndInitializeProxy();

  proxy1->OrderingBarrier(10);
  proxy1->OrderingBarrier(20);
  channel_->EnqueueDeferredMessage(
      mojom::DeferredRequestParams::NewCommandBufferRequest(
          mojom::DeferredCommandBufferRequest::New(
              proxy1->route_id(), mojom::DeferredCommandBufferRequestParams::
                                      NewDestroyTransferBuffer(3))),
      /*sync_token_fences=*/{}, /*release_count=*/0);

  // Make sure the above requests don't hit our mock yet.
  base::RunLoop().RunUntilIdle();

  // Now we can expect a FlushDeferredRequests to be elicited by the
  // FlushPendingWork call below.
  EXPECT_CALL(mock_gpu_channel_, FlushDeferredRequests(_, _))
      .Times(1)
      .WillOnce(Invoke(
          [&](std::vector<mojom::DeferredRequestPtr> requests, int32_t id) {
            EXPECT_EQ(2u, requests.size());
            ExpectOrderingBarrier(*requests[0], proxy1->route_id(), 20);
            ASSERT_TRUE(requests[1]->params->is_command_buffer_request());

            auto& request = *requests[1]->params->get_command_buffer_request();
            ASSERT_TRUE(request.params->is_destroy_transfer_buffer());
            EXPECT_EQ(3, request.params->get_destroy_transfer_buffer());
          }));

  proxy1->FlushPendingWork();

  EXPECT_CALL(mock_gpu_channel_, DestroyCommandBuffer(_))
      .Times(1)
      .WillOnce(Return(true));

  if (!features::IsSyncPointGraphValidationEnabled()) {
    // The proxy sends a sync GpuControl flush on disconnect.
    ExpectFlush(1);
  }
}

TEST_P(CommandBufferProxyImplTest, CreateTransferBufferOOM) {
  auto gpu_control_client = std::unique_ptr<MockGpuControlClient>(
      new testing::StrictMock<MockGpuControlClient>());

  auto proxy = CreateAndInitializeProxy();
  proxy->SetGpuControlClient(gpu_control_client.get());

  // This is called once when the CommandBufferProxyImpl is destroyed.
  EXPECT_CALL(*gpu_control_client, OnGpuControlLostContext())
      .Times(1)
      .RetiresOnSaturation();

  // Passing kReturnNullOnOOM should not cause the context to be lost
  EXPECT_CALL(*gpu_control_client, OnGpuControlLostContextMaybeReentrant())
      .Times(0);

  int32_t id = -1;
  scoped_refptr<gpu::Buffer> transfer_buffer_oom = proxy->CreateTransferBuffer(
      std::numeric_limits<uint32_t>::max(), &id, 0,
      TransferBufferAllocationOption::kReturnNullOnOOM);
  if (transfer_buffer_oom) {
    // In this test, there's no guarantee allocating UINT32_MAX will definitely
    // fail, but it is likely to OOM. If it didn't fail, return immediately.
    // TODO(enga): Consider manually injecting an allocation failure to test this
    // better.
    return;
  }

  EXPECT_EQ(id, -1);

  // Make a smaller buffer which should work.
  scoped_refptr<gpu::Buffer> transfer_buffer =
      proxy->CreateTransferBuffer(16, &id);

  EXPECT_NE(transfer_buffer, nullptr);
  EXPECT_NE(id, -1);

  // Now, allocating with kLoseContextOnOOM should cause the context to be lost.
  EXPECT_CALL(*gpu_control_client, OnGpuControlLostContextMaybeReentrant())
      .Times(1)
      .RetiresOnSaturation();

  transfer_buffer_oom = proxy->CreateTransferBuffer(
      std::numeric_limits<uint32_t>::max(), &id, 0,
      TransferBufferAllocationOption::kLoseContextOnOOM);

  EXPECT_CALL(mock_gpu_channel_, DestroyCommandBuffer(_))
      .Times(1)
      .WillOnce(Return(true));

  if (!features::IsSyncPointGraphValidationEnabled()) {
    // The proxy sends a sync GpuControl flush on disconnect.
    ExpectFlush(1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CommandBufferProxyImplTest,
                         testing::Combine(testing::Values(false, true),
                                          testing::Values(false, true)));
}  // namespace
}  // namespace gpu
