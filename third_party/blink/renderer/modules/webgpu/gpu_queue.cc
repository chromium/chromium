// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_elementimage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_copy_element_image_destination.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_copy_element_image_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_draw_element_image_destination.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_draw_element_image_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_external_image.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_image_bitmap.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture_tagged.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texel_copy_texture_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuextent3ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_imagedata_offscreencanvas_videoframe.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/canvas/element_image.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/external_image_utils.h"
#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/texture_utils.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_callback.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace blink {

namespace {

bool IsValidExternalImageDestinationFormat(
    wgpu::TextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case wgpu::TextureFormat::R8Unorm:
    case wgpu::TextureFormat::R16Float:
    case wgpu::TextureFormat::R16Unorm:
    case wgpu::TextureFormat::R32Float:
    case wgpu::TextureFormat::RG8Unorm:
    case wgpu::TextureFormat::RG16Float:
    case wgpu::TextureFormat::RG16Unorm:
    case wgpu::TextureFormat::RG32Float:
    case wgpu::TextureFormat::RGBA8Unorm:
    case wgpu::TextureFormat::RGBA8UnormSrgb:
    case wgpu::TextureFormat::BGRA8Unorm:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
    case wgpu::TextureFormat::RGB10A2Unorm:
    case wgpu::TextureFormat::RG11B10Ufloat:
    case wgpu::TextureFormat::RGBA16Float:
    case wgpu::TextureFormat::RGBA16Unorm:
    case wgpu::TextureFormat::RGBA32Float:
      return true;
    default:
      return false;
  }
}

