// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUERY_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUERY_SET_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class GPUQuerySetDescriptor;

class GPUQuerySet : public DawnObject<WGPUQuerySet> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUQuerySet* Create(GPUDevice* device,
                             const GPUQuerySetDescriptor* webgpu_desc);
  explicit GPUQuerySet(GPUDevice* device, WGPUQuerySet querySet);

  // gpu_queryset.idl
  void destroy();

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUQuerySet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUERY_SET_H_
