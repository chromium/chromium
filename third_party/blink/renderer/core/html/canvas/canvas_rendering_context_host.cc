// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

CanvasRenderingContextHost::CanvasRenderingContextHost() = default;

void CanvasRenderingContextHost::RecordCanvasSizeToUMA(unsigned width,
                                                       unsigned height,
                                                       bool isOffscreen) {
  if (isOffscreen) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                std::sqrt(width * height), 1, 5000, 100);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                std::sqrt(width * height), 1, 5000, 100);
  }
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage(const IntSize& size) const {
  if (!IsValidImageSize(size))
    return nullptr;
  CanvasColorParams color_params = CanvasColorParams();
  if (RenderingContext())
    color_params = RenderingContext()->ColorParams();
  SkImageInfo info = SkImageInfo::Make(
      size.Width(), size.Height(), color_params.GetSkColorType(),
      kPremul_SkAlphaType, color_params.GetSkColorSpaceForSkSurfaces());
  sk_sp<SkSurface> surface =
      SkSurface::MakeRaster(info, info.minRowBytes(), nullptr);
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasRenderingContextHost::Commit(scoped_refptr<CanvasResource>,
                                        const SkIRect&) {
  NOTIMPLEMENTED();
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize(Size());
}

void CanvasRenderingContextHost::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* canvas) const {
  if (RenderingContext())
    RenderingContext()->RestoreCanvasMatrixClipStack(canvas);
}

bool CanvasRenderingContextHost::Is3d() const {
  return RenderingContext() && RenderingContext()->Is3d();
}

bool CanvasRenderingContextHost::Is2d() const {
  return RenderingContext() && RenderingContext()->Is2d();
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider(
    AccelerationHint hint) {
  return GetOrCreateCanvasResourceProviderImpl(hint);
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProviderImpl(
    AccelerationHint hint) {
  if (!ResourceProvider() && !did_fail_to_create_resource_provider_) {
    if (IsValidImageSize(Size())) {
      base::WeakPtr<CanvasResourceDispatcher> dispatcher =
          GetOrCreateResourceDispatcher()
              ? GetOrCreateResourceDispatcher()->GetWeakPtr()
              : nullptr;
      if (Is3d()) {
        const CanvasResourceProvider::ResourceUsage usage =
            SharedGpuContext::IsGpuCompositingEnabled()
                ? CanvasResourceProvider::kAcceleratedCompositedResourceUsage
                : CanvasResourceProvider::kSoftwareCompositedResourceUsage;

        CanvasResourceProvider::PresentationMode presentation_mode =
            RuntimeEnabledFeatures::WebGLImageChromiumEnabled()
                ? CanvasResourceProvider::kAllowImageChromiumPresentationMode
                : CanvasResourceProvider::kDefaultPresentationMode;

        const bool is_origin_top_left =
            !SharedGpuContext::IsGpuCompositingEnabled();

        ReplaceResourceProvider(CanvasResourceProvider::Create(
            Size(), usage, SharedGpuContext::ContextProviderWrapper(),
            0 /* msaa_sample_count */, ColorParams(), presentation_mode,
            std::move(dispatcher), is_origin_top_left));
      } else {
        DCHECK(Is2d());
        const bool want_acceleration =
            hint == kPreferAcceleration && ShouldAccelerate2dContext();

        const CanvasResourceProvider::ResourceUsage usage =
            want_acceleration
                ? CanvasResourceProvider::kAcceleratedCompositedResourceUsage
                : CanvasResourceProvider::kSoftwareCompositedResourceUsage;

        const CanvasResourceProvider::PresentationMode presentation_mode =
            (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled() ||
             (LowLatencyEnabled() && want_acceleration))
                ? CanvasResourceProvider::kAllowImageChromiumPresentationMode
                : CanvasResourceProvider::kDefaultPresentationMode;

        const bool is_origin_top_left =
            !want_acceleration || LowLatencyEnabled();

        ReplaceResourceProvider(CanvasResourceProvider::Create(
            Size(), usage, SharedGpuContext::ContextProviderWrapper(),
            GetMSAASampleCountFor2dContext(), ColorParams(), presentation_mode,
            std::move(dispatcher), is_origin_top_left));

        if (ResourceProvider()) {
          // Always save an initial frame, to support resetting the top level
          // matrix and clip.
          ResourceProvider()->Canvas()->save();
          ResourceProvider()->SetFilterQuality(FilterQuality());
          ResourceProvider()->SetResourceRecyclingEnabled(true);
        }
      }
    }
    if (!ResourceProvider())
      did_fail_to_create_resource_provider_ = true;
  }
  return ResourceProvider();
}

CanvasColorParams CanvasRenderingContextHost::ColorParams() const {
  if (RenderingContext())
    return RenderingContext()->ColorParams();
  return CanvasColorParams();
}

ScriptPromise CanvasRenderingContextHost::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions& options,
    ExceptionState& exception_state) const {
  WTF::String object_name = "Canvas";
  if (this->IsOffscreenCanvas())
    object_name = "OffscreenCanvas";
  std::stringstream error_msg;

  if (this->IsOffscreenCanvas() && this->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return ScriptPromise();
  }

  if (!this->OriginClean()) {
    error_msg << "Tainted " << object_name << " may not be exported.";
    exception_state.ThrowSecurityError(error_msg.str().c_str());
    return ScriptPromise();
  }

  if (!this->IsPaintable() || Size().IsEmpty()) {
    error_msg << "The size of " << object_name << " iz zero.";
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  if (!RenderingContext()) {
    error_msg << object_name << " has no rendering context.";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  TimeTicks start_time = WTF::CurrentTimeTicks();
  scoped_refptr<StaticBitmapImage> image_bitmap =
      RenderingContext()->GetImage(kPreferNoAcceleration);
  if (image_bitmap) {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::Create(script_state);
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kHTMLCanvasConvertToBlobPromise;
    if (this->IsOffscreenCanvas()) {
      function_type =
          CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    }
    CanvasAsyncBlobCreator* async_creator = CanvasAsyncBlobCreator::Create(
        image_bitmap, options, function_type, start_time,
        ExecutionContext::From(script_state), resolver);
    async_creator->ScheduleAsyncBlobCreation(options.quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return ScriptPromise();
}

}  // namespace blink
