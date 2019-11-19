// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PIPELINE_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PIPELINE_LAYOUT_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUPipelineLayoutDescriptor;

class GPUPipelineLayout : public DawnObject<WGPUPipelineLayout> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUPipelineLayout* Create(
      GPUDevice* device,
      const GPUPipelineLayoutDescriptor* webgpu_desc);
  explicit GPUPipelineLayout(GPUDevice* device,
                             WGPUPipelineLayout pipeline_layout);
  ~GPUPipelineLayout() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUPipelineLayout);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_PIPELINE_LAYOUT_H_
