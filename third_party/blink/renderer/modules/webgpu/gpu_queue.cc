// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_external_image.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_image_bitmap.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture_tagged.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_videoframe.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/texture_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/image_extractor.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

bool IsValidExternalImageDestinationFormat(
    wgpu::TextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case wgpu::TextureFormat::R8Unorm:
    case wgpu::TextureFormat::R16Float:
    case wgpu::TextureFormat::R32Float:
    case wgpu::TextureFormat::RG8Unorm:
    case wgpu::TextureFormat::RG16Float:
    case wgpu::TextureFormat::RG32Float:
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
    case wgpu::TextureFormat::RGB10A2Unorm:
    case wgpu::TextureFormat::RGBA16Float:
    case wgpu::TextureFormat::RGBA32Float:
      return true;
    default:
      return false;
  }
}

wgpu::TextureFormat SkColorTypeToDawnColorFormat(SkColorType sk_color_type) {
  switch (sk_color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return wgpu::TextureFormat::RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return wgpu::TextureFormat::BGRA8Unorm;
    default:
      NOTREACHED();
  }
}

static constexpr uint64_t kDawnBytesPerRowAlignmentBits = 8;

// Calculate bytes per row for T2B/B2T copy
// TODO(shaobo.yan@intel.com): Using Dawn's constants once they are exposed
uint64_t AlignBytesPerRow(uint64_t bytesPerRow) {
  return (((bytesPerRow - 1) >> kDawnBytesPerRowAlignmentBits) + 1)
         << kDawnBytesPerRowAlignmentBits;
}

struct ExternalSource {
  ExternalTextureSource external_texture_source;
  scoped_refptr<StaticBitmapImage> image = nullptr;
  uint32_t width = 0;
  uint32_t height = 0;
  bool valid = false;
};

struct ExternalImageDstInfo {
  bool premultiplied_alpha;
  PredefinedColorSpace color_space;
};

// TODO(crbug.com/1471372): Avoid extra copy.
scoped_refptr<StaticBitmapImage> GetImageFromImageData(
    const ImageData* image_data) {
  SkPixmap image_data_pixmap = image_data->GetSkPixmap();
  SkImageInfo info = image_data_pixmap.info().makeColorType(kN32_SkColorType);
  size_t image_pixels_size = info.computeMinByteSize();
  if (SkImageInfo::ByteSizeOverflowed(image_pixels_size)) {
    return nullptr;
  }
  sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
  if (!image_pixels) {
    return nullptr;
  }
  if (!image_data_pixmap.readPixels(info, image_pixels->writable_data(),
                                    info.minRowBytes(), 0, 0)) {
    return nullptr;
  }
  return StaticBitmapImage::Create(std::move(image_pixels), info);
}

