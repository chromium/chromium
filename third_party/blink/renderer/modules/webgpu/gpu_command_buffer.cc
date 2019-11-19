// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUCommandBuffer* GPUCommandBuffer::Create(GPUDevice* device,
                                           WGPUCommandBuffer command_buffer) {
  return MakeGarbageCollected<GPUCommandBuffer>(device, command_buffer);
}

GPUCommandBuffer::GPUCommandBuffer(GPUDevice* device,
                                   WGPUCommandBuffer command_buffer)
    : DawnObject<WGPUCommandBuffer>(device, command_buffer) {}

GPUCommandBuffer::~GPUCommandBuffer() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().commandBufferRelease(GetHandle());
}

}  // namespace blink