std::optional<ExternalImageSource> GetExternalSourceFromExternalImage(
    const V8GPUImageCopyExternalImageSource* external_image,
    const ExternalImageDstInfo& dst_info,
    ExceptionState& exception_state) {
  switch (external_image->GetContentType()) {
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLVideoElement:
      return GetExternalImageSourceFrom(external_image->GetAsHTMLVideoElement(),
                                        dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kVideoFrame:
      return GetExternalImageSourceFrom(external_image->GetAsVideoFrame(),
                                        dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLCanvasElement:
      return GetExternalImageSourceFrom(
          external_image->GetAsHTMLCanvasElement(), dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kImageBitmap:
      return GetExternalImageSourceFrom(external_image->GetAsImageBitmap(),
                                        dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kImageData:
      return GetExternalImageSourceFrom(external_image->GetAsImageData(),
                                        dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kHTMLImageElement:
      return GetExternalImageSourceFrom(external_image->GetAsHTMLImageElement(),
                                        dst_info, exception_state);
    case V8GPUImageCopyExternalImageSource::ContentType::kOffscreenCanvas:
      return GetExternalImageSourceFrom(external_image->GetAsOffscreenCanvas(),
                                        dst_info, exception_state);
  }
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
                        wgpu::QueueWorkDoneStatus status,
                        wgpu::StringView message) {
  switch (status) {
    case wgpu::QueueWorkDoneStatus::Success:
      resolver->Resolve();
      break;
    case wgpu::QueueWorkDoneStatus::Error:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       String::FromUtf8(message));
      break;
    case wgpu::QueueWorkDoneStatus::CallbackCancelled:
      resolver->RejectWithDOMException(DOMExceptionCode::kOperationError,
                                       String::FromUtf8(message));
      break;
  }
}

ScriptPromise<IDLUndefined> GPUQueue::onSubmittedWorkDone(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();

  auto* callback = MakeWGPUOnceCallback(
      resolver->WrapCallbackInScriptScope(BindOnce(&OnWorkDoneCallback)));

  GetHandle().OnSubmittedWorkDone(wgpu::CallbackMode::AllowProcessEvents,
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
  WriteBufferImpl(script_state, buffer, buffer_offset,
                  data->ByteSpanMaybeShared(), data->TypeSize(),
                  data_element_offset, {}, exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const MaybeShared<DOMArrayBufferView>& data,
                           uint64_t data_element_offset,
                           uint64_t data_element_count,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset,
                  data->ByteSpanMaybeShared(), data->TypeSize(),
                  data_element_offset, data_element_count, exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset,
                  data->ByteSpanMaybeShared(), 1, data_byte_offset, {},
                  exception_state);
}

void GPUQueue::writeBuffer(ScriptState* script_state,
                           GPUBuffer* buffer,
                           uint64_t buffer_offset,
                           const DOMArrayBufferBase* data,
                           uint64_t data_byte_offset,
                           uint64_t byte_size,
                           ExceptionState& exception_state) {
  WriteBufferImpl(script_state, buffer, buffer_offset,
                  data->ByteSpanMaybeShared(), 1, data_byte_offset, byte_size,
                  exception_state);
}

void GPUQueue::WriteBufferImpl(ScriptState* script_state,
                               GPUBuffer* buffer,
                               uint64_t buffer_offset,
                               base::span<const uint8_t> data,
                               unsigned data_bytes_per_element,
                               uint64_t data_element_offset,
                               std::optional<uint64_t> data_element_count,
                               ExceptionState& exception_state) {
  CHECK_LE(data_bytes_per_element, 8u);

  if (data_element_offset > data.size() / data_bytes_per_element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Data offset is too large");
    return;
  }

  uint64_t data_byte_offset = data_element_offset * data_bytes_per_element;
  uint64_t max_write_size = data.size() - data_byte_offset;

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
  // the case since `data` comes from an ArrayBuffer.
  if (write_byte_size > uint64_t(std::numeric_limits<size_t>::max())) {
    exception_state.ThrowRangeError(
        "writeSize larger than size_t (please report a bug if you see this)");
    return;
  }

  auto data_span = data.subspan(static_cast<size_t>(data_byte_offset),
                                static_cast<size_t>(write_byte_size));
  GetHandle().WriteBuffer(buffer->GetHandle(), buffer_offset, data_span.data(),
                          data_span.size());
  EnsureFlush(ToEventLoop(script_state));
}

void GPUQueue::writeTexture(ScriptState* script_state,
                            GPUTexelCopyTextureInfo* destination,
                            const MaybeShared<DOMArrayBufferView>& data,
                            GPUTexelCopyBufferLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(script_state, destination, data->ByteSpanMaybeShared(),
                   data_layout, write_size, exception_state);
}

void GPUQueue::writeTexture(ScriptState* script_state,
                            GPUTexelCopyTextureInfo* destination,
                            const DOMArrayBufferBase* data,
                            GPUTexelCopyBufferLayout* data_layout,
                            const V8GPUExtent3D* write_size,
                            ExceptionState& exception_state) {
  WriteTextureImpl(script_state, destination, data->ByteSpanMaybeShared(),
                   data_layout, write_size, exception_state);
}

void GPUQueue::WriteTextureImpl(ScriptState* script_state,
                                GPUTexelCopyTextureInfo* destination,
                                base::span<const uint8_t> data,
                                GPUTexelCopyBufferLayout* data_layout,
                                const V8GPUExtent3D* write_size,
                                ExceptionState& exception_state) {
  wgpu::Extent3D dawn_write_size;
  wgpu::TexelCopyTextureInfo dawn_destination;
  if (!ConvertToDawn(write_size, &dawn_write_size, device_, exception_state) ||
      !ConvertToDawn(destination, &dawn_destination, exception_state)) {
    return;
  }

  wgpu::TexelCopyBufferLayout dawn_data_layout = {};
  {
    const char* error =
        ValidateTexelCopyBufferLayout(data_layout, &dawn_data_layout);
    if (error) {
      device_->InjectError(wgpu::ErrorType::Validation, error);
      return;
    }
  }

  if (dawn_data_layout.offset > data.size()) {
    device_->InjectError(wgpu::ErrorType::Validation,
                         "Data offset is too large");
    return;
  }

  // Handle the data layout offset by offsetting the data pointer instead. This
  // helps move less data between then renderer and GPU process (otherwise all
  // the data from 0 to offset would be copied over as well).
  auto data_span = data.subspan(static_cast<size_t>(dawn_data_layout.offset));
  dawn_data_layout.offset = 0;

  // Compute a tight upper bound of the number of bytes to send for this
  // WriteTexture. This can be 0 for some cases that produce validation errors,
  // but we don't create an error in Blink since Dawn can produce better error
  // messages (and this is more up-to-spec because the errors must be created on
  // the device timeline).
  size_t data_size_upper_bound = EstimateWriteTextureBytesUpperBound(
      dawn_data_layout, dawn_write_size, destination->texture()->Format(),
      dawn_destination.aspect);
  size_t required_copy_size = std::min(data_span.size(), data_size_upper_bound);

  GetHandle().WriteTexture(&dawn_destination, data_span.data(),
                           required_copy_size, &dawn_data_layout,
                           &dawn_write_size);
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

  std::optional<ExternalImageSource> source =
      GetExternalSourceFromExternalImage(
          copyImage->source(), {destination->premultipliedAlpha(), color_space},
          exception_state);
  if (!source) {
    if (!exception_state.HadException()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Could not get the source image");
    }
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): Browser fails extracting valid resource"
        "from external image. This API call will return early.");
    return;
  }

  wgpu::TexelCopyTextureInfo dawn_destination;
  if (!IsValidDestinationTexture(destination, dawn_destination,
                                 exception_state)) {
    return;
  }

  wgpu::Extent3D dawn_copy_size;
  wgpu::Origin2D origin_in_external_image;
  if (!ConvertToDawn(copy_size, &dawn_copy_size, device_, exception_state) ||
      !ConvertToDawn(copyImage->origin(), &origin_in_external_image,
                     exception_state)) {
    return;
  }

  const bool copyRectOutOfBounds =
      source->width < origin_in_external_image.x ||
      source->height < origin_in_external_image.y ||
      source->width - origin_in_external_image.x < dawn_copy_size.width ||
      source->height - origin_in_external_image.y < dawn_copy_size.height;

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

  // Issue the noop copy to continue validation to destination textures
  if (dawn_copy_size.width == 0 || dawn_copy_size.height == 0 ||
      dawn_copy_size.depthOrArrayLayers == 0) {
    device_->AddConsoleWarning(
        "CopyExternalImageToTexture(): It is a noop copy"
        "({width|height|depthOrArrayLayers} equals to 0).");
  }

  if (source->external_texture_source.valid) {
    // Use display size which is based on natural size but considering
    // transformation metadata.
    wgpu::Extent2D video_frame_display_size = {source->width, source->height};
    CopyFromVideoElement(
        source->external_texture_source, video_frame_display_size,
        origin_in_external_image, dawn_copy_size, dawn_destination,
        destination->premultipliedAlpha(), color_space, copyImage->flipY());
    return;
  }

  if (!CopyStaticImagBitmapToWGPUTexture(
          GetDawnControlClient(), device_->GetHandle(), source->image.get(),
          origin_in_external_image, dawn_copy_size, dawn_destination,
          destination->premultipliedAlpha(), color_space, copyImage->flipY())) {
    exception_state.ThrowTypeError(
        "Failed to copy content from external image.");
    return;
  }
}

bool GPUQueue::IsValidDestinationTexture(
    GPUImageCopyTextureTagged* destination,
    wgpu::TexelCopyTextureInfo& dawn_destination,
    ExceptionState& exception_state) {
  if (!ConvertToDawn(destination, &dawn_destination, exception_state)) {
    device_->GetHandle().InjectError(wgpu::ErrorType::Validation,
                                     "Invalid destination.");
    return false;
  }
  if (!IsValidExternalImageDestinationFormat(
          destination->texture()->Format())) {
    device_->GetHandle().InjectError(wgpu::ErrorType::Validation,
                                     "Invalid destination gpu texture format.");
    return false;
  }
  if (destination->texture()->Dimension() != wgpu::TextureDimension::e2D) {
    device_->GetHandle().InjectError(wgpu::ErrorType::Validation,
                                     "Dst gpu texture must be 2d.");
    return false;
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
    return false;
  }
  return true;
}

void GPUQueue::copyElementImageToTexture(
    GPUCopyElementImageSource* source,
    GPUCopyElementImageDestination* destination,
    ExceptionState& exception_state) {
  if (source->hasSx() != source->hasSy() ||
      source->hasSx() != source->hasSwidth() ||
      source->hasSx() != source->hasSheight()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Must specify all or none of (sx, sy, swidth, sheight).");
    return;
  }
  if (destination->hasWidth() != destination->hasHeight()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Must specify neither or both of (width,height).");
    return;
  }
  std::optional<float> sx;
  std::optional<float> sy;
  std::optional<float> swidth;
  std::optional<float> sheight;
  if (source->hasSx()) {
    sx = source->sx();
    sy = source->sy();
    swidth = source->swidth();
    sheight = source->sheight();
  }

  std::optional<uint32_t> width;
  std::optional<uint32_t> height;
  if (destination->hasWidth()) {
    width = destination->width();
    height = destination->height();
  }

  DrawElementImageToTextureInternal(source->source(), sx, sy, swidth, sheight,
                                    width, height, destination->destination(),
                                    exception_state);
}

void GPUQueue::drawElementImageToTexture(
    GPUDrawElementImageSource* source,
    GPUDrawElementImageDestination* destination,
    ExceptionState& exception_state) {
  if (source->hasSourceX() != source->hasSourceY() ||
      source->hasSourceX() != source->hasSourceWidth() ||
      source->hasSourceX() != source->hasSourceHeight()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Must specify all or none of "
        "(sourceX, sourceY, sourceWidth, sourceHeight).");
    return;
  }
  std::optional<wgpu::Extent3D> dawn_copy_size;
  if (destination->hasSize()) {
    dawn_copy_size.emplace();
    if (!ConvertToDawn(destination->size(), &dawn_copy_size.value(), device_,
                       exception_state)) {
      return;
    }
  }

  std::optional<float> sx;
  std::optional<float> sy;
  std::optional<float> swidth;
  std::optional<float> sheight;
  if (source->hasSourceX()) {
    sx = source->sourceX();
    sy = source->sourceY();
    swidth = source->sourceWidth();
    sheight = source->sourceHeight();
  }

  std::optional<uint32_t> width;
  std::optional<uint32_t> height;
  if (dawn_copy_size.has_value()) {
    width = dawn_copy_size->width;
    height = dawn_copy_size->height;
  }

  DrawElementImageToTextureInternal(source->source(), sx, sy, swidth, sheight,
                                    width, height, destination,
                                    exception_state);
}

void GPUQueue::DrawElementImageToTextureInternal(
    const V8UnionElementOrElementImage* source,
    std::optional<float> sx,
    std::optional<float> sy,
    std::optional<float> swidth,
    std::optional<float> sheight,
    std::optional<uint32_t> width,
    std::optional<uint32_t> height,
    GPUImageCopyTextureTagged* destination,
    ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::CanvasDrawElementEnabled(
      device_->GetExecutionContext()));

  CanvasRenderingContext* context = nullptr;
  if (source->IsElement()) {
    context = CanvasRenderingContext::GetEnclosingContextForDrawElement(
        source->GetAsElement(), "drawElementImageToTexture()", exception_state);
  } else {
    const std::unique_ptr<CanvasChildPaintRecord>& record =
        source->GetAsElementImage()->PaintRecord();
    if (record) {
      DOMNodeId canvas_node_id = record->paint_state.canvas_node_id;
      if (canvas_node_id != kInvalidDOMNodeId) {
        if (device_->GetExecutionContext()->IsWindow()) {
          if (auto* html_canvas = DynamicTo<HTMLCanvasElement>(
                  DOMNodeIds::NodeForId(canvas_node_id))) {
            context = html_canvas->RenderingContext();
          }
        }
        if (!context) {
          if (auto* offscreen_canvas = OffscreenCanvas::FromPlaceholderId(
                  device_->GetExecutionContext(), canvas_node_id)) {
            context = offscreen_canvas->RenderingContext();
          }
        }
      }
    }
  }

  if (!context) {
    if (source->IsElementImage()) {
      if (!source->GetAsElementImage()->PaintRecord()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "The ElementImage has been closed.");
      } else {
        exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                          "No context found for ElementImage.");
      }
    }
    return;
  }

  PredefinedColorSpace color_space;
  if (!ValidateAndConvertColorSpace(destination->colorSpace(), color_space,
                                    exception_state)) {
    return;
  }

  wgpu::TexelCopyTextureInfo dawn_destination;
  if (!IsValidDestinationTexture(destination, dawn_destination,
                                 exception_state)) {
    return;
  }

  scoped_refptr<StaticBitmapImage> image =
      context->GetElementImage(source, sx, sy, swidth, sheight, width, height,
                               gpu::SHARED_IMAGE_USAGE_WEBGPU_READ,
                               "drawElementImageToTexture()", exception_state);
  if (!image) {
    return;
  }

  // If the image is larger than the destination texture, clip it
  wgpu::Extent3D dawn_copy_size;
  dawn_copy_size.width = std::min(
      dawn_destination.texture.GetWidth(),
      base::ClampedNumeric(image->Size().width()).Cast<uint32_t>().RawValue());
  dawn_copy_size.height = std::min(
      dawn_destination.texture.GetHeight(),
      base::ClampedNumeric(image->Size().height()).Cast<uint32_t>().RawValue());
  if (!CopyStaticImagBitmapToWGPUTexture(
          GetDawnControlClient(), device_->GetHandle(), image.get(),
          wgpu::Origin2D(), dawn_copy_size, dawn_destination,
          destination->premultipliedAlpha(), color_space,
          /*flipY*/ false)) {
    exception_state.ThrowTypeError(
        "Failed to copy content from element image.");
    return;
  }
}

