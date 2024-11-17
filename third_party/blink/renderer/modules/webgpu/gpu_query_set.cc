// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_query_set_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

// static
GPUQuerySet* GPUQuerySet::Create(GPUDevice* device,
                                 const GPUQuerySetDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  wgpu::QuerySetDescriptor dawn_desc = {
      .type = AsDawnEnum(webgpu_desc->type()),
      .count = webgpu_desc->count(),
  };

  std::string label = webgpu_desc->label().Utf8();
  if (!label.empty()) {
    dawn_desc.label = label.c_str();
  }

  GPUQuerySet* query_set = MakeGarbageCollected<GPUQuerySet>(
      device, device->GetHandle().CreateQuerySet(&dawn_desc),
      webgpu_desc->label());
  return query_set;
}

GPUQuerySet::GPUQuerySet(GPUDevice* device,
                         wgpu::QuerySet querySet,
                         const String& label)
    : DawnObject<wgpu::QuerySet>(device, std::move(querySet), label) {}

void GPUQuerySet::destroy() {
  GetHandle().Destroy();
}

V8GPUQueryType GPUQuerySet::type() const {
  return FromDawnEnum(GetHandle().GetType());
}

uint32_t GPUQuerySet::count() const {
  return GetHandle().GetCount();
}

}  // namespace blink
