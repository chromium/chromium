// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/record_paint_canvas.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_text_cluster_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_element_elementimage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_direction.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_kerning.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_variant_caps.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_performance_monitor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"
#include "third_party/blink/renderer/core/html/canvas/element_image.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/canvas/text_cluster.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_2d_recorder_context.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"
#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/blend_mode.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/text/unicode_bidi.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

// Including "base/time/time.h" triggers a bug in IWYU.
// https://github.com/include-what-you-use/include-what-you-use/issues/1122
// IWYU pragma: no_include "base/numerics/clamped_math.h"

namespace blink {

class MemoryManagedPaintCanvas;

namespace {


bool IsContextProviderValid() {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         !context_provider_wrapper->ContextProvider().IsContextLost();
}

}  // namespace

constexpr char kDefaultFont[] = "10px sans-serif";
const char BaseRenderingContext2D::kInheritString[] = "inherit";

BaseRenderingContext2D::BaseRenderingContext2D(
    CanvasRenderingContextHost* canvas,
    const CanvasContextCreationAttributesCore& attrs,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : CanvasRenderingContext(canvas, attrs, CanvasRenderingAPI::k2D),
      dispatch_context_lost_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::DispatchContextLostEvent),
      dispatch_context_restored_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::DispatchContextRestoredEvent),
      try_restore_context_event_timer_(
          task_runner,
          this,
          &BaseRenderingContext2D::TryRestoreContextEvent),
      color_params_(attrs.color_space,
                    attrs.hdr_metadata,
                    attrs.pixel_format,
                    attrs.alpha) {}

void BaseRenderingContext2D::ResetInternal() {
  Canvas2DRecorderContext::ResetInternal();

}

CanvasRenderingContext2DSettings* BaseRenderingContext2D::getContextAttributes()
    const {
  return ToCanvasRenderingContext2DSettings(CreationAttributes());
}

void BaseRenderingContext2D::DispatchContextLostEvent(TimerBase*) {
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (!host) {
    return;
  }

  Event* event = Event::CreateCancelable(event_type_names::kContextlost);
  host->HostDispatchEvent(event);

  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DContextLostEvent);
  if (event->defaultPrevented()) {
    context_restorable_ = false;
  }

  if (!context_restorable_) {
    return;
  }

  if (context_lost_mode_ == CanvasRenderingContext::kInvalidCanvasSize &&
      host->IsValidImageSize()) {
    // The context is already restored (an invalid canvas size was probably
    // fixed). We can send the restored event right away.
    dispatch_context_restored_event_timer_.StartOneShot(base::TimeDelta(),
                                                        FROM_HERE);
    return;
  }

  if (context_lost_mode_ == CanvasRenderingContext::kRealLostContext ||
      context_lost_mode_ == CanvasRenderingContext::kSyntheticLostContext) {
    try_restore_context_attempt_count_ = 0;
    try_restore_context_event_timer_.StartRepeating(kTryRestoreContextInterval,
                                                    FROM_HERE);
  }
}

void BaseRenderingContext2D::DispatchContextRestoredEvent(TimerBase*) {
  // Since canvas may trigger contextlost event by multiple different ways (ex:
  // gpu crashes and frame eviction), it's possible to triggeer this
  // function while the context is already restored. In this case, we
  // abort it here.
  if (context_lost_mode_ == CanvasRenderingContext::kNotLostContext) {
    return;
  }

  if (!context_restorable_) {
    return;
  }

  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (host == nullptr) {
    // This function can be called in a new task, via
    // `dispatch_context_restored_event_timer_`. Abort if the host was disposed
    // since the task was queued.
    return;
  }

  host->ClearCanvas2DLayerTexture();
  ResetInternal();
  context_lost_mode_ = CanvasRenderingContext::kNotLostContext;
  Event* event(Event::Create(event_type_names::kContextrestored));
  host->HostDispatchEvent(event);
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DContextRestoredEvent);
}

void BaseRenderingContext2D::TryRestoreContextEvent(TimerBase* timer) {
  const CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (host == nullptr) [[unlikely]] {
    // The host was disposed while this callback was pending.
    try_restore_context_event_timer_.Stop();
    return;
  }

  DCHECK(context_lost_mode_ !=
         CanvasRenderingContext::kWebGLLoseContextLostContext);

  // The canvas was changed to an invalid size since the context was lost. We
  // can't restore the context until the canvas is given a valid size. Abort
  // here to avoid creating a shared GPU context we would not use.
  if (!host->IsValidImageSize() && !host->Size().IsEmpty()) {
    context_lost_mode_ = kInvalidCanvasSize;
    try_restore_context_event_timer_.Stop();
    return;
  }

  // For real context losses, we can only restore if the SharedGpuContext is
  // ready.
  if (context_lost_mode_ != CanvasRenderingContext::kRealLostContext ||
      (SharedGpuContext::IsGpuCompositingEnabled() &&
       IsContextProviderValid()) ||
      (!SharedGpuContext::IsGpuCompositingEnabled() &&
       SharedGpuContext::SharedImageInterfaceProvider())) {
    RestoreGuard context_is_being_restored(*this);
    if (GetOrCreateResourceProvider()) {
      try_restore_context_event_timer_.Stop();
      DispatchContextRestoredEvent(nullptr);
      return;
    }
  }

  // Retry up to `kMaxTryRestoreContextAttempts` times before giving up.
  if (++try_restore_context_attempt_count_ > kMaxTryRestoreContextAttempts) {
    try_restore_context_event_timer_.Stop();
    if (on_restore_failed_callback_for_testing_) {
      on_restore_failed_callback_for_testing_.Run();
    }
  }
}

void BaseRenderingContext2D::RestoreFromInvalidSizeIfNeeded() {
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  if (!context_restorable_ || context_lost_mode_ != kInvalidCanvasSize ||
      !host) {
    return;
  }
  DCHECK(!GetResourceProvider());

  if (host->IsValidImageSize()) {
    // The size was restored. Fire a contextrestored event, but only if there's
    // no pending contextlost. contextlost needs to run first and it will take
    // care of running contextrestored if the size is still valid at that point.
    if (!dispatch_context_lost_event_timer_.IsActive()) {
      dispatch_context_restored_event_timer_.StartOneShot(base::TimeDelta(),
                                                          FROM_HERE);
    }
  } else {
    // The canvas was given another invalid size. Abort any pending
    // contextrestored event, these would have to wait until the canvas is given
    // a valid size.
    if (dispatch_context_restored_event_timer_.IsActive()) {
      dispatch_context_restored_event_timer_.Stop();
    }
  }
}

