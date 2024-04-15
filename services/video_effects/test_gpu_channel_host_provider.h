// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_TEST_GPU_CHANNEL_HOST_PROVIDER_H_
#define SERVICES_VIDEO_EFFECTS_TEST_GPU_CHANNEL_HOST_PROVIDER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_channel.mojom-forward.h"
#include "services/video_effects/video_effects_processor_impl.h"

namespace video_effects {

class TestGpuChannelHostProvider : public GpuChannelHostProvider {
 public:
  explicit TestGpuChannelHostProvider(gpu::mojom::GpuChannel& gpu_channel);

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() override;

 private:
  const raw_ref<gpu::mojom::GpuChannel> gpu_channel_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_TEST_GPU_CHANNEL_HOST_PROVIDER_H_
