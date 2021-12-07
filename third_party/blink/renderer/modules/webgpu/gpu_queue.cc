// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_htmlcanvaselement_imagebitmap_offscreencanvas.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_external_image.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_image_bitmap.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture_tagged.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuorigin2ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

WGPUOrigin3D GPUOrigin2DToWGPUOrigin3D(const V8GPUOrigin2D* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {
      0,
      0,
      0,
  };

  switch (webgpu_origin->GetContentType()) {
    case V8GPUOrigin2D::ContentType::kGPUOrigin2DDict: {
      const GPUOrigin2DDict* webgpu_origin_2d_dict =
          webgpu_origin->GetAsGPUOrigin2DDict();
      dawn_origin.x = webgpu_origin_2d_dict->x();
      dawn_origin.y = webgpu_origin_2d_dict->y();
      break;
    }
    case V8GPUOrigin2D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& webgpu_origin_sequence =
          webgpu_origin->GetAsUnsignedLongEnforceRangeSequence();
      // The WebGPU spec states that if the sequence isn't big enough then the
      // default values of 0 are used (which are set above).
      switch (webgpu_origin_sequence.size()) {
        default:
          // This is a 2D origin and the depth should be 0 always.
          dawn_origin.y = webgpu_origin_sequence[1];
          FALLTHROUGH;
        case 1:
          dawn_origin.x = webgpu_origin_sequence[0];
          FALLTHROUGH;
        case 0:
          break;
      }
      break;
    }
  }

  return dawn_origin;
}

bool IsExternalImageWebGLCanvas(
    const V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas* external_image
) {
  CanvasRenderingContextHost* canvas = nullptr;
  switch (external_image->GetContentType()) {
    case V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas::ContentType::
        kHTMLCanvasElement:
      canvas = external_image->GetAsHTMLCanvasElement();
      break;
    case V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas::ContentType::
        kOffscreenCanvas:
      canvas = external_image->GetAsOffscreenCanvas();
      break;
    default:
      canvas = nullptr;
      break;
  }

  return canvas && canvas->IsWebGL();
}

bool IsValidExternalImageDestinationFormat(
    WGPUTextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case WGPUTextureFormat_R8Unorm:
    case WGPUTextureFormat_R16Float:
    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG8Unorm:
    case WGPUTextureFormat_RG16Float:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RGBA8Unorm:
    case WGPUTextureFormat_RGBA8UnormSrgb:
    case WGPUTextureFormat_BGRA8Unorm:
    case WGPUTextureFormat_BGRA8UnormSrgb:
    case WGPUTextureFormat_RGB10A2Unorm:
    case WGPUTextureFormat_RGBA16Float:
    case WGPUTextureFormat_RGBA32Float:
      return true;
    default:
      return false;
  }
}

// TODO(crbug.com/1197369)This duplicate code is for supporting deprecated API
// copyImageBitmapToTexture and should be remove in future.
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

bool IsValidCopyTextureForBrowserFormats(SkColorType src_color_type,
                                         WGPUTextureFormat dst_texture_format) {
  // CopyTextureForBrowser only supports RGBA8Unorm and BGRA8Unorm src texture.
  // TODO(crbug.com/dawn/856): Cover more source formats if needed.
  if ((src_color_type == SkColorType::kRGBA_8888_SkColorType ||
       src_color_type == SkColorType::kBGRA_8888_SkColorType) &&
      (dst_texture_format == WGPUTextureFormat_R8Unorm ||
       dst_texture_format == WGPUTextureFormat_R16Float ||
       dst_texture_format == WGPUTextureFormat_R32Float ||
       dst_texture_format == WGPUTextureFormat_RG8Unorm ||
       dst_texture_format == WGPUTextureFormat_RG16Float ||
       dst_texture_format == WGPUTextureFormat_RG32Float ||
       dst_texture_format == WGPUTextureFormat_RGBA8Unorm ||
       dst_texture_format == WGPUTextureFormat_BGRA8Unorm ||
       dst_texture_format == WGPUTextureFormat_RGB10A2Unorm ||
       dst_texture_format == WGPUTextureFormat_RGBA16Float ||
       dst_texture_format == WGPUTextureFormat_RGBA32Float)) {
    return true;
  }

  return false;
}