ImageData* BaseRenderingContext2D::createImageData(
    ImageData* image_data,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  return ImageData::ValidateAndCreate(
      image_data->Size().width(), image_data->Size().height(), std::nullopt,
      image_data->getSettings(), params, exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  params.default_color_space = GetDefaultImageDataColorSpace();
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), std::nullopt,
                                      /*settings=*/nullptr, params,
                                      exception_state);
}

ImageData* BaseRenderingContext2D::createImageData(
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) const {
  ImageData::ValidateAndCreateParams params;
  params.context_2d_error_mode = true;
  params.default_color_space = GetDefaultImageDataColorSpace();
  return ImageData::ValidateAndCreate(std::abs(sw), std::abs(sh), std::nullopt,
                                      image_data_settings, params,
                                      exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, /*image_data_settings=*/nullptr,
                              exception_state);
}

ImageData* BaseRenderingContext2D::getImageData(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  return getImageDataInternal(sx, sy, sw, sh, image_data_settings,
                              exception_state);
}
perfetto::EventContext GetEventContext();

ImageData* BaseRenderingContext2D::getImageDataInternal(
    int sx,
    int sy,
    int sw,
    int sh,
    ImageDataSettings* image_data_settings,
    ExceptionState& exception_state) {
  if (!base::CheckMul(sw, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  if (layer_count_ != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`getImageData()` cannot be called with open layers.");
    return nullptr;
  }

  if (!OriginClean()) {
    exception_state.ThrowSecurityError(
        "The canvas has been tainted by cross-origin data.");
  } else if (!sw || !sh) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        UNSAFE_TODO(
            String::Format("The source %s is 0.", sw ? "height" : "width")));
  }

  if (exception_state.HadException())
    return nullptr;

  if (sw < 0) {
    if (!base::CheckAdd(sx, sw).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sx += sw;
    sw = base::saturated_cast<int>(base::SafeUnsignedAbs(sw));
  }
  if (sh < 0) {
    if (!base::CheckAdd(sy, sh).IsValid<int>()) {
      exception_state.ThrowRangeError("Out of memory at ImageData creation");
      return nullptr;
    }
    sy += sh;
    sh = base::saturated_cast<int>(base::SafeUnsignedAbs(sh));
  }

  if (!base::CheckAdd(sx, sw).IsValid<int>() ||
      !base::CheckAdd(sy, sh).IsValid<int>()) {
    exception_state.ThrowRangeError("Out of memory at ImageData creation");
    return nullptr;
  }

  const gfx::Rect image_data_rect(sx, sy, sw, sh);

  ImageData::ValidateAndCreateParams validate_and_create_params;
  validate_and_create_params.context_2d_error_mode = true;
  validate_and_create_params.default_color_space =
      GetDefaultImageDataColorSpace();

  if (isContextLost()) {
    return ImageData::ValidateAndCreate(
        sw, sh, std::nullopt, image_data_settings, validate_and_create_params,
        exception_state);
  }

  // Deferred offscreen canvases might have recorded commands, make sure
  // that those get drawn here
  FinalizeFrame(FlushReason::kOther);

  num_readbacks_performed_++;
  CanvasContextCreationAttributesCore::WillReadFrequently
      will_read_frequently_value = GetCanvasRenderingContextHost()
                                       ->RenderingContext()
                                       ->CreationAttributes()
                                       .will_read_frequently;
  if (num_readbacks_performed_ == 2 && GetCanvasRenderingContextHost() &&
      GetCanvasRenderingContextHost()->RenderingContext()) {
    if (will_read_frequently_value ==
        CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined) {
      if (auto* execution_context = GetTopExecutionContext()) {
        const String& message =
            "Canvas2D: Multiple readback operations using getImageData are "
            "faster with the willReadFrequently attribute set to true. See: "
            "https://html.spec.whatwg.org/multipage/"
            "canvas.html#concept-canvas-will-read-frequently";
        execution_context->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kRendering,
                mojom::blink::ConsoleMessageLevel::kWarning, message));
      }
    }
  }

  // The default behavior before the willReadFrequently feature existed:
  // Accelerated canvases fall back to CPU when there is a readback.
  if (will_read_frequently_value ==
      CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined) {
    // GetImageData is faster in Unaccelerated canvases.
    // In Desynchronized canvas disabling the acceleration will break
    // putImageData: crbug.com/1112060.
    if (IsAccelerated() && !IsDesynchronized()) {
      read_count_++;
      if (read_count_ >= kFallbackToCPUAfterReadbacks ||
          ShouldDisableAccelerationBecauseOfReadback()) {
        DisableAcceleration();
        base::UmaHistogramEnumeration("Blink.Canvas.GPUFallbackToCPU",
                                      GPUFallbackToCPUScenario::kGetImageData);
      }
    }
  }

  scoped_refptr<StaticBitmapImage> snapshot = GetImage();

  // Determine if the array should be zero initialized, or if it will be
  // completely overwritten.
  validate_and_create_params.zero_initialize = false;
  if (IsAccelerated()) {
    // GPU readback may fail silently.
    validate_and_create_params.zero_initialize = true;
  } else if (snapshot) {
    // Zero-initialize if some of the readback area is out of bounds.
    if (image_data_rect.x() < 0 || image_data_rect.y() < 0 ||
        image_data_rect.right() > snapshot->Size().width() ||
        image_data_rect.bottom() > snapshot->Size().height()) {
      validate_and_create_params.zero_initialize = true;
    }
  } else {
    // If there's no snapshot, the buffer will not be overwritten and hence must
    // be zero-initialized.
    validate_and_create_params.zero_initialize = true;
  }

  ImageData* image_data =
      ImageData::ValidateAndCreate(sw, sh, std::nullopt, image_data_settings,
                                   validate_and_create_params, exception_state);
  if (!image_data)
    return nullptr;

  // Read pixels into |image_data|.
  if (snapshot) {
    gfx::Rect snapshot_rect{snapshot->Size()};
    if (!snapshot_rect.Intersects(image_data_rect)) {
      // If the readback area is completely out of bounds just return a zero
      // initialized buffer. No point in trying to perform out of bounds read.
      CHECK(validate_and_create_params.zero_initialize);
      return image_data;
    }

    SkPixmap image_data_pixmap = image_data->GetSkPixmap();
    const bool read_pixels_successful =
        snapshot->PaintImageForCurrentFrame().readPixels(
            image_data_pixmap.info(), image_data_pixmap.writable_addr(),
            image_data_pixmap.rowBytes(), sx, sy);
    if (!read_pixels_successful) {
      SkIRect bounds =
          snapshot->PaintImageForCurrentFrame().GetSkImageInfo().bounds();
      DCHECK(!bounds.intersect(SkIRect::MakeXYWH(sx, sy, sw, sh)));
    }
  }

  return image_data;
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          ExceptionState& exception_state) {
  putImageData(data, dx, dy, 0, 0, data->width(), data->height(),
               exception_state);
}

