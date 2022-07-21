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
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/texture_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
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
          [[fallthrough]];
        case 1:
          dawn_origin.x = webgpu_origin_sequence[0];
          [[fallthrough]];
        case 0:
          break;
      }
      break;
    }
  }

  return dawn_origin;
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

WGPUTextureFormat SkColorTypeToDawnColorFormat(SkColorType sk_color_type) {
  switch (sk_color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return WGPUTextureFormat_RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return WGPUTextureFormat_BGRA8Unorm;
    default:
      NOTREACHED();
      return WGPUTextureFormat_Undefined;
  }
}

static constexpr uint64_t kDawnBytesPerRowAlignmentBits = 8;

// Calculate bytes per row for T2B/B2T copy
// TODO(shaobo.yan@intel.com): Using Dawn's constants once they are exposed
uint64_t AlignBytesPerRow(uint64_t bytesPerRow) {
  return (((bytesPerRow - 1) >> kDawnBytesPerRowAlignmentBits) + 1)
         << kDawnBytesPerRowAlignmentBits;
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

  if (canvas && !(canvas->IsWebGL() || canvas->IsRenderingContext2D() ||
                  canvas->IsWebGPU())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "CopyExternalImageToTexture doesn't support canvas without 2d, webgl,"
        " webgl2 or webgpu context");
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

// CopyExternalImageToTexture() will always copy from the top-left corner of the
// back resource. But whether do flipY is decided by the source
// StaticBitmapImage origin.
// The StaticImageBitmap is a container that specifies an orientation for its
// content, but the content of StaticImageBitmap has its own orientation so we
// need to take both into account.
bool IsExternalImageOriginTopLeft(StaticBitmapImage* static_bitmap_image) {
  bool frameTopLeft =
      static_bitmap_image->CurrentFrameOrientation().Orientation() ==
      ImageOrientationEnum::kOriginTopLeft;
  return frameTopLeft == static_bitmap_image->IsOriginTopLeft();
}

}  // namespace

GPUQueue::GPUQueue(GPUDevice* device, WGPUQueue queue)
    : DawnObject<WGPUQueue>(device, queue) {}

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
      BindWGPUOnceCallback(&GPUQueue::OnWorkDoneCallback, WrapPersistent(this),
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
  WGPUImageCopyTexture dawn_destination = AsDawnType(destination);

  WGPUTextureDataLayout dawn_data_layout = {};
  {
    const char* error =
        ValidateTextureDataLayout(data_layout, &dawn_data_layout);
    if (error) {
      device_->InjectError(WGPUErrorType_Validation, error);
      return;
    }
  }

  if (dawn_data_layout.offset > data_size) {
    device_->InjectError(WGPUErrorType_Validation, "Data offset is too large");
    return;
  }

  // Handle the data layout offset by offsetting the data pointer instead. This
  // helps move less data between then renderer and GPU process (otherwise all
  // the data from 0 to offset would be copied over as well).
  const void* data_ptr =
      static_cast<const uint8_t*>(data) + dawn_data_layout.offset;
  data_size -= dawn_data_layout.offset;
  dawn_data_layout.offset = 0;

  // Compute a tight upper bound of the number of bytes to send for this
  // WriteTexture. This can be 0 for some cases that produce validation errors,
  // but we don't create an error in Blink since Dawn can produce better error
  // messages (and this is more up-to-spec because the errors must be created on
  // the device timeline).
  size_t data_size_upper_bound = EstimateWriteTextureBytesUpperBound(
      dawn_data_layout, dawn_write_size, destination->texture()->Format(),
      dawn_destination.aspect);
  size_t required_copy_size = std::min(data_size, data_size_upper_bound);

  GetProcs().queueWriteTexture(GetHandle(), &dawn_destination, data_ptr,
                               required_copy_size, &dawn_data_layout,
                               &dawn_write_size);
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

  WGPUImageCopyTexture dawn_destination = AsDawnType(destination);

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

  PredefinedColorSpace color_space;
  if (!ValidateAndConvertColorSpace(destination->colorSpace(), color_space,
                                    exception_state)) {
    return;
  }

  // Issue the noop copy to continue validation to destination textures
  if (dawn_copy_size.width == 0 || dawn_copy_size.height == 0 ||
      dawn_copy_size.depthOrArrayLayers == 0) {
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): It is a noop copy"
        "({width|height|depthOrArrayLayers} equals to 0).");
  }

  if (!UploadContentToTexture(
          static_bitmap_image.get(), origin_in_external_image, dawn_copy_size,
          dawn_destination, destination->premultipliedAlpha(), color_space,
          copyImage->flipY())) {
    exception_state.ThrowTypeError(
        "Failed to copy content from external image.");
    return;
  }
}