scoped_refptr<Image> GetImageFromExternalImage(
    const V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas*
        external_image,
    ExceptionState& exception_state) {
  CanvasImageSource* source = nullptr;
  CanvasRenderingContextHost* canvas = nullptr;
  switch (external_image->GetContentType()) {
    case V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas::ContentType::
        kHTMLCanvasElement:
      source = external_image->GetAsHTMLCanvasElement();
      canvas = external_image->GetAsHTMLCanvasElement();
      break;
    case V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas::ContentType::
        kImageBitmap:
      source = external_image->GetAsImageBitmap();
      break;
    case V8UnionHTMLCanvasElementOrImageBitmapOrOffscreenCanvas::ContentType::
        kOffscreenCanvas:
      source = external_image->GetAsOffscreenCanvas();
      canvas = external_image->GetAsOffscreenCanvas();
      break;
  }

  // Neutered external image.
  if (source->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "External Image has been detached.");
    return nullptr;
  }

  // Placeholder source is not allowed.
  if (source->IsPlaceholder()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy from a canvas that has had "
                                      "transferControlToOffscreen() called.");
    return nullptr;
  }

  // Canvas element contains cross-origin data and may not be loaded
  if (source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "The external image is tainted by cross-origin data.");
    return nullptr;
  }

  if (canvas && !(canvas->IsWebGL() || canvas->IsRenderingContext2D())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "CopyExternalImageToTexture doesn't support canvas without 2d, webgl "
        "or webgl2 context");
    return nullptr;
  }

  // HTMLCanvasElement and OffscreenCanvas won't care image orientation. But for
  // ImageBitmap, use kRespectImageOrientation will make ElementSize() behave
  // as Size().
  gfx::SizeF image_size = source->ElementSize(
      gfx::SizeF(),  // It will be ignored and won't affect size.
      kRespectImageOrientation);

  // TODO(crbug.com/1197369): Ensure kUnpremultiplyAlpha impl will also make
  // image live on GPU if possible.
  // Use kDontChangeAlpha here to bypass the alpha type conversion here.
  // Left the alpha op to CopyTextureForBrowser() and CopyContentFromCPU().
  // This will help combine more transforms (e.g. flipY, color-space)
  // into a single blit.
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  auto image = source->GetSourceImageForCanvas(&source_image_status, image_size,
                                               kDontChangeAlpha);
  if (source_image_status != kNormalSourceImageStatus) {
    // Canvas back resource is broken, zero size, incomplete or invalid.
    // but developer can do nothing. Return nullptr and issue an noop.
    return nullptr;
  }

  return image;
}

}  // namespace

GPUQueue::GPUQueue(GPUDevice* device, WGPUQueue queue)
    : DawnObject<WGPUQueue>(device, queue) {
}

void GPUQueue::submit(const HeapVector<Member<GPUCommandBuffer>>& buffers) {
  std::unique_ptr<WGPUCommandBuffer[]> commandBuffers = AsDawnType(buffers);

  GetProcs().queueSubmit(GetHandle(), buffers.size(), commandBuffers.get());
  // WebGPU guarantees that submitted commands finish in finite time so we
  // need to ensure commands are flushed. Flush immediately so the GPU process
  // eagerly processes commands to maximize throughput.
  FlushNow();
}

