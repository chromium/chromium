// Copyright 2019 The Chromium Authors
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

  GPUTextureView(const GPUTextureView&) = delete;
  GPUTextureView& operator=(const GPUTextureView&) = delete;

 private:
  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetProcs().textureViewSetLabel(GetHandle(), utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_VIEW_H_
