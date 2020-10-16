// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_fence_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_bitmap_copy_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_copy_view.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

WGPUOrigin3D GPUOrigin2DToWGPUOrigin3D(
    const UnsignedLongEnforceRangeSequenceOrGPUOrigin2DDict* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {};

  if (webgpu_origin->IsUnsignedLongEnforceRangeSequence()) {
    const Vector<uint32_t>& webgpu_origin_sequence =
        webgpu_origin->GetAsUnsignedLongEnforceRangeSequence();
    DCHECK_EQ(webgpu_origin_sequence.size(), 3UL);
    dawn_origin.x = webgpu_origin_sequence[0];
    dawn_origin.y = webgpu_origin_sequence[1];
    dawn_origin.z = 0;
  } else if (webgpu_origin->IsGPUOrigin2DDict()) {
    const GPUOrigin2DDict* webgpu_origin_2d_dict =
        webgpu_origin->GetAsGPUOrigin2DDict();
    dawn_origin.x = webgpu_origin_2d_dict->x();
    dawn_origin.y = webgpu_origin_2d_dict->y();
    dawn_origin.z = 0;
  } else {
    NOTREACHED();
  }

  return dawn_origin;
}

// TODO(shaobo.yan@intel.com): This function will be removed when
// dawn has the copyTextureCHROMIUM like API.
bool AreCompatibleFormatForImageBitmapGPUCopy(
    SkColorType sk_color_type,
    WGPUTextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case WGPUTextureFormat_RGBA8Unorm:
      return sk_color_type == SkColorType::kRGBA_8888_SkColorType;
    case WGPUTextureFormat_BGRA8Unorm:
      return sk_color_type == SkColorType::kBGRA_8888_SkColorType;
    case WGPUTextureFormat_RGB10A2Unorm:
      return sk_color_type == SkColorType::kRGBA_1010102_SkColorType;
    case WGPUTextureFormat_RGBA16Float:
      return sk_color_type == SkColorType::kRGBA_F16_SkColorType;
    case WGPUTextureFormat_RGBA32Float:
      return sk_color_type == SkColorType::kRGBA_F32_SkColorType;
    case WGPUTextureFormat_RG8Unorm:
      return sk_color_type == SkColorType::kR8G8_unorm_SkColorType;
    case WGPUTextureFormat_RG16Float:
      return sk_color_type == SkColorType::kR16G16_float_SkColorType;
    default:
      return false;
  }
}

bool IsValidCopyIB2TDestinationFormat(WGPUTextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG16Float:
      return true;
    default:
      return false;
  }
}

bool CanUploadThroughGPU(StaticBitmapImage* image,
                         GPUTexture* dest_texture) {
  // Cannot handle top left origin image
  if (image->CurrentFrameOrientation().Orientation() !=
      ImageOrientationEnum::kOriginBottomLeft) {
    return false;
  }

  // Cannot handle source and dest texture have uncompatible format
  SkImageInfo image_info = image->PaintImageForCurrentFrame().GetSkImageInfo();
  if (!AreCompatibleFormatForImageBitmapGPUCopy(image_info.colorType(),
                                                dest_texture->Format())) {
    return false;
  }

  // Only Windows platform can try this path now
  // TODO(shaobo.yan@intel.com) : release this condition for all passthrough
  // platform
#if defined(OS_WIN)
  // TODO(shaobo.yan@intel.com): Need to figure out color space and
  // pre/unmultiply alpha
  return true;
#else
  return false;
#endif  // defined(OS_WIN)
}
}  // anonymous namespace

GPUQueue::GPUQueue(GPUDevice* device, WGPUQueue queue)
    : DawnObject<WGPUQueue>(device, queue) {
  produce_dawn_texture_handler_ = base::AdoptRef(new DawnTextureFromImageBitmap(
      GetDawnControlClient(), GetDeviceClientID()));
}

GPUQueue::~GPUQueue() {
  produce_dawn_texture_handler_ = nullptr;

  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().queueRelease(GetHandle());
}

