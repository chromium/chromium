// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
#define GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_

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
  MOCK_METHOD1(Flush, void(FlushCallback));
  MOCK_METHOD6(CreateCommandBuffer,
               void(mojom::CreateCommandBufferParamsPtr,
                    int32_t,
                    base::UnsafeSharedMemoryRegion,
                    mojo::PendingAssociatedReceiver<mojom::CommandBuffer>,
                    mojo::PendingAssociatedRemote<mojom::CommandBufferClient>,
                    CreateCommandBufferCallback));
  MOCK_METHOD7(CreateCommandBuffer,
               bool(mojom::CreateCommandBufferParamsPtr,
                    int32_t,
                    base::UnsafeSharedMemoryRegion,
                    mojo::PendingAssociatedReceiver<mojom::CommandBuffer>,
                    mojo::PendingAssociatedRemote<mojom::CommandBufferClient>,
                    ContextResult*,
                    Capabilities*));
  MOCK_METHOD1(DestroyCommandBuffer, bool(int32_t));
  MOCK_METHOD2(DestroyCommandBuffer,
               void(int32_t, DestroyCommandBufferCallback));
  MOCK_METHOD2(ScheduleImageDecode,
               void(mojom::ScheduleImageDecodeParamsPtr, uint64_t));
  MOCK_METHOD1(FlushDeferredRequests,
               void(std::vector<mojom::DeferredRequestPtr>));
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
#endif  // BUILDFLAG(IS_WIN)
  MOCK_METHOD4(WaitForTokenInRange,
               void(int32_t, int32_t, int32_t, WaitForTokenInRangeCallback));
  MOCK_METHOD5(WaitForGetOffsetInRange,
               void(int32_t,
                    uint32_t,
                    int32_t,
                    int32_t,
                    WaitForGetOffsetInRangeCallback));
#if BUILDFLAG(IS_FUCHSIA)
  MOCK_METHOD5(RegisterSysmemBufferCollection,
               void(mojo::PlatformHandle,
                    mojo::PlatformHandle,
                    gfx::BufferFormat,
                    gfx::BufferUsage,
                    bool));
#endif  // BUILDFLAG(IS_FUCHSIA)
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
