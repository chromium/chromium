// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
#define GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_

#include <cstdint>

#include "build/build_config.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockGpuChannel : public mojom::GpuChannel {
 public:
  MockGpuChannel();
  ~MockGpuChannel() override;

  // mojom::GpuChannel:
  MOCK_METHOD0(CrashForTesting, void());
  MOCK_METHOD0(TerminateForTesting, void());
  MOCK_METHOD1(GetChannelToken, void(GetChannelTokenCallback));
  MOCK_METHOD0(Flush, bool());

  MOCK_METHOD1(GetSharedMemoryForFlushId,
               bool(::base::ReadOnlySharedMemoryRegion*));
  MOCK_METHOD1(GetSharedMemoryForFlushId,
               void(GetSharedMemoryForFlushIdCallback));

  MOCK_METHOD1(Flush, void(FlushCallback));
  MOCK_METHOD6(CreateCommandBuffer,
               void(mojom::CreateCommandBufferParamsPtr,
                    int32_t,
                    base::UnsafeSharedMemoryRegion,
                    mojo::PendingAssociatedReceiver<mojom::CommandBuffer>,
                    mojo::PendingAssociatedRemote<mojom::CommandBufferClient>,
                    CreateCommandBufferCallback));
  MOCK_METHOD8(CreateCommandBuffer,
               bool(mojom::CreateCommandBufferParamsPtr,
                    int32_t,
                    base::UnsafeSharedMemoryRegion,
                    mojo::PendingAssociatedReceiver<mojom::CommandBuffer>,
                    mojo::PendingAssociatedRemote<mojom::CommandBufferClient>,
                    ContextResult*,
                    Capabilities*,
                    GLCapabilities*));
  MOCK_METHOD1(DestroyCommandBuffer, bool(int32_t));
  MOCK_METHOD2(DestroyCommandBuffer,
               void(int32_t, DestroyCommandBufferCallback));
  MOCK_METHOD2(ScheduleImageDecode,
               void(mojom::ScheduleImageDecodeParamsPtr, uint64_t));
  MOCK_METHOD2(FlushDeferredRequests,
               void(std::vector<mojom::DeferredRequestPtr>, uint32_t));
  MOCK_METHOD4(CreateGpuMemoryBuffer,
               void(const gfx::Size&,
                    const viz::SharedImageFormat&,
                    gfx::BufferUsage,
                    CreateGpuMemoryBufferCallback));
  MOCK_METHOD2(GetGpuMemoryBufferHandleInfo,
               void(const gpu::Mailbox&, GetGpuMemoryBufferHandleInfoCallback));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD3(CreateStreamTexture,
               void(int32_t,
                    mojo::PendingAssociatedReceiver<mojom::StreamTexture>,
                    CreateStreamTextureCallback));
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  MOCK_METHOD3(CreateDCOMPTexture,
               void(int32_t,
                    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture>,
                    CreateDCOMPTextureCallback));
  MOCK_METHOD3(RegisterOverlayStateObserver,
               void(mojo::PendingRemote<gpu::mojom::OverlayStateObserver>,
                    const gpu::Mailbox&,
                    RegisterOverlayStateObserverCallback));
  MOCK_METHOD4(CopyToGpuMemoryBufferAsync,
               void(const Mailbox&,
                    const std::vector<SyncToken>&,
                    uint64_t,
                    CopyToGpuMemoryBufferAsyncCallback));
  MOCK_METHOD3(CopyNativeGmbToSharedMemorySync,
               void(gfx::GpuMemoryBufferHandle,
                    base::UnsafeSharedMemoryRegion,
                    CopyNativeGmbToSharedMemorySyncCallback));
  MOCK_METHOD3(CopyNativeGmbToSharedMemoryAsync,
               void(gfx::GpuMemoryBufferHandle,
                    base::UnsafeSharedMemoryRegion,
                    CopyNativeGmbToSharedMemoryAsyncCallback));
#endif  // BUILDFLAG(IS_WIN)
  MOCK_METHOD4(WaitForTokenInRange,
               void(int32_t, int32_t, int32_t, WaitForTokenInRangeCallback));
  MOCK_METHOD5(WaitForGetOffsetInRange,
               void(int32_t,
                    uint32_t,
                    int32_t,
                    int32_t,
                    WaitForGetOffsetInRangeCallback));
  MOCK_METHOD5(
      WaitForGetOffsetInRange,
      bool(int32_t, uint32_t, int32_t, int32_t, CommandBuffer::State*));
#if BUILDFLAG(IS_FUCHSIA)
  MOCK_METHOD5(RegisterSysmemBufferCollection,
               void(mojo::PlatformHandle,
                    mojo::PlatformHandle,
                    const viz::SharedImageFormat&,
                    gfx::BufferUsage,
                    bool));
#endif  // BUILDFLAG(IS_FUCHSIA)
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
