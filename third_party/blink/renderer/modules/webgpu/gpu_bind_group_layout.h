// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBindGroupLayoutDescriptor;

class GPUBindGroupLayout : public DawnObject<WGPUBindGroupLayout> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUBindGroupLayout* Create(
      GPUDevice* device,
      const GPUBindGroupLayoutDescriptor* webgpu_desc);
  explicit GPUBindGroupLayout(GPUDevice* device,
                              WGPUBindGroupLayout bind_group_layout);
  ~GPUBindGroupLayout() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUBindGroupLayout);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BIND_GROUP_LAYOUT_H_