bool GPUQueue::UploadContentToTexture(StaticBitmapImage* image,
                                      const WGPUOrigin3D& origin,
                                      const WGPUExtent3D& copy_size,
                                      const WGPUImageCopyTexture& destination,
                                      bool dst_premultiplied_alpha,
                                      PredefinedColorSpace dst_color_space,
                                      bool flipY) {
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  SkColorType source_color_type = paint_image.GetSkImageInfo().colorType();

  // TODO(crbug.com/dawn/856): Expand copyTextureForBrowser to support any
  // non-depth, non-stencil, non-compressed texture format pair copy.
  if (source_color_type != SkColorType::kRGBA_8888_SkColorType &&
      source_color_type != SkColorType::kBGRA_8888_SkColorType) {
    return false;
  }

  // Set options for CopyTextureForBrowser except flipY. The possible
  // image->MakeUnaccelerated() call changes the source orientation, so we set
  // the flipY option when we need to issue CopyTextureForBrowser() to ensure
  // the correctness.
  WGPUCopyTextureForBrowserOptions options = {};

  options.srcAlphaMode = image->IsPremultiplied()
                             ? WGPUAlphaMode_Premultiplied
                             : WGPUAlphaMode_Unpremultiplied;
  options.dstAlphaMode = dst_premultiplied_alpha
                             ? WGPUAlphaMode_Premultiplied
                             : WGPUAlphaMode_Unpremultiplied;

  // Set color space conversion params
  sk_sp<SkColorSpace> sk_src_color_space =
      paint_image.GetSkImageInfo().refColorSpace();

  // If source input discard the color space info(e.g. ImageBitmap created with
  // flag colorSpaceConversion: none). Treat the source color space as sRGB.
  if (sk_src_color_space == nullptr) {
    sk_src_color_space = SkColorSpace::MakeSRGB();
  }
  sk_sp<SkColorSpace> sk_dst_color_space =
      PredefinedColorSpaceToSkColorSpace(dst_color_space);
  std::array<float, 7> gamma_decode_params;
  std::array<float, 7> gamma_encode_params;
  std::array<float, 9> conversion_matrix;
  if (!SkColorSpace::Equals(sk_src_color_space.get(),
                            sk_dst_color_space.get())) {
    skcms_TransferFunction src_transfer_fn = {};
    skcms_TransferFunction dst_transfer_fn = {};

    // Row major matrix
    skcms_Matrix3x3 transfer_matrix = {};

    sk_src_color_space->transferFn(&src_transfer_fn);
    sk_dst_color_space->invTransferFn(&dst_transfer_fn);
    sk_src_color_space->gamutTransformTo(sk_dst_color_space.get(),
                                         &transfer_matrix);
    gamma_decode_params = {src_transfer_fn.g, src_transfer_fn.a,
                           src_transfer_fn.b, src_transfer_fn.c,
                           src_transfer_fn.d, src_transfer_fn.e,
                           src_transfer_fn.f};
    gamma_encode_params = {dst_transfer_fn.g, dst_transfer_fn.a,
                           dst_transfer_fn.b, dst_transfer_fn.c,
                           dst_transfer_fn.d, dst_transfer_fn.e,
                           dst_transfer_fn.f};

    // From row major matrix to col major matrix
    conversion_matrix = {transfer_matrix.vals[0][0], transfer_matrix.vals[1][0],
                         transfer_matrix.vals[2][0], transfer_matrix.vals[0][1],
                         transfer_matrix.vals[1][1], transfer_matrix.vals[2][1],
                         transfer_matrix.vals[0][2], transfer_matrix.vals[1][2],
                         transfer_matrix.vals[2][2]};

    options.needsColorSpaceConversion = true;
    options.srcTransferFunctionParameters = gamma_decode_params.data();
    options.dstTransferFunctionParameters = gamma_encode_params.data();
    options.conversionMatrix = conversion_matrix.data();
  }

  // Handling GPU resource.
  if (image->IsTextureBacked()) {
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromStaticBitmapImage(
            GetDawnControlClient(), device_->GetHandle(),
            static_cast<WGPUTextureUsage>(WGPUTextureUsage_CopyDst |
                                          WGPUTextureUsage_CopySrc |
                                          WGPUTextureUsage_TextureBinding),
            image, source_color_type);

    if (mailbox_texture != nullptr) {
      WGPUImageCopyTexture src = {};
      src.texture = mailbox_texture->GetTexture();
      src.origin = origin;

      bool is_external_image_origin_top_left =
          IsExternalImageOriginTopLeft(image);
      options.flipY = (is_external_image_origin_top_left == flipY);

      GetProcs().queueCopyTextureForBrowser(GetHandle(), &src, &destination,
                                            &copy_size, &options);
      return true;
    }
  }

  // Call MakeUnaccelerated() to ensure image is CPU back resource.
  // This path will handle all CPU backend resource StaticBitmapImage or the one
  // with texture backed resource but fail to associate staticBitmapImage to
  // dawn resource.
  scoped_refptr<StaticBitmapImage> unaccelerated_image =
      image->MakeUnaccelerated();
  image = unaccelerated_image.get();

  // Handling CPU resource.
  // Source type is SkColorType::kRGBA_8888_SkColorType or
  // SkColorType::kBGRA_8888_SkColorType.
  uint64_t bytes_per_pixel = 4;

  gfx::Rect source_image_rect = image->Rect();
  base::CheckedNumeric<uint32_t> bytes_per_row =
      AlignBytesPerRow(image->width() * bytes_per_pixel);

  // Static cast to uint64_t to catch overflow during multiplications and use
  // base::CheckedNumeric to catch this overflow.
  base::CheckedNumeric<size_t> size_in_bytes =
      bytes_per_row * static_cast<uint64_t>(image->height());

  // Overflow happens when calculating size or row bytes.
  if (!size_in_bytes.IsValid()) {
    return false;
  }

  uint32_t wgpu_bytes_per_row = bytes_per_row.ValueOrDie();

  // Create a mapped buffer to receive external image contents
  WGPUBufferDescriptor buffer_desc = {};
  buffer_desc.usage = WGPUBufferUsage_CopySrc;
  buffer_desc.size = size_in_bytes.ValueOrDie();
  buffer_desc.mappedAtCreation = true;

  WGPUBuffer intermediate_buffer =
      GetProcs().deviceCreateBuffer(device_->GetHandle(), &buffer_desc);

  size_t size = static_cast<size_t>(buffer_desc.size);
  void* data = GetProcs().bufferGetMappedRange(intermediate_buffer, 0, size);

  auto dest_pixels = base::span<uint8_t>(static_cast<uint8_t*>(data), size);

  bool success = paint_image.readPixels(
      paint_image.GetSkImageInfo(), dest_pixels.data(), wgpu_bytes_per_row,
      source_image_rect.x(), source_image_rect.y());
  if (!success) {
    // Release the buffer.
    GetProcs().bufferRelease(intermediate_buffer);
    return false;
  }

  GetProcs().bufferUnmap(intermediate_buffer);

  uint32_t source_image_width = static_cast<uint32_t>(image->width());
  uint32_t source_image_height = static_cast<uint32_t>(image->height());

  // Create intermediate texture as input for CopyTextureForBrowser().
  WGPUTextureDescriptor texture_desc = {};
  texture_desc.usage = WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
                       WGPUTextureUsage_TextureBinding;
  texture_desc.dimension = WGPUTextureDimension_2D;
  texture_desc.size = {source_image_width, source_image_height, 1};
  texture_desc.format = SkColorTypeToDawnColorFormat(source_color_type);
  texture_desc.mipLevelCount = 1;
  texture_desc.sampleCount = 1;

  WGPUTexture intermediate_texture =
      GetProcs().deviceCreateTexture(device_->GetHandle(), &texture_desc);

  // Start a B2T copy to move contents from buffer to intermediate texture
  WGPUImageCopyBuffer dawn_intermediate_buffer = {};
  dawn_intermediate_buffer.buffer = intermediate_buffer;
  dawn_intermediate_buffer.layout.bytesPerRow = wgpu_bytes_per_row;
  dawn_intermediate_buffer.layout.rowsPerImage = source_image_height;

  WGPUImageCopyTexture dawn_intermediate_texture = {};
  dawn_intermediate_texture.texture = intermediate_texture;
  dawn_intermediate_texture.aspect = WGPUTextureAspect_All;

  WGPUExtent3D source_image_copy_size = {source_image_width,
                                         source_image_height, 1};

  WGPUCommandEncoder encoder =
      GetProcs().deviceCreateCommandEncoder(device_->GetHandle(), nullptr);
  GetProcs().commandEncoderCopyBufferToTexture(
      encoder, &dawn_intermediate_buffer, &dawn_intermediate_texture,
      &source_image_copy_size);
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, nullptr);

  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);
  GetProcs().bufferRelease(intermediate_buffer);

  WGPUImageCopyTexture src = {};
  src.texture = intermediate_texture;
  src.origin = origin;

  // MakeUnaccelerated() call might change the StaticBitmapImage orientation so
  // we need to query the orientation again.
  bool is_external_image_origin_top_left = IsExternalImageOriginTopLeft(image);
  options.flipY = (is_external_image_origin_top_left == flipY);
  GetProcs().queueCopyTextureForBrowser(GetHandle(), &src, &destination,
                                        &copy_size, &options);

  // Release intermediate texture.
  GetProcs().textureRelease(intermediate_texture);
  return true;
}
}  // namespace blink
