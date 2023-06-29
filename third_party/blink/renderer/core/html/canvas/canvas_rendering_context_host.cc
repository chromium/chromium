// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

CanvasRenderingContextHost::CanvasRenderingContextHost(HostType host_type)
    : host_type_(host_type) {}

void CanvasRenderingContextHost::RecordCanvasSizeToUMA(const gfx::Size& size) {
  if (did_record_canvas_size_to_uma_)
    return;
  did_record_canvas_size_to_uma_ = true;

  switch (host_type_) {
    case HostType::kNone:
      NOTREACHED();
      break;
    case HostType::kCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                  std::sqrt(size.Area64()), 1, 5000, 100);
      break;
    case HostType::kOffscreenCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                  std::sqrt(size.Area64()), 1, 5000, 100);
      break;
  }
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage(
    const gfx::Size& size) const {
  if (!IsValidImageSize(size))
    return nullptr;
  SkImageInfo info = SkImageInfo::Make(
      gfx::SizeToSkISize(size),
      GetRenderingContextSkColorInfo().makeAlphaType(kPremul_SkAlphaType));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(info, info.minRowBytes(), nullptr);
  if (!surface)
    return nullptr;
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

void CanvasRenderingContextHost::Commit(scoped_refptr<CanvasResource>&&,
                                        const SkIRect&) {
  NOTIMPLEMENTED();
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize(Size());
}

bool CanvasRenderingContextHost::PrintedInCurrentTask() const {
  return RenderingContext() && RenderingContext()->did_print_in_current_task();
}

void CanvasRenderingContextHost::RestoreCanvasMatrixClipStack(
    cc::PaintCanvas* canvas) const {
  if (RenderingContext())
    RenderingContext()->RestoreCanvasMatrixClipStack(canvas);
}

bool CanvasRenderingContextHost::IsWebGL() const {
  return RenderingContext() && RenderingContext()->IsWebGL();
}

bool CanvasRenderingContextHost::IsWebGPU() const {
  return RenderingContext() && RenderingContext()->IsWebGPU();
}

bool CanvasRenderingContextHost::IsRenderingContext2D() const {
  return RenderingContext() && RenderingContext()->IsRenderingContext2D();
}

bool CanvasRenderingContextHost::IsImageBitmapRenderingContext() const {
  return RenderingContext() &&
         RenderingContext()->IsImageBitmapRenderingContext();
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider(
    RasterModeHint hint) {
  return GetOrCreateCanvasResourceProviderImpl(hint);
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProviderImpl(
    RasterModeHint hint) {
  if (!ResourceProvider() && !did_fail_to_create_resource_provider_) {
    if (IsValidImageSize(Size())) {
      if (IsWebGPU()) {
        CreateCanvasResourceProviderWebGPU();
      } else if (IsWebGL()) {
        CreateCanvasResourceProviderWebGL();
      } else {
        CreateCanvasResourceProvider2D(hint);
      }
    }
    if (!ResourceProvider())
      did_fail_to_create_resource_provider_ = true;
  }
  return ResourceProvider();
}

void CanvasRenderingContextHost::CreateCanvasResourceProviderWebGPU() {
  const SkImageInfo resource_info =
      SkImageInfo::Make(SkISize::Make(Size().width(), Size().height()),
                        GetRenderingContextSkColorInfo());
  std::unique_ptr<CanvasResourceProvider> provider;
  if (SharedGpuContext::IsGpuCompositingEnabled()) {
    provider = CanvasResourceProvider::CreateWebGPUImageProvider(resource_info);
  }
  ReplaceResourceProvider(std::move(provider));
  if (ResourceProvider() && ResourceProvider()->IsValid()) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              ResourceProvider()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  ResourceProvider()->GetType());
  }
}

void CanvasRenderingContextHost::CreateCanvasResourceProviderWebGL() {
  DCHECK(IsWebGL());

  base::WeakPtr<CanvasResourceDispatcher> dispatcher =
      GetOrCreateResourceDispatcher()
          ? GetOrCreateResourceDispatcher()->GetWeakPtr()
          : nullptr;

  std::unique_ptr<CanvasResourceProvider> provider;
  const SkImageInfo resource_info =
      SkImageInfo::Make(SkISize::Make(Size().width(), Size().height()),
                        GetRenderingContextSkColorInfo());
  // Use top left origin for shared image CanvasResourceProviders since those
  // can be used for rendering with Skia, and Skia's Graphite backend doesn't
  // support bottom left origin SkSurfaces.
  constexpr bool kIsSharedImageOriginTopLeft = true;
  // Do not initialize the CRP using Skia. The CRP can have bottom left origin
  // in which case Skia Graphite won't be able to render into it, and WebGL is
  // responsible for clearing the CRP when it renders anyway and we have clear
  // rect tracking in the shared image system to enforce this.
  constexpr auto kShouldInitialize =
      CanvasResourceProvider::ShouldInitialize::kNo;
  if (SharedGpuContext::IsGpuCompositingEnabled() && LowLatencyEnabled()) {
    // If LowLatency is enabled, we need a resource that is able to perform well
    // in such mode. It will first try a PassThrough provider and, if that is
    // not possible, it will try a SharedImage with the appropriate flags.
    if ((RenderingContext() && RenderingContext()->UsingSwapChain()) ||
        RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
      // If either SwapChain is enabled or WebGLImage mode is enabled, we can
      // try a passthrough provider.
      DCHECK(LowLatencyEnabled());
      provider = CanvasResourceProvider::CreatePassThroughProvider(
          resource_info, FilterQuality(),
          SharedGpuContext::ContextProviderWrapper(), dispatcher,
          RenderingContext()->IsOriginTopLeft());
    }
    if (!provider) {
      // If PassThrough failed, try a SharedImage with usage display enabled,
      // and if WebGLImageChromium is enabled, add concurrent read write and
      // usage scanout (overlay).
      uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
      if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
        shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
        shared_image_usage_flags |=
            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
      }
      provider = CanvasResourceProvider::CreateSharedImageProvider(
          resource_info, FilterQuality(), kShouldInitialize,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          kIsSharedImageOriginTopLeft, shared_image_usage_flags);
    }
  } else if (SharedGpuContext::IsGpuCompositingEnabled()) {
    // If there is no LawLatency mode, and GPU is enabled, will try a GPU
    // SharedImage that should support Usage Display and probably Usage Canbout
    // if WebGLImageChromium is enabled.
    uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (RuntimeEnabledFeatures::WebGLImageChromiumEnabled()) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        resource_info, FilterQuality(), kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        kIsSharedImageOriginTopLeft, shared_image_usage_flags);
  }

  // If either of the other modes failed and / or it was not possible to do, we
  // will backup with a SharedBitmap, and if that was not possible with a Bitmap
  // provider.
  if (!provider) {
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        resource_info, FilterQuality(), kShouldInitialize, dispatcher);
  }
  if (!provider) {
    provider = CanvasResourceProvider::CreateBitmapProvider(
        resource_info, FilterQuality(), kShouldInitialize);
  }

  ReplaceResourceProvider(std::move(provider));
  if (ResourceProvider() && ResourceProvider()->IsValid()) {
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              ResourceProvider()->IsAccelerated());
    base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                  ResourceProvider()->GetType());
  }
}