void BaseRenderingContext2D::putImageData(ImageData* data,
                                          int dx,
                                          int dy,
                                          int dirty_x,
                                          int dirty_y,
                                          int dirty_width,
                                          int dirty_height,
                                          ExceptionState& exception_state) {
  if (!base::CheckMul(dirty_width, dirty_height).IsValid<int>()) {
    return;
  }

  if (data->IsBufferBaseDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The source data has been detached.");
    return;
  }

  if (layer_count_ != 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "`putImageData()` cannot be called with open layers.");
    return;
  }

  if (isContextLost() || !CanCreateResourceProvider()) [[unlikely]] {
    return;
  }

  if (dirty_width < 0) {
    if (dirty_x < 0) {
      dirty_x = dirty_width = 0;
    } else {
      dirty_x += dirty_width;
      dirty_width =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_width));
    }
  }

  if (dirty_height < 0) {
    if (dirty_y < 0) {
      dirty_y = dirty_height = 0;
    } else {
      dirty_y += dirty_height;
      dirty_height =
          base::saturated_cast<int>(base::SafeUnsignedAbs(dirty_height));
    }
  }

  gfx::Rect dest_rect(dirty_x, dirty_y, dirty_width, dirty_height);
  dest_rect.Intersect(gfx::Rect(0, 0, data->width(), data->height()));
  gfx::Vector2d dest_offset(static_cast<int>(dx), static_cast<int>(dy));
  dest_rect.Offset(dest_offset);
  dest_rect.Intersect(gfx::Rect(0, 0, Width(), Height()));
  if (dest_rect.IsEmpty())
    return;

  gfx::Rect source_rect = dest_rect;
  source_rect.Offset(-dest_offset);

  SkPixmap data_pixmap = data->GetSkPixmap();

  // WritePixels (called by PutByteArray) requires that the source and
  // destination pixel formats have the same bytes per pixel.
  SkColorType dest_color_type =
      viz::ToClosestSkColorType(color_params_.GetSharedImageFormat());
  if (SkColorTypeBytesPerPixel(dest_color_type) !=
      SkColorTypeBytesPerPixel(data_pixmap.colorType())) {
    SkImageInfo converted_info =
        data_pixmap.info().makeColorType(dest_color_type);
    SkBitmap converted_bitmap;
    if (!converted_bitmap.tryAllocPixels(converted_info)) {
      exception_state.ThrowRangeError("Out of memory in putImageData");
      return;
    }
    if (!converted_bitmap.writePixels(data_pixmap, 0, 0)) {
      NOTREACHED() << "Failed to convert ImageData with writePixels.";
    }

    PutByteArray(converted_bitmap.pixmap(), source_rect, dest_offset);
    if (GetPaintCanvas()) {
      WillDraw(dest_rect, CanvasPerformanceMonitor::DrawType::kImageData);
    }
    return;
  }

  PutByteArray(data_pixmap, source_rect, dest_offset);
  if (GetPaintCanvas()) {
    WillDraw(dest_rect, CanvasPerformanceMonitor::DrawType::kImageData);
  }
}

void BaseRenderingContext2D::PutByteArray(const SkPixmap& source,
                                          const gfx::Rect& source_rect,
                                          const gfx::Vector2d& dest_offset) {
  DCHECK(gfx::Rect(source.width(), source.height()).Contains(source_rect));
  int dest_x = dest_offset.x() + source_rect.x();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, Width());
  int dest_y = dest_offset.y() + source_rect.y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, Height());

  SkImageInfo info =
      source.info().makeWH(source_rect.width(), source_rect.height());
  if (!HasAlpha()) {
    // If the surface is opaque, tell it that we are writing opaque
    // pixels.  Writing non-opaque pixels to opaque is undefined in
    // Skia.  There is some discussion about whether it should be
    // defined in skbug.com/6157.  For now, we can get the desired
    // behavior (memcpy) by pretending the write is opaque.
    info = info.makeAlphaType(kOpaque_SkAlphaType);
  } else {
    info = info.makeAlphaType(kUnpremul_SkAlphaType);
  }

  WritePixels(info, source.addr(source_rect.x(), source_rect.y()),
              source.rowBytes(), dest_x, dest_y);
}

String BaseRenderingContext2D::letterSpacing() const {
  return GetState().GetLetterSpacing();
}

String BaseRenderingContext2D::wordSpacing() const {
  return GetState().GetWordSpacing();
}

V8CanvasTextRendering BaseRenderingContext2D::textRendering() const {
  return V8CanvasTextRendering(GetState().GetTextRendering());
}

V8CanvasTextAlign BaseRenderingContext2D::textAlign() const {
  return V8CanvasTextAlign(GetState().GetTextAlign());
}

void BaseRenderingContext2D::setTextAlign(const V8CanvasTextAlign align) {
  GetState().SetTextAlign(align.AsEnum());
}

V8CanvasTextBaseline BaseRenderingContext2D::textBaseline() const {
  return V8CanvasTextBaseline(GetState().GetTextBaseline());
}

void BaseRenderingContext2D::setTextBaseline(
    const V8CanvasTextBaseline baseline) {
  GetState().SetTextBaseline(baseline.AsEnum());
}

V8CanvasFontKerning BaseRenderingContext2D::fontKerning() const {
  switch (GetState().GetFontKerning()) {
    case (FontDescription::Kerning::kAutoKerning):
      return V8CanvasFontKerning(V8CanvasFontKerning::Enum::kAuto);
    case (FontDescription::Kerning::kNoneKerning):
      return V8CanvasFontKerning(V8CanvasFontKerning::Enum::kNone);
    case (FontDescription::Kerning::kNormalKerning):
      return V8CanvasFontKerning(V8CanvasFontKerning::Enum::kNormal);
  }
}