ExternalSource GetExternalSourceFromExternalImage(
    const V8GPUImageCopyExternalImageSource* external_image,
    const ExternalImageDstInfo& external_image_dst_info,
    ExceptionState& exception_state) {
  ExternalSource external_source;
  ExternalTextureSource external_texture_source;
  CanvasImageSource* canvas_image_source = nullptr;
  CanvasRenderingContextHost* canvas = nullptr;
  VideoFrame* video_frame = nullptr;

  switch (external_image->GetContentType()) {
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLVideoElement:
      external_texture_source = GetExternalTextureSourceFromVideoElement(
          external_image->GetAsHTMLVideoElement(), exception_state);
      if (external_texture_source.valid) {
        external_source.external_texture_source = external_texture_source;
        CHECK(external_texture_source.media_video_frame);

        // Use display size to handle rotated video frame.
        auto media_video_frame = external_texture_source.media_video_frame;

        const auto transform =
            media_video_frame->metadata().transformation.value_or(
                media::kNoTransformation);
        if (transform == media::kNoTransformation ||
            transform.rotation == media::VIDEO_ROTATION_0 ||
            transform.rotation == media::VIDEO_ROTATION_180) {
          external_source.width =
              static_cast<uint32_t>(media_video_frame->natural_size().width());
          external_source.height =
              static_cast<uint32_t>(media_video_frame->natural_size().height());
        } else {
          external_source.width =
              static_cast<uint32_t>(media_video_frame->natural_size().height());
          external_source.height =
              static_cast<uint32_t>(media_video_frame->natural_size().width());
        }
        external_source.valid = true;
      }
      return external_source;
    case V8GPUImageCopyExternalImageSource::ContentType::kVideoFrame:
      video_frame = external_image->GetAsVideoFrame();
      external_texture_source =
          GetExternalTextureSourceFromVideoFrame(video_frame, exception_state);
      if (external_texture_source.valid) {
        external_source.external_texture_source = external_texture_source;
        CHECK(external_texture_source.media_video_frame);
        external_source.width = video_frame->displayWidth();
        external_source.height = video_frame->displayHeight();
        external_source.valid = true;
      }
      return external_source;
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLCanvasElement:
      canvas_image_source = external_image->GetAsHTMLCanvasElement();
      canvas = external_image->GetAsHTMLCanvasElement();
      break;
    case V8GPUImageCopyExternalImageSource::ContentType::kImageBitmap:
      canvas_image_source = external_image->GetAsImageBitmap();
      break;
    case V8GPUImageCopyExternalImageSource::ContentType::kImageData: {
      auto image = GetImageFromImageData(external_image->GetAsImageData());
      if (!image) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "Cannot get image.");
        return external_source;
      }
      external_source.image = image;
      external_source.width = static_cast<uint32_t>(image->width());
      external_source.height = static_cast<uint32_t>(image->height());
      external_source.valid = true;
      return external_source;
    }
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLImageElement:
      canvas_image_source = external_image->GetAsHTMLImageElement();
      break;
    case V8GPUImageCopyExternalImageSource::ContentType::kOffscreenCanvas:
      canvas_image_source = external_image->GetAsOffscreenCanvas();
      canvas = external_image->GetAsOffscreenCanvas();
      break;
  }

  // Neutered external image.
  if (canvas_image_source->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "External Image has been detached.");
    return external_source;
  }

  // Placeholder source is not allowed.
  if (canvas_image_source->IsPlaceholder()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy from a canvas that has had "
                                      "transferControlToOffscreen() called.");
    return external_source;
  }

  // Canvas element contains cross-origin data and may not be loaded
  if (canvas_image_source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "The external image is tainted by cross-origin data.");
    return external_source;
  }

  if (canvas &&
      !(canvas->IsWebGL() || canvas->IsRenderingContext2D() ||
        canvas->IsWebGPU() || canvas->IsImageBitmapRenderingContext())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "CopyExternalImageToTexture doesn't support canvas without rendering "
        "context");
    return external_source;
  }

  // HTMLCanvasElement and OffscreenCanvas won't care image orientation. But for
  // ImageBitmap, use kRespectImageOrientation will make ElementSize() behave
  // as Size().
  gfx::SizeF image_size = canvas_image_source->ElementSize(
      gfx::SizeF(),  // It will be ignored and won't affect size.
      kRespectImageOrientation);

  // TODO(crbug.com/1197369): Ensure kUnpremultiplyAlpha impl will also make
  // image live on GPU if possible.
  // Use kDontChangeAlpha here to bypass the alpha type conversion here.
  // Left the alpha op to CopyTextureForBrowser() and CopyContentFromCPU().
  // This will help combine more transforms (e.g. flipY, color-space)
  // into a single blit.
  SourceImageStatus source_image_status = kInvalidSourceImageStatus;
  auto image_for_canvas = canvas_image_source->GetSourceImageForCanvas(
      FlushReason::kWebGPUExternalImage, &source_image_status, image_size,
      kDontChangeAlpha);
  if (source_image_status != kNormalSourceImageStatus) {
    // Canvas back resource is broken, zero size, incomplete or invalid.
    // but developer can do nothing. Return nullptr and issue an noop.
    return external_source;
  }

  // TODO(crbug.com/1471372): It would be better if GetSourceImageForCanvas()
  // would always return a StaticBitmapImage.
  if (auto* image = DynamicTo<StaticBitmapImage>(image_for_canvas.get())) {
    external_source.image = image;
  } else {
    // HTMLImageElement input
    ImageExtractor image_extractor(image_for_canvas.get(),
                                   external_image_dst_info.premultiplied_alpha,
                                   PredefinedColorSpaceToSkColorSpace(
                                       external_image_dst_info.color_space));
    auto sk_image = image_extractor.GetSkImage();

    if (!sk_image) {
      return external_source;
    }
    // Handle LazyGenerated images.
    if (sk_image->isLazyGenerated()) {
      SkBitmap bitmap;
      auto image_info = sk_image->imageInfo();
      bitmap.allocPixels(image_info, image_info.minRowBytes());
      if (!sk_image->readPixels(bitmap.pixmap(), 0, 0)) {
        return external_source;
      }

      sk_image = SkImages::RasterFromBitmap(bitmap);
    }

    external_source.image = UnacceleratedStaticBitmapImage::Create(
        std::move(sk_image), image_for_canvas->CurrentFrameOrientation());
  }
  external_source.width = static_cast<uint32_t>(external_source.image->width());
  external_source.height =
      static_cast<uint32_t>(external_source.image->height());
  external_source.valid = true;

  return external_source;
}