void GPUQueue::OnWorkDoneCallback(ScriptPromiseResolver* resolver,
                                  WGPUQueueWorkDoneStatus status) {
  switch (status) {
    case WGPUQueueWorkDoneStatus_Success:
      resolver->Resolve();
      break;
    case WGPUQueueWorkDoneStatus_Error:
    case WGPUQueueWorkDoneStatus_Unknown:
    case WGPUQueueWorkDoneStatus_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GPUQueue::onSubmittedWorkDone(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindDawnOnceCallback(&GPUQueue::OnWorkDoneCallback, WrapPersistent(this),
                           WrapPersistent(resolver));

  GetProcs().queueOnSubmittedWorkDone(
      GetHandle(), 0u, callback->UnboundCallback(), callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
  return promise;
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_element_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->byteLength(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_element_offset, {}, exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_element_offset,
                           uint64_t data_element_count,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->byteLength(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_element_offset, data_element_count, exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->ByteLength(),
                  data->DataMaybeShared(), 1, data_byte_offset, {},
                  exception_state);
}

void GPUQueue::writeBuffer(GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           uint64_t byte_size,
                           ExceptionState& exception_state) {
  WriteBufferImpl(buffer, buffer_offset, data->ByteLength(),
                  data->DataMaybeShared(), 1, data_byte_offset, byte_size,
                  exception_state);
}

void GPUQueue::WriteBufferImpl(GPUBuffer* buffer,
                               uint64_t buffer_offset,
                               uint64_t data_byte_length,
                               const void* data_base_ptr,
                               unsigned data_bytes_per_element,
                               uint64_t data_element_offset,
                               absl::optional<uint64_t> data_element_count,
                               ExceptionState& exception_state) {
  if (buffer_offset % 4 != 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Buffer offset must be a multiple of 4");
    return;
  }

  CHECK_LE(data_bytes_per_element, 8u);

  if (data_element_offset > data_byte_length / data_bytes_per_element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Data offset is too large");
    return;
  }

  uint64_t data_byte_offset = data_element_offset * data_bytes_per_element;
  uint64_t max_write_size = data_byte_length - data_byte_offset;

  uint64_t write_byte_size = max_write_size;
  if (data_element_count.has_value()) {
    if (data_element_count.value() > max_write_size / data_bytes_per_element) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          "Number of bytes to write is too large");
      return;
    }
    write_byte_size = data_element_count.value() * data_bytes_per_element;
  }
  if (write_byte_size % 4 != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Number of bytes to write must be a multiple of 4");
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
  EnsureFlush();
}

void GPUQueue::writeTexture(GPUImageCopyTexture* destination,
                            const MaybeShared<DOMArrayBufferView>& data,
                            GPUImageDataLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(destination, data->BaseAddressMaybeShared(),
                   data->byteLength(), data_layout, write_size,
                   exception_state);
}

void GPUQueue::writeTexture(GPUImageCopyTexture* destination,
                            const DOMArrayBufferBase* data,
                            GPUImageDataLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(destination, data->DataMaybeShared(), data->ByteLength(),
                   data_layout, write_size, exception_state);
}

void GPUQueue::WriteTextureImpl(GPUImageCopyTexture* destination,
                                const void* data,
                                size_t data_size,
                                GPUImageDataLayout* data_layout,
                                const V8GPUExtent3D* write_size,
                                ExceptionState& exception_state) {
  WGPUExtent3D dawn_write_size = AsDawnType(write_size);
  WGPUImageCopyTexture dawn_destination = AsDawnType(destination, device_);

  WGPUTextureDataLayout dawn_data_layout = {};
  {
    const char* error =
        ValidateTextureDataLayout(data_layout, &dawn_data_layout);
    if (error) {
      device_->InjectError(WGPUErrorType_Validation, error);
      return;
    }
  }

  GetProcs().queueWriteTexture(GetHandle(), &dawn_destination, data, data_size,
                               &dawn_data_layout, &dawn_write_size);
  EnsureFlush();
  return;
}

void GPUQueue::copyExternalImageToTexture(
    GPUImageCopyExternalImage* copyImage,
    GPUImageCopyTextureTagged* destination,
    const V8GPUExtent3D* copy_size,
    ExceptionState& exception_state) {
  // "srgb" is the only valid color space for now.
  DCHECK_EQ(destination->colorSpace(), "srgb");

  // TODO(crbug.com/1257856): Current implementation takes wrong flip step for
  // WebGL canvas. It should follow the canvas origin but it follows WebGL
  // coords instead. Use the temporary origin config for WebGL canvas so user
  // could fix the flip issue.
  bool copy_origin_is_bottom_left =
      copyImage->temporaryOriginBottomLeftIfWebGL() &&
      IsExternalImageWebGLCanvas(copyImage->source());

  if (copy_origin_is_bottom_left) {
    device_->AddConsoleWarning(
        "temporaryOriginBottomLeftIfWebGL is true means the top-left pixel in "
        "destination gpu texture is from"
        "bottom-left pixel of WebGL Canvas. Set "
        "temporaryOriginBottomLeftIfWebGL to false to unflip the result.");
  }

  scoped_refptr<Image> image =
      GetImageFromExternalImage(copyImage->source(), exception_state);

  scoped_refptr<StaticBitmapImage> static_bitmap_image =
      DynamicTo<StaticBitmapImage>(image.get());
  if (!static_bitmap_image) {
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): Browser fails extracting valid resource"
        "from external image. This API call will return early.");
    return;
  }

  // TODO(crbug.com/1197369): Extract alpha info and config the following
  // CopyContentFromCPU() and CopyContentFromGPU().

  WGPUExtent3D dawn_copy_size = AsDawnType(copy_size);

  // Extract source origin
  WGPUOrigin3D origin_in_external_image =
      GPUOrigin2DToWGPUOrigin3D(copyImage->origin());

  // Validate origin value
  const bool copyRectOutOfBounds =
      static_cast<uint32_t>(static_bitmap_image->width()) <
          origin_in_external_image.x ||
      static_cast<uint32_t>(static_bitmap_image->height()) <
          origin_in_external_image.y ||
      static_cast<uint32_t>(static_bitmap_image->width()) -
              origin_in_external_image.x <
          dawn_copy_size.width ||
      static_cast<uint32_t>(static_bitmap_image->height()) -
              origin_in_external_image.y <
          dawn_copy_size.height;

  if (copyRectOutOfBounds) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Copy rect is out of bounds of external image");
    return;
  }

  // Check copy depth.
  // the validation rule is origin.z + copy_size.depth <= 1.
  // Since origin in external image is 2D Origin(z always equals to 0),
  // checks copy size here only.
  if (dawn_copy_size.depthOrArrayLayers > 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Copy depth is out of bounds of external image.");
    return;
  }

  WGPUImageCopyTexture dawn_destination = AsDawnType(destination, device_);

  if (!IsValidExternalImageDestinationFormat(
          destination->texture()->Format())) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "Invalid destination gpu texture format.");
    return;
  }

  if (destination->texture()->Dimension() != WGPUTextureDimension_2D) {
    GetProcs().deviceInjectError(device_->GetHandle(), WGPUErrorType_Validation,
                                 "Dst gpu texture must be 2d.");
    return;
  }

  WGPUTextureUsage dst_texture_usage = destination->texture()->Usage();

  if ((dst_texture_usage & WGPUTextureUsage_RenderAttachment) !=
          WGPUTextureUsage_RenderAttachment ||
      (dst_texture_usage & WGPUTextureUsage_CopyDst) !=
          WGPUTextureUsage_CopyDst) {
    GetProcs().deviceInjectError(
        device_->GetHandle(), WGPUErrorType_Validation,
        "Destination texture needs to have CopyDst and RenderAttachment "
        "usage.");
    return;
  }

  // Issue the noop copy to continue validation to destination textures
  if (dawn_copy_size.width == 0 || dawn_copy_size.height == 0 ||
      dawn_copy_size.depthOrArrayLayers == 0) {
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): It is a noop copy"
        "({width|height|depthOrArrayLayers} equals to 0).");
  }

  // Try GPU path first and delegate noop copy to CPU path.
  if (static_bitmap_image->IsTextureBacked()) {  // Try GPU uploading path.
    if (CopyContentFromGPU(static_bitmap_image.get(), origin_in_external_image,
                           dawn_copy_size, dawn_destination,
                           destination->texture()->Format(),
                           destination->premultipliedAlpha(),
                           static_bitmap_image->IsOriginTopLeft() ==
                               copy_origin_is_bottom_left)) {
      return;
    }
  }
  // GPU path failed, fallback to CPU path
  static_bitmap_image = static_bitmap_image->MakeUnaccelerated();
  DCHECK_EQ(static_bitmap_image->IsOriginTopLeft(), true);

  // CPU path is the fallback path and should always work.
  if (!CopyContentFromCPU(
          static_bitmap_image.get(), origin_in_external_image, dawn_copy_size,
          dawn_destination, destination->texture()->Format(),
          destination->premultipliedAlpha(), copy_origin_is_bottom_left)) {
    exception_state.ThrowTypeError(
        "Failed to copy content from external image.");
    return;
  }
}