V8CanvasFontStretch BaseRenderingContext2D::fontStretch() const {
  return V8CanvasFontStretch(GetState().GetFontStretch());
}

V8CanvasFontVariantCaps BaseRenderingContext2D::fontVariantCaps() const {
  switch (GetState().GetFontVariantCaps()) {
    case (FontDescription::FontVariantCaps::kCapsNormal):
      return V8CanvasFontVariantCaps(V8CanvasFontVariantCaps::Enum::kNormal);
    case (FontDescription::FontVariantCaps::kSmallCaps):
      return V8CanvasFontVariantCaps(V8CanvasFontVariantCaps::Enum::kSmallCaps);
    case (FontDescription::FontVariantCaps::kAllSmallCaps):
      return V8CanvasFontVariantCaps(
          V8CanvasFontVariantCaps::Enum::kAllSmallCaps);
    case (FontDescription::FontVariantCaps::kPetiteCaps):
      return V8CanvasFontVariantCaps(
          V8CanvasFontVariantCaps::Enum::kPetiteCaps);
    case (FontDescription::FontVariantCaps::kAllPetiteCaps):
      return V8CanvasFontVariantCaps(
          V8CanvasFontVariantCaps::Enum::kAllPetiteCaps);
    case (FontDescription::FontVariantCaps::kTitlingCaps):
      return V8CanvasFontVariantCaps(
          V8CanvasFontVariantCaps::Enum::kTitlingCaps);
    case (FontDescription::FontVariantCaps::kUnicase):
      return V8CanvasFontVariantCaps(V8CanvasFontVariantCaps::Enum::kUnicase);
  }
}

void BaseRenderingContext2D::Trace(Visitor* visitor) const {
  visitor->Trace(dispatch_context_lost_event_timer_);
  visitor->Trace(dispatch_context_restored_event_timer_);
  visitor->Trace(try_restore_context_event_timer_);
  CanvasRenderingContext::Trace(visitor);
  Canvas2DRecorderContext::Trace(visitor);
}

bool BaseRenderingContext2D::Is2DCanvasAccelerated() const {
  if (IsHibernating()) {
    return false;
  }

  auto* resource_provider = GetResourceProvider();
  return resource_provider ? resource_provider->IsAccelerated()
                           : Host()->ShouldTryToUseGpuRaster();
}

void BaseRenderingContext2D::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* c) const {
  RestoreMatrixClipStack(c);
}

void BaseRenderingContext2D::Reset() {
  ResetInternal();
}

scoped_refptr<StaticBitmapImage>
BaseRenderingContext2D::PaintRenderingResultsToSnapshot(
    SourceDrawingBuffer source_buffer) {
  if (!IsResourceProviderValid()) {
    return nullptr;
  }

  CanvasResourceProvider* provider = GetResourceProvider();
  provider->Flush();
  return provider->Snapshot();
}

bool BaseRenderingContext2D::IsResourceProviderValid() {
  return GetResourceProvider() && GetResourceProvider()->IsValid();
}

void BaseRenderingContext2D::WillUseCurrentFont() const {
  if (HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
      canvas != nullptr) {
    canvas->GetDocument().GetCanvasFontCache()->WillUseCurrentFont();
  }
}

String BaseRenderingContext2D::font() const {
  const CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    return kDefaultFont;
  }

  WillUseCurrentFont();
  StringBuilder serialized_font;
  const FontDescription& font_description = state.GetFontDescription();

  if (font_description.Style() == kItalicSlopeValue) {
    serialized_font.Append("italic ");
  }
  if (font_description.Weight() == kBoldWeightValue) {
    serialized_font.Append("bold ");
  } else if (font_description.Weight() != kNormalWeightValue) {
    int weight_as_int = static_cast<int>((float)font_description.Weight());
    serialized_font.AppendNumber(weight_as_int);
    serialized_font.Append(" ");
  }
  if (font_description.VariantCaps() == FontDescription::kSmallCaps) {
    serialized_font.Append("small-caps ");
  }

  serialized_font.AppendNumber(font_description.ComputedSize());
  serialized_font.Append("px ");

  serialized_font.Append(
      ComputedStyleUtils::ValueForFontFamily(font_description.Family())
          ->CssText());

  return serialized_font.ToString();
}

bool BaseRenderingContext2D::WillSetFont() const {
  return true;
}

bool BaseRenderingContext2D::CurrentFontResolvedAndUpToDate() const {
  const CanvasRenderingContext2DState& state = GetState();
  return state.HasRealizedFont() && !state.LangIsDirty();
}

void BaseRenderingContext2D::setFont(const String& new_font) {
  if (!WillSetFont()) [[unlikely]] {
    return;
  }

  CanvasRenderingContext2DState& state = GetState();
  if (new_font == state.UnparsedFont() && CurrentFontResolvedAndUpToDate()) {
    return;
  }

  if (!ResolveFont(new_font)) {
    return;
  }

  // The parse succeeded.
  state.SetUnparsedFont(new_font);
}

static inline TextDirection ToTextDirection(
    V8CanvasDirection::Enum direction,
    CanvasRenderingContextHost* host,
    const ComputedStyle* style = nullptr) {
  switch (direction) {
    case V8CanvasDirection::Enum::kInherit:
      return host ? host->GetTextDirection(style) : TextDirection::kLtr;
    case V8CanvasDirection::Enum::kRtl:
      return TextDirection::kRtl;
    case V8CanvasDirection::Enum::kLtr:
      return TextDirection::kLtr;
  }
  NOTREACHED();
}

V8CanvasDirection BaseRenderingContext2D::direction() const {
  const CanvasRenderingContext2DState& state = GetState();
  bool value_is_inherit =
      state.GetDirection() == V8CanvasDirection::Enum::kInherit;
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasTextDirectionGet);
  if (value_is_inherit) {
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvasTextDirectionGetInherit);
  }
  return ToTextDirection(state.GetDirection(),
                         GetCanvasRenderingContextHost()) == TextDirection::kRtl
             ? V8CanvasDirection(V8CanvasDirection::Enum::kRtl)
             : V8CanvasDirection(V8CanvasDirection::Enum::kLtr);
}

void BaseRenderingContext2D::setDirection(const V8CanvasDirection direction) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasTextDirectionSet);
  if (direction == V8CanvasDirection::Enum::kInherit) {
    UseCounter::Count(GetTopExecutionContext(),
                      WebFeature::kCanvasTextDirectionSetInherit);
  }

  CanvasRenderingContext2DState& state = GetState();
  state.SetDirection(direction.AsEnum());
}

