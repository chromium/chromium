// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBindGroupDescriptor;

class GPUBindGroup : public DawnObject<WGPUBindGroup> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUBindGroup* Create(GPUDevice* device,
                              const GPUBindGroupDescriptor* webgpu_desc);
  explicit GPUBindGroup(GPUDevice* device, WGPUBindGroup bind_group);
  ~GPUBindGroup() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUBindGroup);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_H_
