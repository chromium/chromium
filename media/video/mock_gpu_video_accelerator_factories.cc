// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/video/mock_gpu_video_accelerator_factories.h"

#include <array>
#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/buffer_format_util.h"

namespace media {

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories(
    gpu::SharedImageInterface* sii)
    : sii_(sii) {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() = default;

bool MockGpuVideoAcceleratorFactories::IsGpuVideoDecodeAcceleratorEnabled() {
  return true;
}

bool MockGpuVideoAcceleratorFactories::IsGpuVideoEncodeAcceleratorEnabled() {
  return true;
}

base::UnsafeSharedMemoryRegion
MockGpuVideoAcceleratorFactories::CreateSharedMemoryRegion(size_t size) {
  return base::UnsafeSharedMemoryRegion::Create(size);
}

std::unique_ptr<VideoEncodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return base::WrapUnique(DoCreateVideoEncodeAccelerator());
}

bool MockGpuVideoAcceleratorFactories::ShouldUseGpuMemoryBuffersForVideoFrames(
    bool for_media_stream) const {
  return false;
}

}  // namespace media