void GPUQueue::submit(const HeapVector<Member<GPUCommandBuffer>>& buffers) {
  std::unique_ptr<WGPUCommandBuffer[]> commandBuffers = AsDawnType(buffers);

  GetProcs().queueSubmit(GetHandle(), buffers.size(), commandBuffers.get());
  // WebGPU guarantees that submitted commands finish in finite time so we
  // need to ensure commands are flushed. Flush immediately so the GPU process
  // eagerly processes commands to maximize throughput.
  FlushNow();
}

void GPUQueue::signal(GPUFence* fence, uint64_t signal_value) {
  GetProcs().queueSignal(GetHandle(), fence->GetHandle(), signal_value);
  // Signaling a fence adds a callback to update the fence value to the
  // completed value. WebGPU guarantees that the fence completion is
  // observable in finite time so we need to ensure commands are flushed.
  EnsureFlush();
}

GPUFence* GPUQueue::createFence(const GPUFenceDescriptor* descriptor) {
  DCHECK(descriptor);

  std::string label;
  WGPUFenceDescriptor desc = {};
  desc.nextInChain = nullptr;
  desc.initialValue = descriptor->initialValue();
  if (descriptor->hasLabel()) {
    label = descriptor->label().Utf8();
    desc.label = label.c_str();
  }

  return MakeGarbageCollected<GPUFence>(
      device_, GetProcs().queueCreateFence(GetHandle(), &desc));
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_byte_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->byteLengthAsSizeT(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_byte_offset, {}, exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_byte_offset,
                           uint64_t byte_size,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->byteLengthAsSizeT(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_byte_offset, byte_size, exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->ByteLengthAsSizeT(),
                  data->DataMaybeShared(), 1, data_byte_offset, {},
                  exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           uint64_t byte_size,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->ByteLengthAsSizeT(),
                  data->DataMaybeShared(), 1, data_byte_offset, byte_size,
                  exception_state);
}

void GPUQueue::WriteBufferImpl(GPUBuffer* buffer,
                               uint64_t buffer_offset,
                               uint64_t data_byte_length,
                               const void* data_base_ptr,
                               unsigned data_bytes_per_element,
                               uint64_t data_byte_offset,
                               base::Optional<uint64_t> byte_size,
                               ExceptionState& exception_state) {
  if (buffer_offset % 4 != 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "bufferOffset must be a multiple of 4");
    return;
  }

  if (data_byte_offset % data_bytes_per_element != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "dataByteOffset must be a multiple of data.BYTES_PER_ELEMENT");
    return;
  }

  if (data_byte_offset > data_byte_length) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "dataByteOffset is too large");
    return;
  }
  uint64_t max_write_size = data_byte_length - data_byte_offset;

  uint64_t write_byte_size = max_write_size;
  if (byte_size.has_value()) {
    write_byte_size = byte_size.value();
    if (write_byte_size > max_write_size) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "byteSize is too large");
      return;
    }
  }
  if (write_byte_size % std::max(4u, data_bytes_per_element) != 0) {
    exception_state.ThrowRangeError(
        "byteSize must be a multiple of max(4, data.BYTES_PER_ELEMENT)");
    return;
  }

  // Check that the write size can be cast to a size_t. This should always be
  // the case since data_byte_length comes from an ArrayBuffer size.
  if (write_byte_size > uint64_t(std::numeric_limits<size_t>::max())) {
    exception_state.ThrowRangeError(
        "writeSize larger than size_t (please report a bug if you see this)");
    return;
  }

  const uint8_t* data_base_ptr_bytes =
      static_cast<const uint8_t*>(data_base_ptr);
  const uint8_t* data_ptr = data_base_ptr_bytes + data_byte_offset;
  GetProcs().queueWriteBuffer(GetHandle(), buffer->GetHandle(), buffer_offset,
                              data_ptr, static_cast<size_t>(write_byte_size));
}

void GPUQueue::writeTexture(
    GPUTextureCopyView* destination,
    const MaybeShared<DOMArrayBufferView>& data,
    GPUTextureDataLayout* data_layout,
    UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
    ExceptionState& exception_state) {
  WriteTextureImpl(destination, data->BaseAddressMaybeShared(),
                   data->byteLengthAsSizeT(), data_layout, write_size,
                   exception_state);
}

