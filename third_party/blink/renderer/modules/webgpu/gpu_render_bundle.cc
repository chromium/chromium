// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPURenderBundle* GPURenderBundle::Create(GPUDevice* device,
                                         WGPURenderBundle render_bundle) {
  return MakeGarbageCollected<GPURenderBundle>(device, render_bundle);
}

GPURenderBundle::GPURenderBundle(GPUDevice* device,
                                 WGPURenderBundle render_bundle)
    : DawnObject<WGPURenderBundle>(device, render_bundle) {}

GPURenderBundle::~GPURenderBundle() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderBundleRelease(GetHandle());
}

}  // namespace blink
