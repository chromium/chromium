// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPUExternalTexture::GPUExternalTexture(GPUDevice* device,
                                       WGPUExternalTexture externalTexture)
    : DawnObject<WGPUExternalTexture>(device, externalTexture) {}

}  // namespace blink
