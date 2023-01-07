// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

GPURenderBundle::GPURenderBundle(GPUDevice* device,
                                 WGPURenderBundle render_bundle)
    : DawnObject<WGPURenderBundle>(device, render_bundle) {}

}  // namespace blink
