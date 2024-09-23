// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/test_gpu_channel_host_provider.h"

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace video_effects {

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  explicit TestGpuChannelHost(gpu::mojom::GpuChannel& gpu_channel)
      : GpuChannelHost(0 /* channel_id */,
                       gpu::GPUInfo(),
                       gpu::GpuFeatureInfo(),
                       gpu::SharedImageCapabilities(),
                       mojo::ScopedMessagePipeHandle(
                           mojo::MessagePipeHandle(mojo::kInvalidHandleValue))),
        gpu_channel_(gpu_channel) {}

  gpu::mojom::GpuChannel& GetGpuChannel() override {
    return gpu_channel_.get();
  }

 protected:
  ~TestGpuChannelHost() override = default;

 private:
  const raw_ref<gpu::mojom::GpuChannel> gpu_channel_;
};

TestGpuChannelHostProvider::TestGpuChannelHostProvider(
    gpu::mojom::GpuChannel& gpu_channel)
    : gpu_channel_(gpu_channel) {}

scoped_refptr<gpu::GpuChannelHost>
TestGpuChannelHostProvider::GetGpuChannelHost() {
  return base::MakeRefCounted<TestGpuChannelHost>(gpu_channel_.get());
}

}  // namespace video_effects
