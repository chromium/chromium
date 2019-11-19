// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "base/feature_list.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {
const base::Feature kLowLatencyCanvas2dSwapChain{
    "LowLatencyCanvas2dSwapChain", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace

CanvasRenderingContextHost::CanvasRenderingContextHost(HostType host_type)
    : host_type_(host_type) {}

void CanvasRenderingContextHost::RecordCanvasSizeToUMA(const IntSize& size) {
  if (did_record_canvas_size_to_uma_)
    return;
  did_record_canvas_size_to_uma_ = true;

  if (host_type_ == kCanvasHost) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                std::sqrt(size.Area()), 1, 5000, 100);
  } else if (host_type_ == kOffscreenCanvasHost) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                std::sqrt(size.Area()), 1, 5000, 100);
  } else {
    NOTREACHED();
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
        CanvasResourceProvider::ResourceUsage usage;
        if (SharedGpuContext::IsGpuCompositingEnabled()) {
          if (LowLatencyEnabled()) {
            usage = CanvasResourceProvider::ResourceUsage::
                kAcceleratedDirect3DResourceUsage;
          } else {
            usage = CanvasResourceProvider::ResourceUsage::
                kAcceleratedCompositedResourceUsage;
          }
        } else {
          usage = CanvasResourceProvider::ResourceUsage::
              kSoftwareCompositedResourceUsage;
        }

        uint8_t presentation_mode =
            CanvasResourceProvider::kDefaultPresentationMode;
        if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
          presentation_mode |=
              CanvasResourceProvider::kAllowImageChromiumPresentationMode;
        }
        // Allow swap chain presentation only if 3d context is using a swap
        // chain since we'll be importing it as a passthrough texture.
        if (RenderingContext() && RenderingContext()->UsingSwapChain()) {
          presentation_mode |=
              CanvasResourceProvider::kAllowSwapChainPresentationMode;
        }

        ReplaceResourceProvider(CanvasResourceProvider::CreateForCanvas(
            Size(), usage, SharedGpuContext::ContextProviderWrapper(),
            0 /* msaa_sample_count */, FilterQuality(), ColorParams(),
            presentation_mode, std::move(dispatcher),
            RenderingContext()->IsOriginTopLeft()));
      } else {
        DCHECK(Is2d());
        const bool want_acceleration =
            hint == kPreferAcceleration && ShouldAccelerate2dContext();

        CanvasResourceProvider::ResourceUsage usage;
        if (want_acceleration) {
          if (LowLatencyEnabled()) {
            usage = CanvasResourceProvider::ResourceUsage::
                kAcceleratedDirect2DResourceUsage;
          } else {
            usage = CanvasResourceProvider::ResourceUsage::
                kAcceleratedCompositedResourceUsage;
          }
        } else {
          if (LowLatencyEnabled()) {
            usage = CanvasResourceProvider::ResourceUsage::
                kSoftwareCompositedDirect2DResourceUsage;
          } else {
            usage = CanvasResourceProvider::ResourceUsage::
                kSoftwareCompositedResourceUsage;
          }
        }

        uint8_t presentation_mode =
            CanvasResourceProvider::kDefaultPresentationMode;
        // Allow GMB image resources if the runtime feature is enabled or if
        // we want to use it for low latency mode.
        if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled() ||
            (LowLatencyEnabled() && want_acceleration)) {
          presentation_mode |=
              CanvasResourceProvider::kAllowImageChromiumPresentationMode;
        }
        // Allow swap chains only if the runtime feature is enabled and we're
        // in low latency mode too.
        if (base::FeatureList::IsEnabled(kLowLatencyCanvas2dSwapChain) &&
            LowLatencyEnabled() && want_acceleration) {
          presentation_mode |=
              CanvasResourceProvider::kAllowSwapChainPresentationMode;
        }

        // It is important to not use the context's IsOriginTopLeft() here
        // because that denotes the current state and could change after the
        // new resource provider is created e.g. due to switching between
        // unaccelerated and accelerated modes during tab switching.
        const bool is_origin_top_left =
            !want_acceleration || LowLatencyEnabled();

        ReplaceResourceProvider(CanvasResourceProvider::CreateForCanvas(
            Size(), usage, SharedGpuContext::ContextProviderWrapper(),
            GetMSAASampleCountFor2dContext(), FilterQuality(), ColorParams(),
            presentation_mode, std::move(dispatcher), is_origin_top_left));

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
    const ImageEncodeOptions* options,
    ExceptionState& exception_state) {
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

  // It's possible that there are recorded commands that have not been resolved
  // Finalize frame will be called in GetImage, but if there's no
  // resourceProvider yet then the IsPaintable check will fail
  if (RenderingContext())
    RenderingContext()->FinalizeFrame();

  if (!this->IsPaintable() || Size().IsEmpty()) {
    error_msg << "The size of " << object_name << " is zero.";
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

  base::TimeTicks start_time = base::TimeTicks::Now();
  scoped_refptr<StaticBitmapImage> image_bitmap =
      RenderingContext()->GetImage(kPreferNoAcceleration);
  if (image_bitmap) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kHTMLCanvasConvertToBlobPromise;
    if (this->IsOffscreenCanvas()) {
      function_type =
          CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    }
    auto* async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options, function_type, start_time,
        ExecutionContext::From(script_state), resolver);
    async_creator->ScheduleAsyncBlobCreation(options->quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return ScriptPromise();
}

bool CanvasRenderingContextHost::IsOffscreenCanvas() const {
  return host_type_ == kOffscreenCanvasHost;
}

}  // namespace blink