void GPUQueue::writeTexture(
    GPUTextureCopyView* destination,
    const DOMArrayBufferBase* data,
    GPUTextureDataLayout* data_layout,
    UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
    ExceptionState& exception_state) {
  WriteTextureImpl(destination, data->DataMaybeShared(),
                   data->ByteLengthAsSizeT(), data_layout, write_size,
                   exception_state);
}

void GPUQueue::WriteTextureImpl(
    GPUTextureCopyView* destination,
    const void* data,
    size_t data_size,
    GPUTextureDataLayout* data_layout,
    UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& write_size,
    ExceptionState& exception_state) {
  WGPUTextureCopyView dawn_destination = AsDawnType(destination, device_);
  WGPUTextureDataLayout dawn_data_layout = AsDawnType(data_layout);
  WGPUExtent3D dawn_write_size = AsDawnType(&write_size);

  GetProcs().queueWriteTexture(GetHandle(), &dawn_destination, data, data_size,
                               &dawn_data_layout, &dawn_write_size);
  return;
}

// TODO(shaobo.yan@intel.com): Implement this function
void GPUQueue::copyImageBitmapToTexture(
    GPUImageBitmapCopyView* source,
    GPUTextureCopyView* destination,
    UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  if (!source->imageBitmap()) {
    exception_state.ThrowTypeError("No valid imageBitmap");
    return;
  }

  // ImageBitmap shouldn't in closed state.
  if (source->imageBitmap()->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "ImageBitmap is closed.");
    return;
  }

  scoped_refptr<StaticBitmapImage> image = source->imageBitmap()->BitmapImage();


  // TODO(shaobo.yan@intel.com) : Check that the destination GPUTexture has an
  // appropriate format. Now only support texture format exactly the same. The
  // compatible formats need to be defined in WebGPU spec.

  WGPUExtent3D dawn_copy_size = AsDawnType(&copy_size);

  // Extract imageBitmap attributes
  WGPUOrigin3D origin_in_image_bitmap =
      GPUOrigin2DToWGPUOrigin3D(&(source->origin()));

  // Validate copy depth
  if (dawn_copy_size.depth > 1) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "Copy depth is out of bounds of imageBitmap.");
    return;
  }

  // Validate origin value
  if (static_cast<uint32_t>(image->width()) < origin_in_image_bitmap.x ||
      static_cast<uint32_t>(image->height()) < origin_in_image_bitmap.y) {
    GetProcs().deviceInjectError(
        device_->GetHandle(), WGPUErrorType_Validation,
        "Copy origin is out of bounds of imageBitmap.");
    return;
  }

  // Validate the copy rect is inside the imageBitmap
  if (image->width() - origin_in_image_bitmap.x < dawn_copy_size.width ||
      image->height() - origin_in_image_bitmap.y < dawn_copy_size.height) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "Copy rect is out of bounds of imageBitmap.");
    return;
  }

  WGPUTextureCopyView dawn_destination = AsDawnType(destination, device_);

  if (!IsValidCopyIB2TDestinationFormat(destination->texture()->Format())) {
    return exception_state.ThrowTypeError("Invalid gpu texture format.");
    return;
  }

  bool isNoopCopy = dawn_copy_size.width == 0 || dawn_copy_size.height == 0 ||
                    dawn_copy_size.depth == 0;

  // TODO(shaobo.yan@intel.com): Implement GPU copy path
  // Try GPU path first and delegate noop copy to CPU path.
  if (image->IsTextureBacked() && !isNoopCopy) {  // Try GPU uploading path.
    if (CanUploadThroughGPU(image.get(), destination->texture())) {
      if (CopyContentFromGPU(image.get(), origin_in_image_bitmap,
                             dawn_copy_size, dawn_destination)) {
        return;
      }
    }
    // GPU path failed, fallback to CPU path
    image = image->MakeUnaccelerated();
  }
  // CPU path is the fallback path and should always work.
  if (!CopyContentFromCPU(image.get(), origin_in_image_bitmap, dawn_copy_size,
                          dawn_destination, destination->texture()->Format())) {
    exception_state.ThrowTypeError("Failed to copy content from imageBitmap.");
    return;
  }
}

