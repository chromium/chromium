// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes for implementing gpu client side unit tests.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class FakeCommandBufferServiceBase : public CommandBufferServiceBase {
 public:
  static const int32_t kTransferBufferBaseId = 0x123;
  static const int32_t kMaxTransferBuffers = 32;

  FakeCommandBufferServiceBase();
  ~FakeCommandBufferServiceBase() override;

  CommandBuffer::State GetState() override;
  void SetReleaseCount(uint64_t release_count) override;
  scoped_refptr<gpu::Buffer> GetTransferBuffer(int32_t id) override;
  void SetToken(int32_t token) override;
  void SetParseError(error::Error error) override;
  void SetContextLostReason(error::ContextLostReason reason) override;

  // Get's the Id of the next transfer buffer that will be returned
  // by CreateTransferBuffer. This is useful for testing expected ids.
  int32_t GetNextFreeTransferBufferId();

  void FlushHelper(int32_t put_offset);
  void SetGetBufferHelper(int transfer_buffer_id, int32_t token);
  scoped_refptr<gpu::Buffer> CreateTransferBufferHelper(uint32_t size,
                                                        int32_t* id);
  void DestroyTransferBufferHelper(int32_t id);

 private:
  scoped_refptr<Buffer> transfer_buffer_buffers_[kMaxTransferBuffers];
  CommandBuffer::State state_;
};

class MockClientCommandBuffer : public CommandBuffer,
                                public FakeCommandBufferServiceBase {
 public:
  MockClientCommandBuffer();
  ~MockClientCommandBuffer() override;

  State GetLastState() override;
  State WaitForTokenInRange(int32_t start, int32_t end) override;
  State WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                int32_t start,
                                int32_t end) override;
  void SetGetBuffer(int transfer_buffer_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(
      uint32_t size,
      int32_t* id,
      uint32_t alignment = 0,
      TransferBufferAllocationOption option =
          TransferBufferAllocationOption::kLoseContextOnOOM) override;

  // This is so we can use all the gmock functions when Flush is called.
  MOCK_METHOD0(OnFlush, void());
  MOCK_METHOD1(DestroyTransferBuffer, void(int32_t id));

  void Flush(int32_t put_offset) override;
  void OrderingBarrier(int32_t put_offset) override;

  void DelegateToFake();

  int32_t GetServicePutOffset() { return put_offset_; }

  void SetTokenForSetGetBuffer(int32_t token) { token_ = token; }

  void ForceLostContext(error::ContextLostReason reason) override;

 private:
  int32_t put_offset_ = 0;
  int32_t token_ = 10000;  // All token checks in the tests should pass.
};

class MockClientCommandBufferMockFlush : public MockClientCommandBuffer {
 public:
  MockClientCommandBufferMockFlush();
  ~MockClientCommandBufferMockFlush() override;

  MOCK_METHOD1(Flush, void(int32_t put_offset));
  MOCK_METHOD1(OrderingBarrier, void(int32_t put_offset));

  void DelegateToFake();
  void DoFlush(int32_t put_offset);
};

class MockClientGpuControl : public GpuControl {
 public:
  MockClientGpuControl();

  MockClientGpuControl(const MockClientGpuControl&) = delete;
  MockClientGpuControl& operator=(const MockClientGpuControl&) = delete;

  ~MockClientGpuControl() override;

  MOCK_METHOD1(SetGpuControlClient, void(GpuControlClient*));
  MOCK_CONST_METHOD0(GetCapabilities, const Capabilities&());
  MOCK_CONST_METHOD0(GetGLCapabilities, const GLCapabilities&());
  MOCK_METHOD3(CreateImage,
               int32_t(ClientBuffer buffer, size_t width, size_t height));
  MOCK_METHOD1(DestroyImage, void(int32_t id));

  // Workaround for move-only args in GMock.
  MOCK_METHOD2(DoSignalQuery,
               void(uint32_t query, base::OnceClosure* callback));
  void SignalQuery(uint32_t query, base::OnceClosure callback) override {
    DoSignalQuery(query, &callback);
  }
  MOCK_METHOD0(CancelAllQueries, void());

  MOCK_METHOD1(CreateStreamTexture, uint32_t(uint32_t));
  MOCK_METHOD1(SetLock, void(base::Lock*));
  MOCK_METHOD0(EnsureWorkVisible, void());
  MOCK_CONST_METHOD0(GetNamespaceID, CommandBufferNamespace());
  MOCK_CONST_METHOD0(GetCommandBufferID, CommandBufferId());
  MOCK_METHOD0(FlushPendingWork, void());
  MOCK_METHOD0(GenerateFenceSyncRelease, uint64_t());
  MOCK_METHOD1(IsFenceSyncReleased, bool(uint64_t release));

  // Workaround for move-only args in GMock.
  MOCK_METHOD2(DoSignalSyncToken,
               void(const SyncToken& sync_token, base::OnceClosure* callback));
  void SignalSyncToken(const SyncToken& sync_token,
                       base::OnceClosure callback) override {
    DoSignalSyncToken(sync_token, &callback);
  }

  MOCK_METHOD1(WaitSyncToken, void(const SyncToken&));
  MOCK_METHOD1(CanWaitUnverifiedSyncToken, bool(const SyncToken&));
  MOCK_METHOD2(CreateGpuFence,
               void(uint32_t gpu_fence_id, ClientGpuFence source));
  // OnceCallback isn't mockable?
  void GetGpuFence(uint32_t gpu_fence_id,
                   base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)>
                       callback) override {}
  MOCK_METHOD1(SetDisplayTransform, void(gfx::OverlayTransform));
};

class FakeDecoderClient : public DecoderClient {
 public:
  ~FakeDecoderClient() override;
  void OnConsoleMessage(int32_t id, const std::string& message) override;
  void CacheBlob(gpu::GpuDiskCacheType type,
                 const std::string& key,
                 const std::string& shader) override;
  void OnFenceSyncRelease(uint64_t release) override;
  void OnDescheduleUntilFinished() override;
  void OnRescheduleAfterFinished() override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void ScheduleGrContextCleanup() override;
  void SetActiveURL(GURL url) override;
  void HandleReturnData(base::span<const uint8_t> data) override;
  bool ShouldYield() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_TEST_HELPER_H_