void CanvasRenderingContextHost::CreateCanvasResourceProvider2D(
    RasterModeHint hint) {
  DCHECK(IsRenderingContext2D() || IsImageBitmapRenderingContext());
  base::WeakPtr<CanvasResourceDispatcher> dispatcher =
      GetOrCreateResourceDispatcher()
          ? GetOrCreateResourceDispatcher()->GetWeakPtr()
          : nullptr;

  std::unique_ptr<CanvasResourceProvider> provider;
  const SkImageInfo resource_info =
      SkImageInfo::Make(SkISize::Make(Size().width(), Size().height()),
                        GetRenderingContextSkColorInfo());
  const bool use_gpu =
      hint == RasterModeHint::kPreferGPU && ShouldAccelerate2dContext();
  constexpr auto kShouldInitialize =
      CanvasResourceProvider::ShouldInitialize::kCallClear;
  // It is important to not use the context's IsOriginTopLeft() here
  // because that denotes the current state and could change after the
  // new resource provider is created e.g. due to switching between
  // unaccelerated and accelerated modes during tab switching.
  constexpr bool kIsOriginTopLeft = true;
  if (use_gpu && LowLatencyEnabled()) {
    // If we can use the gpu and low latency is enabled, we will try to use a
    // SwapChain if possible.
    provider = CanvasResourceProvider::CreateSwapChainProvider(
        resource_info, FilterQuality(), kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), dispatcher,
        kIsOriginTopLeft);
    // If SwapChain failed or it was not possible, we will try a SharedImage
    // with a set of flags trying to add Usage Display and Usage Scanout and
    // Concurrent Read and Write if possible.
    if (!provider) {
      uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
      if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled() ||
          base::FeatureList::IsEnabled(
              features::kLowLatencyCanvas2dImageChromium)) {
        shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
        shared_image_usage_flags |=
            gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;
      }
      provider = CanvasResourceProvider::CreateSharedImageProvider(
          resource_info, FilterQuality(), kShouldInitialize,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          kIsOriginTopLeft, shared_image_usage_flags);
    }
  } else if (use_gpu) {
    // First try to be optimized for displaying on screen. In the case we are
    // hardware compositing, we also try to enable the usage of the image as
    // scanout buffer (overlay)
    uint32_t shared_image_usage_flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
    if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
      shared_image_usage_flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;
    }
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        resource_info, FilterQuality(), kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        kIsOriginTopLeft, shared_image_usage_flags);
  } else if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
    const uint32_t shared_image_usage_flags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    provider = CanvasResourceProvider::CreateSharedImageProvider(
        resource_info, FilterQuality(), kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kCPU,
        kIsOriginTopLeft, shared_image_usage_flags);
  }

  // If either of the other modes failed and / or it was not possible to do, we
  // will backup with a SharedBitmap, and if that was not possible with a Bitmap
  // provider.
  if (!provider) {
    provider = CanvasResourceProvider::CreateSharedBitmapProvider(
        resource_info, FilterQuality(), kShouldInitialize, dispatcher);
  }
  if (!provider) {
    provider = CanvasResourceProvider::CreateBitmapProvider(
        resource_info, FilterQuality(), kShouldInitialize);
  }

  ReplaceResourceProvider(std::move(provider));

  if (ResourceProvider()) {
    if (ResourceProvider()->IsValid()) {
      base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                                ResourceProvider()->IsAccelerated());
      base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType",
                                    ResourceProvider()->GetType());
    }
    ResourceProvider()->SetFilterQuality(FilterQuality());
    ResourceProvider()->SetResourceRecyclingEnabled(true);
  }
}

