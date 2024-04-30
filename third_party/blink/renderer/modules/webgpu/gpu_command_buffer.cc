// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPUCommandBuffer::GPUCommandBuffer(GPUDevice* device,
                                   wgpu::CommandBuffer command_buffer,
                                   const String& label)
    : DawnObject<wgpu::CommandBuffer>(device,
                                      std::move(command_buffer),
                                      label) {}

}  // namespace blink
