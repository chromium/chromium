// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

#define DAWN_OBJECTS                          \
  X(BindGroup, bindGroup)                     \
  X(BindGroupLayout, bindGroupLayout)         \
  X(Buffer, buffer)                           \
  X(CommandBuffer, commandBuffer)             \
  X(CommandEncoder, commandEncoder)           \
  X(ComputePassEncoder, computePassEncoder)   \
  X(ComputePipeline, computePipeline)         \
  X(Device, device)                           \
  X(Fence, fence)                             \
  X(Instance, instance)                       \
  X(PipelineLayout, pipelineLayout)           \
  X(QuerySet, querySet)                       \
  X(Queue, queue)                             \
  X(RenderBundle, renderBundle)               \
  X(RenderBundleEncoder, renderBundleEncoder) \
  X(RenderPassEncoder, renderPassEncoder)     \
  X(RenderPipeline, renderPipeline)           \
  X(Sampler, sampler)                         \
  X(ShaderModule, shaderModule)               \
  X(Surface, surface)                         \
  X(SwapChain, swapChain)                     \
  X(Texture, texture)                         \
  X(TextureView, textureView)

namespace gpu {
namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu
}  // namespace gpu

namespace blink {

template <typename T>
struct WGPUReleaseFn;

#define X(Name, name)                                         \
  template <>                                                 \
  struct WGPUReleaseFn<WGPU##Name> {                          \
    static constexpr void (*DawnProcTable::*fn)(WGPU##Name) = \
        &DawnProcTable::name##Release;                        \
  };
DAWN_OBJECTS
#undef X

class GPUDevice;

// This class allows objects to hold onto a DawnControlClientHolder.
// The DawnControlClientHolder is used to hold the WebGPUInterface and keep
// track of whether or not the client has been destroyed. If the client is
// destroyed, we should not call any Dawn functions.
class DawnObjectBase {
 public:
  explicit DawnObjectBase(
      scoped_refptr<DawnControlClientHolder> dawn_control_client);

  const scoped_refptr<DawnControlClientHolder>& GetDawnControlClient() const;
  gpu::webgpu::WebGPUInterface* GetInterface() const;
  const DawnProcTable& GetProcs() const;

  // Ensure commands up until now on this object's parent device are flushed by
  // the end of the task.
  void EnsureFlush();

  // Flush commands up until now on this object's parent device immediately.
  void FlushNow();

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
};

class DawnObjectImpl : public ScriptWrappable, public DawnObjectBase {
 public:
  explicit DawnObjectImpl(GPUDevice* device);
  ~DawnObjectImpl() override;

  WGPUDevice GetDeviceHandle();

  void Trace(Visitor* visitor) const override;

 protected:
  Member<GPUDevice> device_;
};

template <typename Handle>
class DawnObject : public DawnObjectImpl {
 public:
  DawnObject(GPUDevice* device, Handle handle)
      : DawnObjectImpl(device),
        handle_(handle),
        device_handle_(GetDeviceHandle()) {
    // All WebGPU Blink objects created directly or by the Device hold a
    // Member<GPUDevice> which keeps the device alive. However, this does not
    // enforce that the GPUDevice is finalized after all objects referencing it.
    // Add an extra ref in this constructor, and a release in the destructor to
    // ensure that the Dawn device is destroyed last.
    // TODO(enga): Investigate removing Member<GPUDevice>.
    GetProcs().deviceReference(device_handle_);
  }

  ~DawnObject() override {
    // Note: The device is released last because all child objects must be
    // destroyed first.
    (GetProcs().*WGPUReleaseFn<Handle>::fn)(handle_);
    GetProcs().deviceRelease(device_handle_);
  }

  Handle GetHandle() const { return handle_; }

 private:
  Handle const handle_;
  WGPUDevice device_handle_;
};

template <>
class DawnObject<WGPUDevice> : public DawnObjectBase {
 public:
  DawnObject(scoped_refptr<DawnControlClientHolder> dawn_control_client,
             WGPUDevice handle)
      : DawnObjectBase(dawn_control_client), handle_(handle) {}
  ~DawnObject() { GetProcs().deviceRelease(handle_); }

  WGPUDevice GetHandle() const { return handle_; }

 private:
  WGPUDevice const handle_;
};

}  // namespace blink

#undef DAWN_OBJECTS

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_DAWN_OBJECT_H_
