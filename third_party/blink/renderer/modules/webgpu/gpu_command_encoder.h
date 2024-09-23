// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ExceptionState;
class GPUImageCopyBuffer;
class GPUCommandBuffer;
class GPUCommandBufferDescriptor;
class GPUCommandEncoderDescriptor;
class GPUComputePassDescriptor;
class GPUComputePassEncoder;
class GPURenderPassDescriptor;
class GPURenderPassEncoder;
class GPUImageCopyTexture;

class GPUCommandEncoder : public DawnObject<wgpu::CommandEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandEncoder* Create(
      GPUDevice* device,
      const GPUCommandEncoderDescriptor* webgpu_desc);
  explicit GPUCommandEncoder(GPUDevice* device,
                             wgpu::CommandEncoder command_encoder,
                             const String& label);

  GPUCommandEncoder(const GPUCommandEncoder&) = delete;
  GPUCommandEncoder& operator=(const GPUCommandEncoder&) = delete;

  // gpu_command_encoder.idl
  GPURenderPassEncoder* beginRenderPass(
      const GPURenderPassDescriptor* descriptor,
      ExceptionState& exception_state);
  GPUComputePassEncoder* beginComputePass(
      const GPUComputePassDescriptor* descriptor,
      ExceptionState& exception_state);
  void copyBufferToBuffer(DawnObject<wgpu::Buffer>* src,
                          uint64_t src_offset,
                          DawnObject<wgpu::Buffer>* dst,
                          uint64_t dst_offset,
                          uint64_t size) {
    DCHECK(src);
    DCHECK(dst);
    GetHandle().CopyBufferToBuffer(src->GetHandle(), src_offset,
                                   dst->GetHandle(), dst_offset, size);
  }
  void copyBufferToTexture(GPUImageCopyBuffer* source,
                           GPUImageCopyTexture* destination,
                           const V8GPUExtent3D* copy_size,
                           ExceptionState& exception_state);
  void copyTextureToBuffer(GPUImageCopyTexture* source,
                           GPUImageCopyBuffer* destination,
                           const V8GPUExtent3D* copy_size,
                           ExceptionState& exception_state);
  void copyTextureToTexture(GPUImageCopyTexture* source,
                            GPUImageCopyTexture* destination,
                            const V8GPUExtent3D* copy_size,
                            ExceptionState& exception_state);
  void pushDebugGroup(String groupLabel) {
    std::string label = groupLabel.Utf8();
    GetHandle().PushDebugGroup(label.c_str());
  }
  void popDebugGroup() { GetHandle().PopDebugGroup(); }
  void insertDebugMarker(String markerLabel) {
    std::string label = markerLabel.Utf8();
    GetHandle().InsertDebugMarker(label.c_str());
  }
  void resolveQuerySet(DawnObject<wgpu::QuerySet>* querySet,
                       uint32_t firstQuery,
                       uint32_t queryCount,
                       DawnObject<wgpu::Buffer>* destination,
                       uint64_t destinationOffset) {
    GetHandle().ResolveQuerySet(querySet->GetHandle(), firstQuery, queryCount,
                                destination->GetHandle(), destinationOffset);
  }
  void writeTimestamp(DawnObject<wgpu::QuerySet>* querySet,
                      uint32_t queryIndex,
                      ExceptionState& exception_state);
  void clearBuffer(DawnObject<wgpu::Buffer>* buffer, uint64_t offset) {
    GetHandle().ClearBuffer(buffer->GetHandle(), offset);
  }
  void clearBuffer(DawnObject<wgpu::Buffer>* buffer,
                   uint64_t offset,
                   uint64_t size) {
    GetHandle().ClearBuffer(buffer->GetHandle(), offset, size);
  }
  GPUCommandBuffer* finish(const GPUCommandBufferDescriptor* descriptor);

  void setLabelImpl(const String& value) override {
    std::string utf8_label = value.Utf8();
    GetHandle().SetLabel(utf8_label.c_str());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
