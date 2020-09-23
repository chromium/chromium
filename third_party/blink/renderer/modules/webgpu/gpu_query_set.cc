// Copyright 2020 The Chromium Authors. All rights reserved.
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

  WGPUQuerySetDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.type = AsDawnEnum<WGPUQueryType>(webgpu_desc->type());
  dawn_desc.count = webgpu_desc->count();

  std::unique_ptr<WGPUPipelineStatisticName[]> pipeline_statistics;
  if (webgpu_desc->hasPipelineStatistics()) {
    pipeline_statistics = AsDawnEnum<WGPUPipelineStatisticName>(
        webgpu_desc->pipelineStatistics());
    dawn_desc.pipelineStatistics = pipeline_statistics.get();
    dawn_desc.pipelineStatisticsCount =
        webgpu_desc->pipelineStatistics().size();
  }

  std::string label;
  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPUQuerySet>(
      device,
      device->GetProcs().deviceCreateQuerySet(device->GetHandle(), &dawn_desc));
}

GPUQuerySet::GPUQuerySet(GPUDevice* device, WGPUQuerySet querySet)
    : DawnObject<WGPUQuerySet>(device, querySet) {}

GPUQuerySet::~GPUQuerySet() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().querySetRelease(GetHandle());
}

void GPUQuerySet::destroy() {
  GetProcs().querySetDestroy(GetHandle());
}

}  // namespace blink