void BaseRenderingContext2D::fillText(const String& text, double x, double y) {
  CanvasRenderingContext2DState& state = GetState();
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType,
                   state.GetTextAlign(), state.GetTextBaseline(), 0,
                   text.length());
}

void BaseRenderingContext2D::fillText(const String& text,
                                      double x,
                                      double y,
                                      double max_width) {
  CanvasRenderingContext2DState& state = GetState();
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kFillPaintType,
                   state.GetTextAlign(), state.GetTextBaseline(), 0,
                   text.length(), &max_width);
}

void BaseRenderingContext2D::fillTextCluster(const TextCluster* text_cluster,
                                             double x,
                                             double y) {
  fillTextCluster(text_cluster, x, y, /*cluster_options=*/nullptr);
}

void BaseRenderingContext2D::fillTextCluster(
    const TextCluster* text_cluster,
    double x,
    double y,
    const TextClusterOptions* cluster_options) {
  DCHECK(text_cluster);
  V8CanvasTextAlign::Enum cluster_align = text_cluster->align().AsEnum();
  V8CanvasTextBaseline::Enum cluster_baseline =
      text_cluster->baseline().AsEnum();
  double cluster_x = text_cluster->x();
  double cluster_y = text_cluster->y();
  if (cluster_options != nullptr) {
    if (cluster_options->hasX()) {
      cluster_x = cluster_options->x();
    }
    if (cluster_options->hasY()) {
      cluster_y = cluster_options->y();
    }
    if (cluster_options->hasAlign()) {
      cluster_align = cluster_options->align().AsEnum();
    }
    if (cluster_options->hasBaseline()) {
      cluster_baseline = cluster_options->baseline().AsEnum();
    }
  }
  DrawTextInternal(text_cluster->text(), cluster_x + x, cluster_y + y,
                   CanvasRenderingContext2DState::kFillPaintType, cluster_align,
                   cluster_baseline, text_cluster->start(), text_cluster->end(),
                   nullptr, text_cluster->textMetrics()->GetFont());
}

void BaseRenderingContext2D::strokeText(const String& text,
                                        double x,
                                        double y) {
  CanvasRenderingContext2DState& state = GetState();
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType,
                   state.GetTextAlign(), state.GetTextBaseline(), 0,
                   text.length());
}

void BaseRenderingContext2D::strokeText(const String& text,
                                        double x,
                                        double y,
                                        double max_width) {
  CanvasRenderingContext2DState& state = GetState();
  DrawTextInternal(text, x, y, CanvasRenderingContext2DState::kStrokePaintType,
                   state.GetTextAlign(), state.GetTextBaseline(), 0,
                   text.length(), &max_width);
}

void BaseRenderingContext2D::strokeTextCluster(const TextCluster* text_cluster,
                                               double x,
                                               double y) {
  strokeTextCluster(text_cluster, x, y, /*cluster_options=*/nullptr);
}

void BaseRenderingContext2D::strokeTextCluster(
    const TextCluster* text_cluster,
    double x,
    double y,
    const TextClusterOptions* cluster_options) {
  DCHECK(text_cluster);
  V8CanvasTextAlign::Enum cluster_align = text_cluster->align().AsEnum();
  V8CanvasTextBaseline::Enum cluster_baseline =
      text_cluster->baseline().AsEnum();
  double cluster_x = text_cluster->x();
  double cluster_y = text_cluster->y();
  if (cluster_options != nullptr) {
    if (cluster_options->hasX()) {
      cluster_x = cluster_options->x();
    }
    if (cluster_options->hasY()) {
      cluster_y = cluster_options->y();
    }
    if (cluster_options->hasAlign()) {
      cluster_align = cluster_options->align().AsEnum();
    }
    if (cluster_options->hasBaseline()) {
      cluster_baseline = cluster_options->baseline().AsEnum();
    }
  }
  DrawTextInternal(text_cluster->text(), cluster_x + x, cluster_y + y,
                   CanvasRenderingContext2DState::kStrokePaintType,
                   cluster_align, cluster_baseline, text_cluster->start(),
                   text_cluster->end(), nullptr,
                   text_cluster->textMetrics()->GetFont());
}