// CopyExternalImageToTexture() needs to set src/dst AlphaMode, flipY and color
// space conversion related params. This helper function also initializes
// ColorSpaceConversionConstants param.
wgpu::CopyTextureForBrowserOptions CreateCopyTextureForBrowserOptions(
    const StaticBitmapImage* image,
    const PaintImage* paint_image,
    PredefinedColorSpace dst_color_space,
    bool dst_premultiplied_alpha,
    bool flipY,
    ColorSpaceConversionConstants* color_space_conversion_constants) {
  wgpu::CopyTextureForBrowserOptions options = {
      .srcAlphaMode = image->IsPremultiplied()
                          ? wgpu::AlphaMode::Premultiplied
                          : wgpu::AlphaMode::Unpremultiplied,
      .dstAlphaMode = dst_premultiplied_alpha
                          ? wgpu::AlphaMode::Premultiplied
                          : wgpu::AlphaMode::Unpremultiplied,
  };

  // Set color space conversion params
  sk_sp<SkColorSpace> sk_src_color_space =
      paint_image->GetSkImageInfo().refColorSpace();

  // If source input discard the color space info(e.g. ImageBitmap created with
  // flag colorSpaceConversion: none). Treat the source color space as sRGB.
  if (sk_src_color_space == nullptr) {
    sk_src_color_space = SkColorSpace::MakeSRGB();
  }

  gfx::ColorSpace gfx_src_color_space = gfx::ColorSpace(*sk_src_color_space);
  gfx::ColorSpace gfx_dst_color_space =
      PredefinedColorSpaceToGfxColorSpace(dst_color_space);

  *color_space_conversion_constants = GetColorSpaceConversionConstants(
      gfx_src_color_space, gfx_dst_color_space);

  if (gfx_src_color_space != gfx_dst_color_space) {
    options.needsColorSpaceConversion = true;
    options.srcTransferFunctionParameters =
        color_space_conversion_constants->src_transfer_constants.data();
    options.dstTransferFunctionParameters =
        color_space_conversion_constants->dst_transfer_constants.data();
    options.conversionMatrix =
        color_space_conversion_constants->gamut_conversion_matrix.data();
  }
  // The source texture, which is either a WebGPUMailboxTexture for
  // accelerated images or an intermediate texture created for unaccelerated
  // images, is always origin top left, so no additional flip is needed apart
  // from the client specified flip in GPUImageCopyExternalImage i.e. |flipY|.
  options.flipY = flipY;

  return options;
}

// Helper function to get clipped rect from source image. Using in
// CopyExternalImageToTexture().
gfx::Rect GetSourceImageSubrect(StaticBitmapImage* image,
                                gfx::Rect source_image_rect,
                                const wgpu::Origin2D& origin,
                                const wgpu::Extent3D& copy_size) {
  int width = static_cast<int>(copy_size.width);
  int height = static_cast<int>(copy_size.height);
  int x = static_cast<int>(origin.x) + source_image_rect.x();
  int y = static_cast<int>(origin.y) + source_image_rect.y();

  // Ensure generated source image subrect is into source image rect.
  CHECK(width <= source_image_rect.width() - source_image_rect.x() &&
        height <= source_image_rect.height() - source_image_rect.y() &&
        x <= source_image_rect.width() - source_image_rect.x() - width &&
        y <= source_image_rect.height() - source_image_rect.y() - height);

  return gfx::Rect(x, y, width, height);
}

}  // namespace

