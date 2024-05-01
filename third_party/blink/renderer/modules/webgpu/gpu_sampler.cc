// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_sampler_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

namespace {

wgpu::SamplerDescriptor AsDawnType(const GPUSamplerDescriptor* webgpu_desc,
                                   std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  wgpu::SamplerDescriptor dawn_desc = {
      .addressModeU = AsDawnEnum(webgpu_desc->addressModeU()),
      .addressModeV = AsDawnEnum(webgpu_desc->addressModeV()),
      .addressModeW = AsDawnEnum(webgpu_desc->addressModeW()),
      .magFilter = AsDawnEnum(webgpu_desc->magFilter()),
      .minFilter = AsDawnEnum(webgpu_desc->minFilter()),
      .mipmapFilter = AsDawnEnum(webgpu_desc->mipmapFilter()),
      .lodMinClamp = webgpu_desc->lodMinClamp(),
      .lodMaxClamp = webgpu_desc->lodMaxClamp(),
      .maxAnisotropy = webgpu_desc->maxAnisotropy(),
  };
  if (webgpu_desc->hasCompare()) {
    dawn_desc.compare = AsDawnEnum(webgpu_desc->compare());
  }
  *label = webgpu_desc->label().Utf8();
  if (!label->empty()) {
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
  wgpu::SamplerDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  GPUSampler* sampler = MakeGarbageCollected<GPUSampler>(
      device, device->GetHandle().CreateSampler(&dawn_desc),
      webgpu_desc->label());
  return sampler;
}

GPUSampler::GPUSampler(GPUDevice* device,
                       wgpu::Sampler sampler,
                       const String& label)
    : DawnObject<wgpu::Sampler>(device, std::move(sampler), label) {}

}  // namespace blink