void BaseRenderingContext2D::DrawTextInternal(
    const String& text,
    double x,
    double y,
    CanvasRenderingContext2DState::PaintType paint_type,
    V8CanvasTextAlign::Enum align,
    V8CanvasTextBaseline::Enum baseline,
    unsigned run_start,
    unsigned run_end,
    double* max_width,
    const Font* cluster_font) {
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();
  if (canvas) {
    // The style resolution required for fonts is not available in frame-less
    // documents.
    if (!canvas->GetDocument().GetFrame()) {
      return;
    }

    // accessFont needs the style to be up to date, but updating style can cause
    // script to run, (e.g. due to autofocus) which can free the canvas (set
    // size to 0, for example), so update style before grabbing the PaintCanvas.
    canvas->GetDocument().UpdateStyleAndLayoutTreeForElement(
        canvas, DocumentUpdateReason::kCanvas);
  }

  // Abort if we don't have a paint canvas (e.g. the context was lost).
  cc::PaintCanvas* paint_canvas = GetOrCreatePaintCanvas();
  if (!paint_canvas) {
    return;
  }

  if (!std::isfinite(x) || !std::isfinite(y)) {
    return;
  }
  if (max_width && (!std::isfinite(*max_width) || *max_width <= 0)) {
    return;
  }

  const Font* font =
      (cluster_font != nullptr) ? cluster_font : AccessFont(canvas);
  const SimpleFontData* font_data = font->PrimaryFont();
  DCHECK(font_data);
  if (!font_data) {
    return;
  }

  // FIXME: Need to turn off font smoothing.

  const CanvasRenderingContext2DState& state = GetState();
  const ComputedStyle* computed_style =
      canvas ? canvas->EnsureComputedStyle() : nullptr;
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  TextDirection direction =
      ToTextDirection(state.GetDirection(), host, computed_style);
  bool is_rtl = direction == TextDirection::kRtl;
  bool bidi_override =
      computed_style ? IsOverride(computed_style->GetUnicodeBidi()) : false;

  PlainTextPainter& text_painter = host->GetPlainTextPainter();
  TextRun text_run(text, direction, bidi_override);
  // Draw the item text at the correct point.
  gfx::PointF location(ClampTo<float>(x), ClampTo<float>(y));
  gfx::RectF bounds;
  double font_width = 0;
  if (run_start == 0 && run_end == text.length()) [[likely]] {
    font_width = text_painter.ComputeInlineSize(text_run, *font, &bounds);
  } else {
    font_width = text_painter.ComputeSubInlineSize(text_run, run_start, run_end,
                                                   *font, &bounds);
  }

  bool use_max_width = (max_width && *max_width < font_width);
  double width = use_max_width ? *max_width : font_width;

  if (align == V8CanvasTextAlign::Enum::kStart) {
    align = is_rtl ? V8CanvasTextAlign::Enum::kRight
                   : V8CanvasTextAlign::Enum::kLeft;
  } else if (align == V8CanvasTextAlign::Enum::kEnd) {
    align = is_rtl ? V8CanvasTextAlign::Enum::kLeft
                   : V8CanvasTextAlign::Enum::kRight;
  }

  switch (align) {
    case V8CanvasTextAlign::Enum::kCenter:
      location.set_x(location.x() - width / 2);
      break;
    case V8CanvasTextAlign::Enum::kRight:
      location.set_x(location.x() - width);
      break;
    case V8CanvasTextAlign::Enum::kEnd:
    case V8CanvasTextAlign::Enum::kLeft:
    case V8CanvasTextAlign::Enum::kStart:
      break;
  }

  location.Offset(0, TextMetrics::GetFontBaseline(baseline, *font_data));

  bounds.Offset(location.x(), location.y());
  if (paint_type == CanvasRenderingContext2DState::kStrokePaintType) {
    InflateStrokeRect(bounds);
  }

  if (use_max_width) {
    paint_canvas->save();
    // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op)
    // still work. As the width of canvas is scaled, so text can be scaled to
    // match the given maxwidth, update text location so it appears on desired
    // place.
    paint_canvas->scale(ClampTo<float>(width / font_width), 1);
    location.set_x(location.x() / ClampTo<float>(width / font_width));
  }

  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [font, text = std::move(text), direction, bidi_override, location,
       run_start, run_end, canvas, &text_painter](MemoryManagedPaintCanvas* c,
                                                  const cc::PaintFlags* flags) {
        TextRun text_run(text, direction, bidi_override);
        // Font::DrawType::kGlyphsAndClusters is required for printing to PDF,
        // otherwise the character to glyph mapping will not be reversible,
        // which prevents text data from being extracted from PDF files or
        // from the print preview. This is only needed in vector printing mode
        // (i.e. when rendering inside the beforeprint event listener),
        // because in all other cases the canvas is just a rectangle of pixels.
        // Note: Test coverage for this is assured by manual (non-automated)
        // web test printing/manual/canvas2d-vector-text.html
        // That test should be run manually against CLs that touch this code.
        Font::DrawType draw_type = (canvas && canvas->IsPrinting())
                                       ? Font::DrawType::kGlyphsAndClusters
                                       : Font::DrawType::kGlyphsOnly;
        text_painter.DrawWithBidiReorder(text_run, run_start, run_end, *font,
                                         Font::kUseFallbackIfFontNotReady, *c,
                                         location, *flags, draw_type);
      },
      NoOverdraw, bounds, paint_type, CanvasRenderingContext2DState::kNoImage,
      CanvasPerformanceMonitor::DrawType::kText);

  if (use_max_width) {
    // Make sure that `paint_canvas` is still valid and active. Calling `Draw`
    // might reset `paint_canvas`. If that happens, `GetOrCreatePaintCanvas`
    // will create a new `paint_canvas` and return a new address. This new
    // canvas won't have the `save()` added above, so it would be invalid to
    // call `restore()` here.
    if (paint_canvas == GetOrCreatePaintCanvas()) {
      paint_canvas->restore();
    }
  }
  ValidateStateStack();
}

TextMetrics* BaseRenderingContext2D::measureText(const String& text) {
  // The style resolution required for fonts is not available in frame-less
  // documents.
  HTMLCanvasElement* canvas = HostAsHTMLCanvasElement();

  if (canvas) {
    if (!canvas->GetDocument().GetFrame()) {
      return MakeGarbageCollected<TextMetrics>();
    }

    canvas->GetDocument().UpdateStyleAndLayoutTreeForElement(
        canvas, DocumentUpdateReason::kCanvas);
  }

  const Font* font = AccessFont(canvas);

  const CanvasRenderingContext2DState& state = GetState();
  const ComputedStyle* computed_style =
      canvas ? canvas->EnsureComputedStyle() : nullptr;
  CanvasRenderingContextHost* host = GetCanvasRenderingContextHost();
  TextDirection direction =
      ToTextDirection(state.GetDirection(), host, computed_style);

  return MakeGarbageCollected<TextMetrics>(
      font, direction, state.GetTextBaseline(), state.GetTextAlign(), text,
      host->GetPlainTextPainter());
}

String BaseRenderingContext2D::lang() const {
  return GetState().GetLang();
}

void BaseRenderingContext2D::setLang(const String& lang_string) {
  CanvasRenderingContext2DState& state = GetState();
  if (state.GetLang() == lang_string) {
    return;
  }

  state.SetLang(lang_string);

  // If the font has been realized, reset it to account for the new lang
  // setting. When not yet realized, the lang will be accounted for the first
  // time the font is realized.
  if (state.HasRealizedFont()) {
    setFont(font());
  }
}

const LayoutLocale* BaseRenderingContext2D::LocaleFromLang() {
  String lang_string = GetState().GetLang();
  if (lang_string == kInheritString) {
    return GetCanvasRenderingContextHost()->GetLocale();
  }

  return &LayoutLocale::ValueOrDefault(
      LayoutLocale::Get(AtomicString(lang_string)));
}

void BaseRenderingContext2D::setLetterSpacing(const String& letter_spacing) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DLetterSpacing);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  state.SetLetterSpacing(letter_spacing, GetFontSelector());
}

void BaseRenderingContext2D::setWordSpacing(const String& word_spacing) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DWordSpacing);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  state.SetWordSpacing(word_spacing, GetFontSelector());
}

void BaseRenderingContext2D::setTextRendering(
    const V8CanvasTextRendering& text_rendering) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DTextRendering);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  if (state.GetTextRendering() == text_rendering) {
    return;
  }
  state.SetTextRendering(text_rendering.AsEnum(), GetFontSelector());
}

