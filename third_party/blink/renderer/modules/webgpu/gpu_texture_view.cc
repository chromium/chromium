// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPUTextureView::GPUTextureView(GPUDevice* device,
                               wgpu::TextureView texture_view,
                               const String& label)
    : DawnObject<wgpu::TextureView>(device, std::move(texture_view), label) {}

}  // namespace blink