void GPUQueue::CopyFromVideoElement(
    const ExternalTextureSource source,
    const wgpu::Extent2D& video_frame_natural_size,
    const wgpu::Origin2D& origin,
    const wgpu::Extent3D& copy_size,
    const wgpu::TexelCopyTextureInfo& destination,
    bool dst_premultiplied_alpha,
    PredefinedColorSpace dst_color_space,
    bool flipY) {
  CHECK(source.valid);

  // Create External Texture with dst color space. No color space conversion
  // happens during copy step.
  ExternalTexture external_texture =
      CreateExternalTexture(device_, dst_color_space, source.media_video_frame);

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

  if (external_texture.is_zero_copy &&
      source.media_video_frame->metadata().read_lock_fences_enabled) {
    ReferenceUntilGPUIsFinished(std::move(external_texture.mailbox_texture));
  }
}

void GPUQueue::ReferenceUntilGPUIsFinished(
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture) {
  CHECK(mailbox_texture);
  ExecutionContext* execution_context = device_->GetExecutionContext();

  // If device has no valid execution context. Release
  // the mailbox immediately.
  if (!execution_context) {
    return;
  }

  // Keep mailbox texture alive until callback returns.
  auto* callback = BindWGPUOnceCallback(
      [](scoped_refptr<WebGPUMailboxTexture> mailbox_texture,
         wgpu::QueueWorkDoneStatus, wgpu::StringView) {},
      std::move(mailbox_texture));

  GetHandle().OnSubmittedWorkDone(wgpu::CallbackMode::AllowProcessEvents,
                                  callback->UnboundCallback(),
                                  callback->AsUserdata());

  // Ensure commands are flushed.
  device_->EnsureFlush(ToEventLoop(execution_context));
}

}  // namespace blink