GPUQueue::GPUQueue(GPUDevice* device, wgpu::Queue queue, const String& label)
    : DawnObject<wgpu::Queue>(device, std::move(queue), label) {}

void GPUQueue::submit(ScriptState* script_state,
                      const HeapVector<Member<GPUCommandBuffer>>& buffers) {
  std::unique_ptr<wgpu::CommandBuffer[]> commandBuffers = AsDawnType(buffers);

  GetHandle().Submit(buffers.size(), commandBuffers.get());
  // WebGPU guarantees that submitted commands finish in finite time so we
  // need to ensure commands are flushed. Flush immediately so the GPU process
  // eagerly processes commands to maximize throughput.
  FlushNow();

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  UseCounter::Count(execution_context, WebFeature::kWebGPUQueueSubmit);
}

void OnWorkDoneCallback(ScriptPromiseResolver<IDLUndefined>* resolver,
                        wgpu::QueueWorkDoneStatus status) {
  switch (status) {
    case wgpu::QueueWorkDoneStatus::Success:
      resolver->Resolve();
      break;
    case wgpu::QueueWorkDoneStatus::Error:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          "Unexpected failure in onSubmittedWorkDone");
      break;
    case wgpu::QueueWorkDoneStatus::Unknown:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          "Unknown failure in onSubmittedWorkDone");
      break;
    case wgpu::QueueWorkDoneStatus::InstanceDropped:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          "Instance dropped in onSubmittedWorkDone");
      break;
    case wgpu::QueueWorkDoneStatus::DeviceLost:
      resolver->RejectWithDOMException(
          DOMExceptionCode::kOperationError,
          "Device lost during onSubmittedWorkDone (do not use this error for "
          "recovery - it is NOT guaranteed to happen on device loss)");
      break;
  }
}