bool GPUQueue::CopyContentFromCPU(StaticBitmapImage* image,
                                  const WGPUOrigin3D& origin,
                                  const WGPUExtent3D& copy_size,
                                  const WGPUTextureCopyView& destination,
                                  const WGPUTextureFormat dest_texture_format) {
  // Prepare for uploading CPU data.
  IntRect image_data_rect(origin.x, origin.y, copy_size.width,
                          copy_size.height);

  WebGPUImageUploadSizeInfo info = ComputeImageBitmapWebGPUUploadSizeInfo(
      image_data_rect, dest_texture_format);

  bool isNoopCopy = info.size_in_bytes == 0 || copy_size.depth == 0;

  // Create a mapped buffer to receive image bitmap contents
  WGPUBufferDescriptor buffer_desc = {};
  buffer_desc.usage = WGPUBufferUsage_CopySrc;
  buffer_desc.size = info.size_in_bytes;
  buffer_desc.mappedAtCreation = !isNoopCopy;

  if (buffer_desc.size > uint64_t(std::numeric_limits<size_t>::max())) {
    return false;
  }
  size_t size = static_cast<size_t>(buffer_desc.size);

  WGPUBuffer buffer =
      GetProcs().deviceCreateBuffer(device_->GetHandle(), &buffer_desc);

  // Bypass extract source content in noop copy but follow the copy path
  // for validation.
  if (!isNoopCopy) {
    void* data = GetProcs().bufferGetMappedRange(buffer, 0, size);

    if (!CopyBytesFromImageBitmapForWebGPU(
            image, base::span<uint8_t>(static_cast<uint8_t*>(data), size),
            image_data_rect, dest_texture_format)) {
      // Release the buffer.
      GetProcs().bufferRelease(buffer);
      return false;
    }

    GetProcs().bufferUnmap(buffer);
  }

  // Start a B2T copy to move contents from buffer to destination texture
  WGPUBufferCopyView dawn_intermediate = {};
  dawn_intermediate.nextInChain = nullptr;
  dawn_intermediate.buffer = buffer;
  dawn_intermediate.layout.offset = 0;
  dawn_intermediate.layout.bytesPerRow = info.wgpu_bytes_per_row;
  dawn_intermediate.layout.rowsPerImage = image->height();

  WGPUCommandEncoder encoder =
      GetProcs().deviceCreateCommandEncoder(device_->GetHandle(), nullptr);
  GetProcs().commandEncoderCopyBufferToTexture(encoder, &dawn_intermediate,
                                               &destination, &copy_size);
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, nullptr);

  // Don't need to add fence after this submit. Because if user want to use the
  // texture to do copy or render, it will trigger another queue submit. Dawn
  // will insert the necessary resource transitions.
  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);
  GetProcs().bufferRelease(buffer);

  return true;
}

bool GPUQueue::CopyContentFromGPU(StaticBitmapImage* image,
                                  const WGPUOrigin3D& origin,
                                  const WGPUExtent3D& copy_size,
                                  const WGPUTextureCopyView& destination) {
  WGPUTexture src_texture =
      produce_dawn_texture_handler_->ProduceDawnTextureFromImageBitmap(image);
  // Failed to produceDawnTexture.
  if (!src_texture) {
    return false;
  }

  WGPUTextureCopyView src = {};
  src.texture = src_texture;
  src.origin = origin;

  WGPUCommandEncoder encoder =
      GetProcs().deviceCreateCommandEncoder(device_->GetHandle(), nullptr);
  GetProcs().commandEncoderCopyTextureToTexture(encoder, &src, &destination,
                                                &copy_size);
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, nullptr);

  // Don't need to add fence after this submit. Because if user want to use the
  // texture to do copy or render, it will trigger another queue submit. Dawn
  // will insert the necessary resource transitions.
  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);

  produce_dawn_texture_handler_->FinishDawnTextureFromImageBitmapAccess();
  return true;
}

}  // namespace blink