void BaseRenderingContext2D::setFontKerning(
    const V8CanvasFontKerning font_kerning) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontKerning);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }
  FontDescription::Kerning kerning = state.GetFontKerning();
  switch (font_kerning.AsEnum()) {
    case V8CanvasFontKerning::Enum::kAuto:
      kerning = FontDescription::kAutoKerning;
      break;
    case V8CanvasFontKerning::Enum::kNone:
      kerning = FontDescription::kNoneKerning;
      break;
    case V8CanvasFontKerning::Enum::kNormal:
      kerning = FontDescription::kNormalKerning;
      break;
  }

  state.SetFontKerning(kerning, GetFontSelector());
}

void BaseRenderingContext2D::setFontStretch(
    const V8CanvasFontStretch& font_stretch) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontStretch);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }

  if (state.GetFontStretch() == font_stretch) {
    return;
  }
  state.SetFontStretch(font_stretch.AsEnum(), GetFontSelector());
}

void BaseRenderingContext2D::setFontVariantCaps(
    const V8CanvasFontVariantCaps& font_variant_caps) {
  UseCounter::Count(GetTopExecutionContext(),
                    WebFeature::kCanvasRenderingContext2DFontVariantCaps);
  CanvasRenderingContext2DState& state = GetState();
  if (!state.HasRealizedFont()) {
    setFont(font());
  }
  FontDescription::FontVariantCaps variant_caps = state.GetFontVariantCaps();
  switch (font_variant_caps.AsEnum()) {
    case (V8CanvasFontVariantCaps::Enum::kNormal):
      variant_caps = FontDescription::kCapsNormal;
      break;
    case (V8CanvasFontVariantCaps::Enum::kSmallCaps):
      variant_caps = FontDescription::kSmallCaps;
      break;
    case (V8CanvasFontVariantCaps::Enum::kAllSmallCaps):
      variant_caps = FontDescription::kAllSmallCaps;
      break;
    case (V8CanvasFontVariantCaps::Enum::kPetiteCaps):
      variant_caps = FontDescription::kPetiteCaps;
      break;
    case (V8CanvasFontVariantCaps::Enum::kAllPetiteCaps):
      variant_caps = FontDescription::kAllPetiteCaps;
      break;
    case (V8CanvasFontVariantCaps::Enum::kUnicase):
      variant_caps = FontDescription::kUnicase;
      break;
    case (V8CanvasFontVariantCaps::Enum::kTitlingCaps):
      variant_caps = FontDescription::kTitlingCaps;
      break;
  }

  state.SetFontVariantCaps(variant_caps, GetFontSelector());
}

UniqueFontSelector* BaseRenderingContext2D::GetFontSelector() const {
  return nullptr;
}
int BaseRenderingContext2D::LayerCount() const {
  return Canvas2DRecorderContext::LayerCount();
}

DOMMatrix* BaseRenderingContext2D::drawElementImage(
    const V8UnionElementOrElementImage* element,
    double dx,
    double dy,
    ExceptionState& exception_state) {
  return DrawElementInternal(
      element,
      /*sx*/ std::nullopt, /*sy*/ std::nullopt,
      /*swidth*/ std::nullopt, /*sheight*/ std::nullopt, dx, dy,
      /*dwidth*/ std::nullopt, /*dheight*/ std::nullopt, exception_state);
}

DOMMatrix* BaseRenderingContext2D::drawElementImage(
    const V8UnionElementOrElementImage* element,
    double dx,
    double dy,
    double dwidth,
    double dheight,
    ExceptionState& exception_state) {
  return DrawElementInternal(element,
                             /*sx*/ std::nullopt, /*sy*/ std::nullopt,
                             /*swidth*/ std::nullopt, /*sheight*/ std::nullopt,
                             dx, dy, dwidth, dheight, exception_state);
}

DOMMatrix* BaseRenderingContext2D::drawElementImage(
    const V8UnionElementOrElementImage* element,
    double sx,
    double sy,
    double swidth,
    double sheight,
    double dx,
    double dy,
    ExceptionState& exception_state) {
  return DrawElementInternal(element, sx, sy, swidth, sheight, dx, dy,
                             /*dwidth*/ std::nullopt, /*dheight*/ std::nullopt,
                             exception_state);
}

DOMMatrix* BaseRenderingContext2D::drawElementImage(
    const V8UnionElementOrElementImage* element,
    double sx,
    double sy,
    double swidth,
    double sheight,
    double dx,
    double dy,
    double dwidth,
    double dheight,
    ExceptionState& exception_state) {
  return DrawElementInternal(element, sx, sy, swidth, sheight, dx, dy, dwidth,
                             dheight, exception_state);
}

