// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_VIEW_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUTextureView : public DawnObject<WGPUTextureView> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUTextureView(GPUDevice* device, WGPUTextureView texture_view);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUTextureView);
};

// TODO(jie.a.chen@intel.com): Remove this once th Dawn CL below is landed.
// https://dawn-review.googlesource.com/c/dawn/+/63660
#if !defined(WGPU_ARRAY_LAYER_COUNT_UNDEFINED)
#define WGPU_ARRAY_LAYER_COUNT_UNDEFINED (0u)
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_VIEW_H_