SkColorInfo CanvasRenderingContextHost::GetRenderingContextSkColorInfo() const {
  if (RenderingContext())
    return RenderingContext()->CanvasRenderingContextSkColorInfo();
  return SkColorInfo(kN32_SkColorType, kPremul_SkAlphaType,
                     SkColorSpace::MakeSRGB());
}

ScriptPromise CanvasRenderingContextHost::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions* options,
    ExceptionState& exception_state,
    const CanvasRenderingContext* const context) {
  DCHECK(IsOffscreenCanvas());
  WTF::String object_name = "OffscreenCanvas";
  std::stringstream error_msg;

  if (IsOffscreenCanvas() && IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return ScriptPromise();
  }

  if (!OriginClean()) {
    error_msg << "Tainted " << object_name << " may not be exported.";
    exception_state.ThrowSecurityError(error_msg.str().c_str());
    return ScriptPromise();
  }

  // It's possible that there are recorded commands that have not been resolved
  // Finalize frame will be called in GetImage, but if there's no
  // resourceProvider yet then the IsPaintable check will fail
  if (RenderingContext()) {
    RenderingContext()->FinalizeFrame(
        CanvasResourceProvider::FlushReason::kToBlob);
  }

  if (!IsPaintable() || Size().IsEmpty()) {
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
  scoped_refptr<StaticBitmapImage> image_bitmap = RenderingContext()->GetImage(
      CanvasResourceProvider::FlushReason::kToBlob);
  if (image_bitmap) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
        script_state, exception_state.GetContext());
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    auto* execution_context = ExecutionContext::From(script_state);
    auto* async_creator = MakeGarbageCollected<CanvasAsyncBlobCreator>(
        image_bitmap, options, function_type, start_time, execution_context,
        IdentifiabilityStudySettings::Get()->ShouldSampleType(
            IdentifiableSurface::Type::kCanvasReadback)
            ? IdentifiabilityInputDigest(context)
            : 0,
        resolver);
    async_creator->ScheduleAsyncBlobCreation(options->quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return ScriptPromise();
}

bool CanvasRenderingContextHost::IsOffscreenCanvas() const {
  return host_type_ == HostType::kOffscreenCanvasHost;
}

IdentifiableToken CanvasRenderingContextHost::IdentifiabilityInputDigest(
    const CanvasRenderingContext* const context) const {
  const uint64_t context_digest =
      context ? context->IdentifiableTextToken().ToUkmMetricValue() : 0;
  const uint64_t context_type = static_cast<uint64_t>(
      context ? context->GetRenderingAPI()
              : CanvasRenderingContext::CanvasRenderingAPI::kUnknown);
  const bool encountered_skipped_ops =
      context && context->IdentifiabilityEncounteredSkippedOps();
  const bool encountered_sensitive_ops =
      context && context->IdentifiabilityEncounteredSensitiveOps();
  const bool encountered_partially_digested_image =
      context && context->IdentifiabilityEncounteredPartiallyDigestedImage();
  // Bits [0-3] are the context type, bits [4-6] are skipped ops, sensitive
  // ops, and partial image ops bits, respectively. The remaining bits are
  // for the canvas digest.
  uint64_t final_digest = (context_digest << 7) | context_type;
  if (encountered_skipped_ops)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kSkipped;
  if (encountered_sensitive_ops)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kSensitive;
  if (encountered_partially_digested_image)
    final_digest |= IdentifiableSurface::CanvasTaintBit::kPartiallyDigested;
  return final_digest;
}

}  // namespace blink
