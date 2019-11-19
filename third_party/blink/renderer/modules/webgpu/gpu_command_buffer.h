// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUCommandBuffer : public DawnObject<WGPUCommandBuffer> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandBuffer* Create(GPUDevice* device,
                                  WGPUCommandBuffer command_buffer);
  explicit GPUCommandBuffer(GPUDevice* device,
                            WGPUCommandBuffer command_buffer);
  ~GPUCommandBuffer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCommandBuffer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_BUFFER_H_