// TODO(crbug.com/1197369): This API contains duplicated code is to stop
// breaking current workable codes. Will be removed when it is deprecated.
void GPUQueue::copyImageBitmapToTexture(GPUImageCopyImageBitmap* source,
                                        GPUImageCopyTexture* destination,
                                        const V8GPUExtent3D* copy_size,
                                        ExceptionState& exception_state) {
  device_->AddConsoleWarning(
      "The copyImageBitmapToTexture() has been deprecated in favor of the "
      "copyExternalImageToTexture() "
      "and will soon be removed.");

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

  WGPUExtent3D dawn_copy_size = AsDawnType(copy_size);

  // Extract imageBitmap attributes
  WGPUOrigin3D origin_in_image_bitmap =
      GPUOrigin2DToWGPUOrigin3D(source->origin());

  // Validate copy depth
  if (dawn_copy_size.depthOrArrayLayers > 1) {
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

  WGPUImageCopyTexture dawn_destination = AsDawnType(destination, device_);

  if (!IsValidCopyIB2TDestinationFormat(destination->texture()->Format())) {
    return exception_state.ThrowTypeError("Invalid gpu texture format.");
  }

  bool isNoopCopy = dawn_copy_size.width == 0 || dawn_copy_size.height == 0 ||
                    dawn_copy_size.depthOrArrayLayers == 0;

  if (image->IsTextureBacked() && !isNoopCopy) {  // Try GPU uploading path.
    // Fallback to CPU path, GPU uploading requests RENDER_ATTACHMENT usage for
    // dst texture.
    image = image->MakeUnaccelerated();
  }
  // CPU path is the fallback path and should always work.
  if (!CopyContentFromCPU(image.get(), origin_in_image_bitmap, dawn_copy_size,
                          dawn_destination, destination->texture()->Format(),
                          image->IsPremultiplied())) {
    exception_state.ThrowTypeError("Failed to copy content from imageBitmap.");
    return;
  }
}

bool GPUQueue::CopyContentFromCPU(StaticBitmapImage* image,
                                  const WGPUOrigin3D& origin,
                                  const WGPUExtent3D& copy_size,
                                  const WGPUImageCopyTexture& destination,
                                  const WGPUTextureFormat dest_texture_format,
                                  bool premultiplied_alpha,
                                  bool flipY) {
  // Prepare for uploading CPU data.
  gfx::Rect image_data_rect(origin.x, origin.y, copy_size.width,
                            copy_size.height);

  WebGPUImageUploadSizeInfo info = ComputeImageBitmapWebGPUUploadSizeInfo(
      image_data_rect, dest_texture_format);

  bool isNoopCopy =
      info.size_in_bytes == 0 || copy_size.depthOrArrayLayers == 0;

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
            image_data_rect, dest_texture_format, premultiplied_alpha, flipY)) {
      // Release the buffer.
      GetProcs().bufferRelease(buffer);
      return false;
    }

    GetProcs().bufferUnmap(buffer);
  }

  // Start a B2T copy to move contents from buffer to destination texture
  WGPUImageCopyBuffer dawn_intermediate = {};
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
                                  const WGPUImageCopyTexture& destination,
                                  const WGPUTextureFormat dest_texture_format,
                                  bool premultiplied_alpha,
                                  bool flipY) {
  // Check src/dst texture formats are supported by CopyTextureForBrowser
  SkImageInfo image_info = image->PaintImageForCurrentFrame().GetSkImageInfo();
  if (!IsValidCopyTextureForBrowserFormats(image_info.colorType(),
                                           dest_texture_format)) {
    return false;
  }

  // Keep mailbox generation in noop copy to catch possible issue.
  // TODO(crbug.com/1197369): config color space based on image
  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromStaticBitmapImage(
          GetDawnControlClient(), device_->GetHandle(),
          static_cast<WGPUTextureUsage>(WGPUTextureUsage_CopyDst |
                                        WGPUTextureUsage_CopySrc |
                                        WGPUTextureUsage_TextureBinding),
          image, CanvasColorSpace::kSRGB, image_info.colorType());

  // Fail to associate staticBitmapImage to dawn resource.
  if (!mailbox_texture) {
    return false;
  }

  WGPUTexture src_texture = mailbox_texture->GetTexture();
  DCHECK(src_texture != nullptr);

  WGPUImageCopyTexture src = {};
  src.texture = src_texture;
  src.origin = origin;

  WGPUCopyTextureForBrowserOptions options = {};

  if (flipY) {
    options.flipY = true;
  }

  options.alphaOp = image->IsPremultiplied() == premultiplied_alpha
                        ? WGPUAlphaOp_DontChange
                        : premultiplied_alpha ? WGPUAlphaOp_Premultiply
                                              : WGPUAlphaOp_Unpremultiply;

  GetProcs().queueCopyTextureForBrowser(GetHandle(), &src, &destination,
                                        &copy_size, &options);

  return true;
}

}  // namespace blink
