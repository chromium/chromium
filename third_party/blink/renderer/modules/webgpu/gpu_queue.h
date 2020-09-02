// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class DawnTextureFromImageBitmap;
class ExceptionState;
class GPUBuffer;
class GPUCommandBuffer;
class GPUFence;
class GPUFenceDescriptor;
class GPUImageBitmapCopyView;
class GPUTextureCopyView;
class GPUTextureDataLayout;
class StaticBitmapImage;
class UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict;

class GPUQueue : public DawnObject<WGPUQueue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUQueue(GPUDevice* device, WGPUQueue queue);
  ~GPUQueue() override;

  // gpu_queue.idl
  void submit(const HeapVector<Member<GPUCommandBuffer>>& buffers);
  void signal(GPUFence* fence, uint64_t signal_value);
  GPUFence* createFence(const GPUFenceDescriptor* descriptor);
  void writeBuffer(GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const MaybeShared<DOMArrayBufferView>& data,
                   uint64_t data_byte_offset,
                   ExceptionState& exception_state);
  void writeBuffer(GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const MaybeShared<DOMArrayBufferView>& data,
                   uint64_t data_byte_offset,
                   uint64_t byte_size,
                   ExceptionState& exception_state);
  void writeBuffer(GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const DOMArrayBufferBase* data,
                   uint64_t data_byte_offset,
                   ExceptionState& exception_state);
  void writeBuffer(GPUBuffer* buffer,
                   uint64_t buffer_offset,
                   const DOMArrayBufferBase* data,
                   uint64_t data_byte_offset,
                   uint64_t byte_size,
                   ExceptionState& exception_state);
  void writeTexture(
      GPUTextureCopyView* destination,
      const MaybeShared<DOMArrayBufferView>& data,
      GPUTextureDataLayout* data_layout,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
      ExceptionState& exception_state);
  void writeTexture(
      GPUTextureCopyView* destination,
      const DOMArrayBufferBase* data,
      GPUTextureDataLayout* data_layout,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
      ExceptionState& exception_state);
  void copyImageBitmapToTexture(
      GPUImageBitmapCopyView* source,
      GPUTextureCopyView* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copySize,
      ExceptionState& exception_state);

 private:
  bool CopyContentFromCPU(StaticBitmapImage* image,
                          const WGPUOrigin3D& origin,
                          const WGPUExtent3D& copy_size,
                          const WGPUTextureCopyView& destination,
                          const WGPUTextureFormat dest_texture_format);
  bool CopyContentFromGPU(StaticBitmapImage* image,
                          const WGPUOrigin3D& origin,
                          const WGPUExtent3D& copy_size,
                          const WGPUTextureCopyView& destination);
  void WriteBufferImpl(GPUBuffer* buffer,
                       uint64_t buffer_offset,
                       uint64_t data_byte_length,
                       const void* data_base_ptr,
                       unsigned data_bytes_per_element,
                       uint64_t data_byte_offset,
                       base::Optional<uint64_t> byte_size,
                       ExceptionState& exception_state);
  void WriteTextureImpl(
      GPUTextureCopyView* destination,
      const void* data,
      size_t dataSize,
      GPUTextureDataLayout* data_layout,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
      ExceptionState& exception_state);

  scoped_refptr<DawnTextureFromImageBitmap> produce_dawn_texture_handler_;

  DISALLOW_COPY_AND_ASSIGN(GPUQueue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
