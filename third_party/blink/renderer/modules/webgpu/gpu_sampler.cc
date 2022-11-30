// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_sampler_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

namespace {

WGPUSamplerDescriptor AsDawnType(const GPUSamplerDescriptor* webgpu_desc,
                                 std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUSamplerDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.addressModeU = AsDawnEnum(webgpu_desc->addressModeU());
  dawn_desc.addressModeV = AsDawnEnum(webgpu_desc->addressModeV());
  dawn_desc.addressModeW = AsDawnEnum(webgpu_desc->addressModeW());
  dawn_desc.magFilter = AsDawnEnum(webgpu_desc->magFilter());
  dawn_desc.minFilter = AsDawnEnum(webgpu_desc->minFilter());
  dawn_desc.mipmapFilter = AsDawnEnum(webgpu_desc->mipmapFilter());
  dawn_desc.lodMinClamp = webgpu_desc->lodMinClamp();
  dawn_desc.lodMaxClamp = webgpu_desc->lodMaxClamp();
  dawn_desc.maxAnisotropy = webgpu_desc->maxAnisotropy();
  if (webgpu_desc->hasCompare()) {
    dawn_desc.compare = AsDawnEnum(webgpu_desc->compare());
  }
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUSampler* GPUSampler::Create(GPUDevice* device,
                               const GPUSamplerDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  std::string label;
  WGPUSamplerDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  GPUSampler* sampler = MakeGarbageCollected<GPUSampler>(
      device,
      device->GetProcs().deviceCreateSampler(device->GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    sampler->setLabel(webgpu_desc->label());
  return sampler;
}

GPUSampler::GPUSampler(GPUDevice* device, WGPUSampler sampler)
    : DawnObject<WGPUSampler>(device, sampler) {}

}  // namespace blink