DOMMatrix* BaseRenderingContext2D::DrawElementInternal(
    const V8UnionElementOrElementImage* element,
    std::optional<double> sx,
    std::optional<double> sy,
    std::optional<double> swidth,
    std::optional<double> sheight,
    double x,
    double y,
    std::optional<double> dwidth,
    std::optional<double> dheight,
    ExceptionState& exception_state) {
  CHECK(RuntimeEnabledFeatures::CanvasDrawElementEnabled(
      GetCanvasRenderingContextHost()->GetTopExecutionContext()));

  if (!GetOrCreatePaintCanvas()) {
    return DOMMatrix::Create();
  }

  if (!IsDrawElementImageEligible(element, "DrawElementImage",
                                  exception_state)) {
    return nullptr;
  }

  std::optional<CanvasChildPaintRecord> child_paint_record;
  if (element->IsElement()) {
    child_paint_record = GetChildPaintRecord(element->GetAsElement());
  } else if (element->IsElementImage()) {
    if (const auto& record = element->GetAsElementImage()->PaintRecord()) {
      child_paint_record = *record;
    }
  }

  TRACE_EVENT0("blink", "DrawElementImage");

  if (!child_paint_record) {
    if (element->IsElementImage()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The ElementImage has been closed.");
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "No cached paint record for element.");
    }
    return nullptr;
  }

  float dpr = child_paint_record->paint_state.effective_zoom;
  gfx::RectF src_rect(child_paint_record->paint_state.box_size);
  if (sx && sy && swidth && sheight) {
    src_rect = gfx::RectF(*sx * dpr, *sy * dpr, *swidth * dpr, *sheight * dpr);
  }

  if (src_rect.IsEmpty()) {
    return DOMMatrix::Create();
  }

  // The filter needs to be resolved before calling Draw, because it
  // immediately checks IsFilterResolved() and uses a null canvas if not.
  StateGetFilter();

  // The ideal size is the source content size, represented in canvas grid
  // coordinates. This will cause the element to have the same proportions when
  // appearing inside the canvas as it would have were it painted outside the
  // canvas.
  gfx::SizeF ideal_dst_size(src_rect.size());
  gfx::Vector2dF scale_factor =
      GetCanvasGridScaleFactor(child_paint_record->paint_state, Host()->Size());
  ideal_dst_size.Scale(scale_factor.x(), scale_factor.y());

  gfx::RectF dst_rect(x, y, 0, 0);
  if (dwidth && dheight) {
    dst_rect.set_size(gfx::SizeF(*dwidth, *dheight));
  } else {
    // If no explicit destination size is given, default to the ideal size.
    dst_rect.set_size(ideal_dst_size);
  }

  if (dst_rect.IsEmpty()) {
    return DOMMatrix::Create();
  }

  cc::PaintRecord paint_record = std::move(child_paint_record->record);
  uint32_t animated_image_frame_index_map_pos =
      animated_image_frame_index_maps_.size();
  animated_image_frame_index_maps_.emplace_back(
      child_paint_record->paint_state.animated_image_frame_index_map);
  // TODO(crbug.com/421834883): This code is based on image drawing. Maybe we
  // need a distinct paint_type: kImagePaintType seems to do the right thing
  // but maybe its treatment of anti-aliasing is incorrect. The kNonOpaqueImage
  // type controls drop shadow painting under transforms. It's not clear if we
  // should behave like a non-opaque image here, but the element may not be
  // opaque so going with that for now.
  Draw<OverdrawOp::kNone>(
      /*draw_func=*/
      [paint_record, dst_rect, src_rect, animated_image_frame_index_map_pos](
          MemoryManagedPaintCanvas* c, const cc::PaintFlags* flags) {
        cc::RecordPaintCanvas::DisableFlushCheckScope disable_flush_check_scope(
            static_cast<cc::RecordPaintCanvas*>(c));
        int initial_save_count = c->getSaveCount();

        if (flags->getImageFilter() ||
            flags->getBlendMode() != SkBlendMode::kSrcOver ||
            SkColorGetA(flags->getColor()) < 255) {
          SkM44 ctm = c->getLocalToDevice();
          SkM44 inv_ctm;
          if (!ctm.invert(&inv_ctm)) {
            // There is an earlier check for invertibility, but the arithmetic
            // in AffineTransform is not exactly identical, so it is possible
            // for SkMatrix to find the transform to be non-invertible at this
            // stage. crbug.com/504687
            return;
          }
          SkRect bounds = gfx::RectFToSkRect(dst_rect);
          ctm.asM33().mapRect(&bounds);
          if (!bounds.isFinite()) {
            // There is an earlier check for the correctness of the bounds, but
            // it is possible that after applying the matrix transformation we
            // get a faulty set of bounds, so we want to catch this asap and
            // avoid sending a draw command. crbug.com/1039125 We want to do
            // this before the save command is sent.
            return;
          }
          c->save();
          c->concat(inv_ctm);

          cc::PaintFlags layer_flags;
          layer_flags.setBlendMode(flags->getBlendMode());
          layer_flags.setImageFilter(flags->getImageFilter());
          layer_flags.setColor(flags->getColor());

          c->saveLayer(bounds, layer_flags);
          c->concat(ctm);
        }

        c->save();
        c->translate(dst_rect.x(), dst_rect.y());
        if (dst_rect.width() != src_rect.width() ||
            dst_rect.height() != src_rect.height()) {
          c->scale(dst_rect.width() / src_rect.width(),
                   dst_rect.height() / src_rect.height());
        }
        c->translate(-src_rect.x(), -src_rect.y());

        c->clipRect(SkRect::MakeXYWH(src_rect.x(), src_rect.y(),
                                     src_rect.width(), src_rect.height()));

        // A CustomDataOp callback will be used to apply the animated image
        // frame index map for this ElementImage prior to processing
        // DrawImage(Rect)Op's in this paint_record.
        c->recordCustomData(animated_image_frame_index_map_pos);

        c->drawPicture(std::move(paint_record),
                       // use a save at the beginning of the record to keep
                       // transforms local:
                       true);

        // Reset the animated image frame index map
        c->recordCustomData(std::numeric_limits<uint32_t>::max());

        c->restoreToCount(initial_save_count);
      },
      NoOverdraw, /*bounds=*/dst_rect,
      CanvasRenderingContext2DState::kImagePaintType,
      CanvasRenderingContext2DState::kNonOpaqueImage,
      CanvasPerformanceMonitor::DrawType::kElement);

  // Compute the transform, in canvas grid coordinates, that we just drew with.
  // We start from the context's CTM, then offset by x,y, and finally apply any
  // dest scaling.
  gfx::Transform draw_transform = GetState().GetTransform().ToTransform();
  draw_transform.Translate(x, y);
  // The drawing commands above scale by `dst_rect.size() / src_rect.size()`,
  // which does two things: 1) scales the drawing commands of `paint_record` (in
  // physical pixels) to canvas grid coordinates, and 2) applies any additional
  // dest scaling. We are only returning #2 in the logic below.
  draw_transform.Scale(dst_rect.width() / ideal_dst_size.width(),
                       dst_rect.height() / ideal_dst_size.height());

  if (sx && sy) {
    draw_transform.Translate(-src_rect.x() * scale_factor.x(),
                             -src_rect.y() * scale_factor.y());
  }

  // This call will take our draw transform in canvas grid coordinates, and
  // convert it to a transform in CSS pixels suitable for positioning the
  // element.
  gfx::Transform result_transform = blink::GetElementTransform(
      child_paint_record->paint_state, Host()->Size(), draw_transform);

  return MakeGarbageCollected<DOMMatrix>(result_transform,
                                         result_transform.Is2dTransform());
}

scoped_refptr<const cc::AnimatedImageFrameIndexMap>
BaseRenderingContext2D::GetAnimatedImageFrameIndexMap(uint32_t id) const {
  DCHECK(id == std::numeric_limits<uint32_t>::max() ||
         id < animated_image_frame_index_maps_.size());
  if (id < animated_image_frame_index_maps_.size()) {
    return animated_image_frame_index_maps_[id];
  }
  return nullptr;
}

void BaseRenderingContext2D::DidFlush() {
  animated_image_frame_index_maps_.clear();
}

}  // namespace blink
