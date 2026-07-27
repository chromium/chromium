// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RESOURCE_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RESOURCE_TABLE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUResourceTableDescriptor;

class GPUResourceTable : public DawnObject<wgpu::ResourceTable> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUResourceTable* Create(
      GPUDevice* device,
      const GPUResourceTableDescriptor* webgpu_desc);
  explicit GPUResourceTable(GPUDevice* device,
                            wgpu::ResourceTable table,
                            const String& label);

  GPUResourceTable(const GPUResourceTable&) = delete;
  GPUResourceTable& operator=(const GPUResourceTable&) = delete;

  void destroy();

  void update(uint32_t slot,
              V8GPUBindingResource* resource,
              ExceptionState& exception_state);
  uint32_t insert(V8GPUBindingResource* resource,
                  ExceptionState& exception_state);
  void remove(uint32_t slot, ExceptionState& exception_state);

  uint32_t size() const;

 private:
  void SetLabelImpl(std::string_view value) override {
    GetHandle().SetLabel(value);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_RESOURCE_TABLE_H_
