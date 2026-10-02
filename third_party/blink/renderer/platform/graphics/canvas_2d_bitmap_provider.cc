// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_bitmap_provider.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/skia_paint_canvas.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

Canvas2DBitmapProvider::Canvas2DBitmapProvider(
    sk_sp<SkSurface> surface,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space,
    CanvasResourceProviderDelegate* delegate)
    : format_(format),
      color_space_(color_space),
      delegate_(delegate),
      surface_(std::move(surface)) {
  CHECK(surface_);
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

Canvas2DBitmapProvider::~Canvas2DBitmapProvider() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
  if (context_provider_wrapper_) {
    context_provider_wrapper_->RemoveObserver(this);
  }
}

void Canvas2DBitmapProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string dump_name =
      base::StringPrintf("canvas/ResourceProvider/SkSurface/0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(surface_.get()));
  auto* dump = pmd->CreateAllocatorDump(dump_name);

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  GetSize());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects, 1);

  if (const char* system_allocator_name =
          base::trace_event::MemoryDumpManager::GetInstance()
              ->system_allocator_pool_name()) {
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  }
}

size_t Canvas2DBitmapProvider::GetSize() const {
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

void Canvas2DBitmapProvider::OnContextDestroyed() {
  skia_canvas_.reset();
  canvas_image_provider_ = nullptr;
}

CanvasImageProvider*
Canvas2DBitmapProvider::GetOrCreateSWCanvasImageProvider() {
  if (canvas_image_provider_) {
    return canvas_image_provider_.get();
  }

  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (format_ == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
  }

  cc::ImageDecodeCache* cache_rgba8 =
      &Image::SharedCCDecodeCache(kN32_SkColorType);

  if (!context_provider_wrapper_) {
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
    if (context_provider_wrapper_) {
      context_provider_wrapper_->AddObserver(this);
    }
  }
  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, color_space_, format_,
      cc::PlaybackImageProvider::RasterMode::kSoftware,
      context_provider_wrapper_);

  return canvas_image_provider_.get();
}

void Canvas2DBitmapProvider::ApplyAnimatedImageFrameIndexesForId(
    SkCanvas* canvas,
    uint32_t id) {
  CHECK(delegate_);
  CHECK(canvas_image_provider_);
  canvas_image_provider_->SetAnimatedImageFrameIndexes(
      delegate_->GetAnimatedImageFrameIndexes(id));
}

void Canvas2DBitmapProvider::RasterRecord(cc::PaintRecord last_recording) {
  if (!skia_canvas_) {
    skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
        surface_->getCanvas(), GetOrCreateSWCanvasImageProvider());
  }
  cc::PlaybackCallbacks::CustomDataRasterCallback custom_callback;
  if (delegate_) {
    // base::Unretained(this) is safe here because the callback will only be
    // invoked during the scope of skia_canvas_->drawPicture().
    custom_callback = base::BindRepeating(
        &Canvas2DBitmapProvider::ApplyAnimatedImageFrameIndexesForId,
        base::Unretained(this));
  }
  skia_canvas_->drawPicture(std::move(last_recording), custom_callback);
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
    canvas_image_provider_->UnbindTextureBackedImages();
  }
}

std::unique_ptr<Canvas2DBitmapProvider> Canvas2DBitmapProvider::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    CanvasResourceProviderDelegate* delegate) {
  const auto info = SkImageInfo::Make(
      size.width(), size.height(), viz::ToClosestSkColorType(format),
      kPremul_SkAlphaType, color_space.ToSkColorSpace());
  const bool can_use_lcd_text = alpha_type == kOpaque_SkAlphaType;
  const auto props =
      skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &props);
  if (!surface) {
    return nullptr;
  }
  surface->getCanvas()->clear(
      alpha_type == kOpaque_SkAlphaType ? SkColors::kBlack
                                        : SkColors::kTransparent);
  return base::WrapUnique<Canvas2DBitmapProvider>(new Canvas2DBitmapProvider(
      std::move(surface), format, color_space, delegate));
}

}  // namespace blink
