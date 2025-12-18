// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_encode_options.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

bool CanUseGPU() {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         !context_provider_wrapper->ContextProvider().IsContextLost();
}

}  // namespace

CanvasRenderingContextHost::CanvasRenderingContextHost(HostType host_type,
                                                       const gfx::Size& size)
    : size_(size), host_type_(host_type) {}

CanvasRenderingContextHost::~CanvasRenderingContextHost() {
  if (externally_allocated_memory_.is_positive()) {
    external_memory_accounter_.Decrease(v8::Isolate::GetCurrent(),
                                        externally_allocated_memory_.InBytes());
  }
}

void CanvasRenderingContextHost::Trace(Visitor* visitor) const {
  visitor->Trace(plain_text_painter_);
  visitor->Trace(unique_font_selector_);
}

void CanvasRenderingContextHost::RecordCanvasSizeToUMA() {
  if (did_record_canvas_size_to_uma_)
    return;
  did_record_canvas_size_to_uma_ = true;

  switch (host_type_) {
    case HostType::kNone:
      NOTREACHED();
    case HostType::kCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.Canvas.SqrtNumberOfPixels",
                                  std::sqrt(Size().Area64()), 1, 5000, 100);
      break;
    case HostType::kOffscreenCanvasHost:
      UMA_HISTOGRAM_CUSTOM_COUNTS("Blink.OffscreenCanvas.SqrtNumberOfPixels",
                                  std::sqrt(Size().Area64()), 1, 5000, 100);
      break;
  }
}

void CanvasRenderingContextHost::NotifyCachesOfSwitchingFrame() {
  if (plain_text_painter_) {
    plain_text_painter_->DidSwitchFrame();
  }
  if (unique_font_selector_) {
    unique_font_selector_->DidSwitchFrame();
  }
}

void CanvasRenderingContextHost::UpdateMemoryUsage() {
  base::ByteSize externally_allocated_memory =
      RenderingContext() ? RenderingContext()->AllocatedBufferSize()
                         : base::ByteSize(0);

  base::ByteSizeDelta delta_bytes =
      externally_allocated_memory - externally_allocated_memory_;

  // TODO(junov): We assume that it is impossible to be inside a FastAPICall
  // from a host interface other than the rendering context.  This assumption
  // may need to be revisited in the future depending on how the usage of
  // [NoAllocDirectCall] evolves.

  // ExternalMemoryAccounter::Update() with a positive delta can trigger a GC,
  // which is not allowed when `IsAllocationAllowed() == false`.
  CHECK(!delta_bytes.is_positive() ||
        ThreadState::Current()->IsAllocationAllowed());
  external_memory_accounter_.Update(v8::Isolate::GetCurrent(),
                                    delta_bytes.InBytes());
  externally_allocated_memory_ = externally_allocated_memory;
}

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage() const {
  if (!IsValidImageSize()) {
    return nullptr;
  }
  SkImageInfo info = SkImageInfo::Make(
      gfx::SizeToSkISize(Size()),
      viz::ToClosestSkColorType(GetRenderingContextFormat()),
      kPremul_SkAlphaType, GetRenderingContextColorSpace().ToSkColorSpace());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(info, info.minRowBytes(), nullptr);
  if (!surface) {
    return nullptr;
  }
  return UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
}

bool CanvasRenderingContextHost::IsValidImageSize() const {
  const gfx::Size size = Size();
  if (size.IsEmpty()) {
    return false;
  }
  base::CheckedNumeric<int> area = size.GetCheckedArea();
  // Firefox limits width/height to 32767 pixels, but slows down dramatically
  // before it reaches that limit. We limit by area instead, giving us larger
  // maximum dimensions, in exchange for a smaller maximum canvas size.
  static constexpr int kMaxCanvasArea =
      32768 * 8192;  // Maximum canvas area in CSS pixels
  if (!area.IsValid() || area.ValueOrDie() > kMaxCanvasArea) {
    return false;
  }
  // In Skia, we will also limit width/height to 65535.
  static constexpr int kMaxSkiaDim =
      65535;  // Maximum width/height in CSS pixels.
  if (size.width() > kMaxSkiaDim || size.height() > kMaxSkiaDim) {
    return false;
  }
  return true;
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize();
}

void CanvasRenderingContextHost::InitializeForRecording(
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

SkAlphaType CanvasRenderingContextHost::GetRenderingContextAlphaType() const {
  return RenderingContext() ? RenderingContext()->GetAlphaType()
                            : kPremul_SkAlphaType;
}

viz::SharedImageFormat CanvasRenderingContextHost::GetRenderingContextFormat()
    const {
  return RenderingContext() ? RenderingContext()->GetSharedImageFormat()
                            : GetN32FormatForCanvas();
}

gfx::ColorSpace CanvasRenderingContextHost::GetRenderingContextColorSpace()
    const {
  return RenderingContext() ? RenderingContext()->GetColorSpace()
                            : gfx::ColorSpace::CreateSRGB();
}

PlainTextPainter& CanvasRenderingContextHost::GetPlainTextPainter() {
  if (!plain_text_painter_) {
    plain_text_painter_ =
        MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas);
  }
  return *plain_text_painter_;
}

RasterMode CanvasRenderingContextHost::GetRasterModeForCanvas2D() const {
  CHECK(IsRenderingContext2D());
  return IsAccelerated() ? RasterMode::kGPU : RasterMode::kCPU;
}

bool CanvasRenderingContextHost::IsOffscreenCanvas() const {
  return host_type_ == HostType::kOffscreenCanvasHost;
}

bool CanvasRenderingContextHost::IsAccelerated() const {
  if (RenderingContext()) {
    // This method is supported only on 2D contexts.
    CHECK(IsRenderingContext2D());
    return RenderingContext()->Is2DCanvasAccelerated();
  }

  // Whether or not to accelerate is not yet resolved, the canvas cannot be
  // accelerated if the gpu context is lost.
  return ShouldTryToUseGpuRaster();
}

ImageBitmapSourceStatus CanvasRenderingContextHost::CheckUsability() const {
  const gfx::Size size = Size();
  if (size.IsEmpty()) {
    return base::unexpected(size.width() == 0
                                ? ImageBitmapSourceError::kZeroWidth
                                : ImageBitmapSourceError::kZeroHeight);
  }
  return base::ok();
}

void CanvasRenderingContextHost::PageVisibilityChanged() {
  bool page_visible = IsPageVisible();
  if (RenderingContext()) {
    RenderingContext()->PageVisibilityChanged();
    if (page_visible) {
      RenderingContext()->SendContextLostEventIfNeeded();
    }
  }
  if (!page_visible && (IsWebGL() || IsWebGPU())) {
    DiscardResources();
  }
}

bool CanvasRenderingContextHost::ContextHasOpenLayers(
    const CanvasRenderingContext* context) const {
  return context != nullptr && context->IsRenderingContext2D() &&
         context->LayerCount() != 0;
}

void CanvasRenderingContextHost::SetPreferred2DRasterMode(RasterModeHint hint) {
  // TODO(junov): move code that switches between CPU and GPU rasterization
  // to here.
  preferred_2d_raster_mode_ = hint;
}

bool CanvasRenderingContextHost::ShouldTryToUseGpuRaster() const {
  return preferred_2d_raster_mode_ == RasterModeHint::kPreferGPU && CanUseGPU();
}

}  // namespace blink
