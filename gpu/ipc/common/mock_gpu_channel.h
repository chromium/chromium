// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
#define GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_

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
  MOCK_METHOD0(Flush, bool());
  MOCK_METHOD1(Flush, void(FlushCallback));
  MOCK_METHOD2(ScheduleImageDecode,
               void(mojom::ScheduleImageDecodeParamsPtr, uint64_t));
  MOCK_METHOD1(FlushDeferredRequests,
               void(std::vector<mojom::DeferredRequestPtr>));
};

}  // namespace gpu

#endif  // GPU_IPC_COMMON_MOCK_GPU_CHANNEL_H_