ScriptPromise<IDLUndefined> GPUQueue::onSubmittedWorkDone(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  auto* callback = MakeWGPUOnceCallback(
      resolver->WrapCallbackInScriptScope(WTF::BindOnce(&OnWorkDoneCallback)));

  GetHandle().OnSubmittedWorkDone(wgpu::CallbackMode::AllowSpontaneous,
                                  callback->UnboundCallback(),
                                  callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush(ToEventLoop(script_state));
  return promise;
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_element_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset, data->byteLength(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_element_offset, {}, exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_element_offset,
                           uint64_t data_element_count,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset, data->byteLength(),
                  data->BaseAddressMaybeShared(), data->TypeSize(),
                  data_element_offset, data_element_count, exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset, data->ByteLength(),
                  data->DataMaybeShared(), 1, data_byte_offset, {},
                  exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           uint64_t byte_size,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset, data->ByteLength(),
                  data->DataMaybeShared(), 1, data_byte_offset, byte_size,
                  exception_state);
}

void GPUQueue::WriteBufferImpl(ScriptState* script_state,
                               GPUBuffer* buffer,
                               uint64_t buffer_offset,
                               uint64_t data_byte_length,
                               const void* data_base_ptr,
                               unsigned data_bytes_per_element,
                               uint64_t data_element_offset,
                               std::optional<uint64_t> data_element_count,
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
  GetHandle().WriteBuffer(buffer->GetHandle(), buffer_offset, data_ptr,
                          static_cast<size_t>(write_byte_size));
  EnsureFlush(ToEventLoop(script_state));
}

void GPUQueue::writeTexture(ScriptState* script_state,
                            GPUImageCopyTexture* destination,
                            const MaybeShared<DOMArrayBufferView>& data,
                            GPUImageDataLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(script_state, destination, data->BaseAddressMaybeShared(),
                   data->byteLength(), data_layout, write_size,
                   exception_state);
}

void GPUQueue::writeTexture(ScriptState* script_state,
                            GPUImageCopyTexture* destination,
                            const DOMArrayBufferBase* data,
                            GPUImageDataLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(script_state, destination, data->DataMaybeShared(),
                   data->ByteLength(), data_layout, write_size,
                   exception_state);
}

void GPUQueue::WriteTextureImpl(ScriptState* script_state,
                                GPUImageCopyTexture* destination,
                                const void* data,
                                size_t data_size,
                                GPUImageDataLayout* data_layout,
                                const V8GPUExtent3D* write_size,
                                ExceptionState& exception_state) {
  wgpu::Extent3D dawn_write_size;
  wgpu::ImageCopyTexture dawn_destination;
  if (!ConvertToDawn(write_size, &dawn_write_size, device_, exception_state) ||
      !ConvertToDawn(destination, &dawn_destination, exception_state)) {
    return;
  }

  wgpu::TextureDataLayout dawn_data_layout = {};
  {
    const char* error =
        ValidateTextureDataLayout(data_layout, &dawn_data_layout);
    if (error) {
      device_->InjectError(wgpu::ErrorType::Validation, error);
      return;
    }
  }

  if (dawn_data_layout.offset > data_size) {
    device_->InjectError(wgpu::ErrorType::Validation,
                         "Data offset is too large");
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

  GetHandle().WriteTexture(&dawn_destination, data_ptr, required_copy_size,
                           &dawn_data_layout, &dawn_write_size);
  EnsureFlush(ToEventLoop(script_state));
  return;
}

void GPUQueue::copyExternalImageToTexture(
    GPUImageCopyExternalImage* copyImage,
    GPUImageCopyTextureTagged* destination,
    const V8GPUExtent3D* copy_size,
    ExceptionState& exception_state) {

  // Extract color space info before getting source image to handle some
  // redecoded cases like ImageElement.
  PredefinedColorSpace color_space;
  if (!ValidateAndConvertColorSpace(destination->colorSpace(), color_space,
                                    exception_state)) {
    return;
  }

  ExternalSource source = GetExternalSourceFromExternalImage(
      copyImage->source(), {destination->premultipliedAlpha(), color_space},
      exception_state);
  if (!source.valid) {
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): Browser fails extracting valid resource"
        "from external image. This API call will return early.");
    return;
  }

  wgpu::Extent3D dawn_copy_size;
  wgpu::Origin2D origin_in_external_image;
  wgpu::ImageCopyTexture dawn_destination;
  if (!ConvertToDawn(copy_size, &dawn_copy_size, device_, exception_state) ||
      !ConvertToDawn(copyImage->origin(), &origin_in_external_image,
                     exception_state) ||
      !ConvertToDawn(destination, &dawn_destination, exception_state)) {
    return;
  }

  const bool copyRectOutOfBounds =
      source.width < origin_in_external_image.x ||
      source.height < origin_in_external_image.y ||
      source.width - origin_in_external_image.x < dawn_copy_size.width ||
      source.height - origin_in_external_image.y < dawn_copy_size.height;

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

  if (!IsValidExternalImageDestinationFormat(
          destination->texture()->Format())) {
    device_->GetHandle().InjectError(wgpu::ErrorType::Validation,
                                     "Invalid destination gpu texture format.");
    return;
  }

  if (destination->texture()->Dimension() != wgpu::TextureDimension::e2D) {
    device_->GetHandle().InjectError(wgpu::ErrorType::Validation,
                                     "Dst gpu texture must be 2d.");
    return;
  }

  wgpu::TextureUsage dst_texture_usage = destination->texture()->Usage();

  if ((dst_texture_usage & wgpu::TextureUsage::RenderAttachment) !=
          wgpu::TextureUsage::RenderAttachment ||
      (dst_texture_usage & wgpu::TextureUsage::CopyDst) !=
          wgpu::TextureUsage::CopyDst) {
    device_->GetHandle().InjectError(
        wgpu::ErrorType::Validation,
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

  if (source.external_texture_source.valid) {
    // Use display size which is based on natural size but considering
    // transformation metadata.
    wgpu::Extent2D video_frame_display_size = {source.width, source.height};
    CopyFromVideoElement(
        source.external_texture_source, video_frame_display_size,
        origin_in_external_image, dawn_copy_size, dawn_destination,
        destination->premultipliedAlpha(), color_space, copyImage->flipY());
    return;
  }

  if (!CopyFromCanvasSourceImage(source.image.get(), origin_in_external_image,
                                 dawn_copy_size, dawn_destination,
                                 destination->premultipliedAlpha(), color_space,
                                 copyImage->flipY())) {
    exception_state.ThrowTypeError(
        "Failed to copy content from external image.");
    return;
  }
}

void GPUQueue::CopyFromVideoElement(
    const ExternalTextureSource source,
    const wgpu::Extent2D& video_frame_natural_size,
    const wgpu::Origin2D& origin,
    const wgpu::Extent3D& copy_size,
    const wgpu::ImageCopyTexture& destination,
    bool dst_premultiplied_alpha,
    PredefinedColorSpace dst_color_space,
    bool flipY) {
  CHECK(source.valid);

  // Create External Texture with dst color space. No color space conversion
  // happens during copy step.
  ExternalTexture external_texture =
      CreateExternalTexture(device_, dst_color_space, source.media_video_frame,
                            source.video_renderer);

  wgpu::CopyTextureForBrowserOptions options = {
      // Extracting contents from HTMLVideoElement (e.g.
      // CreateStaticBitmapImage(),
      // GetSourceImageForCanvas) always assume alpha mode as premultiplied.
      // Keep this assumption here.
      .srcAlphaMode = wgpu::AlphaMode::Premultiplied,
      .dstAlphaMode = dst_premultiplied_alpha
                          ? wgpu::AlphaMode::Premultiplied
                          : wgpu::AlphaMode::Unpremultiplied,
  };

  options.flipY = flipY;

  wgpu::ImageCopyExternalTexture src = {
      .externalTexture = external_texture.wgpu_external_texture,
      .origin = {origin.x, origin.y},
      .naturalSize = video_frame_natural_size,
  };
  GetHandle().CopyExternalTextureForBrowser(&src, &destination, &copy_size,
                                            &options);
}

bool GPUQueue::CopyFromCanvasSourceImage(
    StaticBitmapImage* image,
    const wgpu::Origin2D& origin,
    const wgpu::Extent3D& copy_size,
    const wgpu::ImageCopyTexture& destination,
    bool dst_premultiplied_alpha,
    PredefinedColorSpace dst_color_space,
    bool flipY) {
  // If GPU backed image failed to uploading through GPU, call
  // MakeUnaccelerated() to generate CPU backed image and fallback to CPU
  // uploading path.
  scoped_refptr<StaticBitmapImage> unaccelerated_image = nullptr;
  bool use_webgpu_mailbox_texture = true;

// TODO(crbug.com/1309194): using webgpu mailbox texture uploading path on linux
// platform requires interop supported. According to the bug, this change will
// be a long time task. So disable using webgpu mailbox texture uploading path
// on linux platform.
// TODO(crbug.com/1424119): using a webgpu mailbox texture on the OpenGLES
// backend is failing for unknown reasons.
#if BUILDFLAG(IS_LINUX)
  bool forceReadback = true;
#elif BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/dawn/1969): Some Android devices don't fail to copy from
  // ImageBitmaps that were created from a non-texture-backed source, like
  // ImageData. Forcing those textures down the readback path is an easy way to
  // ensure the copies succeed. May be able to remove this check with some
  // better synchronization in the future.
  bool forceReadback = !image->IsTextureBacked();
#elif BUILDFLAG(IS_WIN)
  bool forceReadback =
      device()->adapter()->backendType() == wgpu::BackendType::OpenGLES;
#else
  bool forceReadback = false;
#endif
  if (forceReadback) {
    use_webgpu_mailbox_texture = false;
    unaccelerated_image = image->MakeUnaccelerated();
    image = unaccelerated_image.get();
  }

  // TODO(crbug.com/1426666): If disable OOP-R, using webgpu mailbox to upload
  // cpu-backed resource which has unpremultiply alpha type causes issues
  // due to alpha type has been dropped. Disable that
  // upload path if the image is not texture backed, OOP-R is disabled and image
  // alpha type is unpremultiplied.
  if (!features::IsCanvasOopRasterizationEnabled() &&
      !image->IsTextureBacked() && !image->IsPremultiplied()) {
    use_webgpu_mailbox_texture = false;
  }

  bool noop = copy_size.width == 0 || copy_size.height == 0 ||
              copy_size.depthOrArrayLayers == 0;

  // The copy rect might be a small part from a large source image. Instead of
  // copying the whole large source image, clipped to the small rect and upload
  // it is more performant. The clip rect should be chosen carefully when a
  // flipY op is required during uploading.
  gfx::Rect image_source_copy_rect =
      GetSourceImageSubrect(image, image->Rect(), origin, copy_size);

  // Get source image info.
  PaintImage paint_image = image->PaintImageForCurrentFrame();
  SkImageInfo source_image_info = paint_image.GetSkImageInfo();

  // TODO(crbug.com/1457649): If CPU backed source input discard the color
  // space info(e.g. ImageBitmap created with flag colorSpaceConversion: none).
  // disable using use_webgpu_mailbox_texture to fix alpha premultiplied isseu.
  if (!image->IsTextureBacked() && !image->IsPremultiplied() &&
      source_image_info.refColorSpace() == nullptr) {
    use_webgpu_mailbox_texture = false;
  }

  // Source and dst might have different constants
  ColorSpaceConversionConstants color_space_conversion_constants = {};

  // This uploading path try to extract WebGPU mailbox texture from source
  // image based on the copy size.
  // The uploading path works like this:
  // - Try to get WebGPUMailboxTexture with image source copy rect.
  // - If success, Issue Dawn::queueCopyTextureForBrowser to upload contents
  //   to WebGPU texture.
  if (use_webgpu_mailbox_texture) {
    // The copy rect might be a small part from a large source image. Instead of
    // copying large source image, clipped to the small copy rect is more
    // performant. The clip rect should be chosen carefully when a flipY op is
    // required during uploading.
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromStaticBitmapImage(
            GetDawnControlClient(), device_->GetHandle(),
            static_cast<wgpu::TextureUsage>(wgpu::TextureUsage::CopyDst |
                                            wgpu::TextureUsage::CopySrc |
                                            wgpu::TextureUsage::TextureBinding),
            image, source_image_info, image_source_copy_rect, noop);

    if (mailbox_texture != nullptr) {
      wgpu::ImageCopyTexture src = {.texture = mailbox_texture->GetTexture()};

      wgpu::CopyTextureForBrowserOptions options =
          CreateCopyTextureForBrowserOptions(
              image, &paint_image, dst_color_space, dst_premultiplied_alpha,
              flipY, &color_space_conversion_constants);

      GetHandle().CopyTextureForBrowser(&src, &destination, &copy_size,
                                        &options);
      return true;
    }
    // Fallback path accepts CPU backed resource only.
    unaccelerated_image = image->MakeUnaccelerated();
    image = unaccelerated_image.get();
    paint_image = image->PaintImageForCurrentFrame();
    image_source_copy_rect =
        GetSourceImageSubrect(image, image->Rect(), origin, copy_size);
    source_image_info = paint_image.GetSkImageInfo();
  }

  // This fallback path will handle all cases that cannot extract source image
  // to webgpu mailbox texture based on copy rect. It accepts CPU backed
  // resource only. The fallback path works like this:
  // - Always create a mappable wgpu::Buffer and copy CPU backed image resource
  // to the buffer.
  // - Always create a wgpu::Texture and issue a B2T copy to upload the content
  // from buffer to texture.
  // - Issue Dawn::queueCopyTextureForBrowser to upload contents from temp
  // texture to dst texture.
  // - Destroy all temp resources.
  CHECK(!image->IsTextureBacked());
  CHECK(!paint_image.IsTextureBacked());

  // Handling CPU resource.

  // Create intermediate texture as input for CopyTextureForBrowser().
  // For noop copy, creating intermediate texture with minimum size.
  const uint32_t src_width =
      noop && image_source_copy_rect.width() == 0
          ? 1
          : static_cast<uint32_t>(image_source_copy_rect.width());
  const uint32_t src_height =
      noop && image_source_copy_rect.height() == 0
          ? 1
          : static_cast<uint32_t>(image_source_copy_rect.height());

  SkColorType source_color_type = source_image_info.colorType();
  wgpu::TextureDescriptor texture_desc = {
      .usage = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
               wgpu::TextureUsage::TextureBinding,
      .size = {src_width, src_height, 1},
      .format = SkColorTypeToDawnColorFormat(source_color_type),
  };

  wgpu::Texture intermediate_texture =
      device_->GetHandle().CreateTexture(&texture_desc);

  // For noop copy, read source image content to mappable webgpu buffer and
  // using B2T copy to copy source content to intermediate texture.
  if (!noop) {
    // Source type is SkColorType::kRGBA_8888_SkColorType or
    // SkColorType::kBGRA_8888_SkColorType.
    uint64_t bytes_per_pixel = 4;

    base::CheckedNumeric<uint32_t> bytes_per_row =
        AlignBytesPerRow(image_source_copy_rect.width() * bytes_per_pixel);

    // Static cast to uint64_t to catch overflow during multiplications and use
    // base::CheckedNumeric to catch this overflow.
    base::CheckedNumeric<size_t> size_in_bytes =
        bytes_per_row * static_cast<uint64_t>(image_source_copy_rect.height());

    // Overflow happens when calculating size or row bytes.
    if (!size_in_bytes.IsValid()) {
      return false;
    }

    uint32_t wgpu_bytes_per_row = bytes_per_row.ValueOrDie();

    // Create a mapped buffer to receive external image contents
    wgpu::BufferDescriptor buffer_desc = {
        .usage = wgpu::BufferUsage::CopySrc,
        .size = size_in_bytes.ValueOrDie(),
        .mappedAtCreation = true,
    };

    wgpu::Buffer intermediate_buffer =
        device_->GetHandle().CreateBuffer(&buffer_desc);

    // This could happen either on OOM or if the image is to large to fit the
    // size in a uint32.
    if (!intermediate_buffer) {
      return false;
    }

    size_t size = static_cast<size_t>(buffer_desc.size);
    void* data = intermediate_buffer.GetMappedRange(0, size);

    auto dest_pixels = base::span<uint8_t>(static_cast<uint8_t*>(data), size);

    SkImageInfo copy_rect_info = source_image_info.makeWH(
        image_source_copy_rect.width(), image_source_copy_rect.height());
    bool success = paint_image.readPixels(
        copy_rect_info, dest_pixels.data(), wgpu_bytes_per_row,
        image_source_copy_rect.x(), image_source_copy_rect.y());
    if (!success) {
      return false;
    }

    intermediate_buffer.Unmap();

    // Start a B2T copy to move contents from buffer to intermediate texture
    wgpu::ImageCopyBuffer dawn_intermediate_buffer = {
        .layout =
            {
                .bytesPerRow = wgpu_bytes_per_row,
                .rowsPerImage = copy_size.height,
            },
        .buffer = intermediate_buffer,
    };

    wgpu::ImageCopyTexture dawn_intermediate_texture = {
        .texture = intermediate_texture,
        .aspect = wgpu::TextureAspect::All,
    };

    wgpu::Extent3D source_image_copy_size = {copy_size.width, copy_size.height};

    wgpu::CommandEncoder encoder = device_->GetHandle().CreateCommandEncoder();
    encoder.CopyBufferToTexture(&dawn_intermediate_buffer,
                                &dawn_intermediate_texture,
                                &source_image_copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    GetHandle().Submit(1, &commands);
  }

  wgpu::ImageCopyTexture src = {
      .texture = intermediate_texture,
  };
  wgpu::CopyTextureForBrowserOptions options =
      CreateCopyTextureForBrowserOptions(image, &paint_image, dst_color_space,
                                         dst_premultiplied_alpha, flipY,
                                         &color_space_conversion_constants);
  GetHandle().CopyTextureForBrowser(&src, &destination, &copy_size, &options);
  return true;
}
}  // namespace blink
