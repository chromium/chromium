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
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

Canvas2DBitmapProvider::Canvas2DBitmapProvider(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    CanvasResourceProviderDelegate* delegate)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      hdr_metadata_(hdr_metadata),
      delegate_(delegate),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()) {
  max_recorded_op_bytes_ = static_cast<size_t>(kMaxRecordedOpKB.Get()) * 1024;
  max_pinned_image_bytes_ = static_cast<size_t>(kMaxPinnedImageKB.Get()) * 1024;
  recorder_ = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  CanvasMemoryDumpProvider::Instance()->RegisterClient(this);
}

Canvas2DBitmapProvider::~Canvas2DBitmapProvider() {
  CanvasMemoryDumpProvider::Instance()->UnregisterClient(this);
}

bool Canvas2DBitmapProvider::IsPrinting() const {
  return delegate_ && delegate_->IsPrinting();
}

SkSurface* Canvas2DBitmapProvider::GetSkSurface() const {
  if (!surface_) {
    surface_ = CreateSkSurface();
  }
  return surface_.get();
}

void Canvas2DBitmapProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!surface_) {
    return;
  }

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
  if (!surface_) {
    return 0;
  }
  SkImageInfo info = surface_->imageInfo();
  return info.computeByteSize(info.minRowBytes());
}

void Canvas2DBitmapProvider::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  if (delegate_) {
    delegate_->InitializeForRecording(canvas);
  }
}

void Canvas2DBitmapProvider::RecordingCleared() {
  clear_frame_ = true;
}

CanvasImageProvider*
Canvas2DBitmapProvider::GetOrCreateSWCanvasImageProvider() {
  if (canvas_image_provider_) {
    return canvas_image_provider_.get();
  }

  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
    cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
  }

  cc::ImageDecodeCache* cache_rgba8 =
      &Image::SharedCCDecodeCache(kN32_SkColorType);

  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      cache_rgba8, cache_f16, GetColorSpace(), GetSharedImageFormat(),
      cc::PlaybackImageProvider::RasterMode::kSoftware);

  return canvas_image_provider_.get();
}

void Canvas2DBitmapProvider::SetAnimatedImageFrameIndexes(
    scoped_refptr<const cc::AnimatedImageFrameIndexMap> map) {
  CHECK(canvas_image_provider_);
  canvas_image_provider_->SetAnimatedImageFrameIndexes(map);
}

std::unique_ptr<MemoryManagedPaintRecorder>
Canvas2DBitmapProvider::ReleaseRecorder() {
  auto recorder = std::make_unique<MemoryManagedPaintRecorder>(Size(), this);
  recorder_->SetClient(nullptr);
  recorder_.swap(recorder);
  return recorder;
}

void Canvas2DBitmapProvider::SetRecorder(
    std::unique_ptr<MemoryManagedPaintRecorder> recorder) {
  recorder->SetClient(this);
  recorder_ = std::move(recorder);
}

scoped_refptr<StaticBitmapImage> Canvas2DBitmapProvider::Snapshot(
    ImageOrientation orientation) {
  TRACE_EVENT0("blink", "Canvas2DBitmapProvider::Snapshot");
  if (!IsValid()) {
    return nullptr;
  }

  cc::PaintImage paint_image;

  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (sk_image) {
    auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
    snapshot_sk_image_id_ = sk_image->uniqueID();

    // Ensure that a new PaintImage::ContentId is used only when the underlying
    // SkImage changes. This is necessary to ensure that the same image results
    // in a cache hit in cc's ImageDecodeCache.
    if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
        last_snapshot_sk_image_id != snapshot_sk_image_id_) {
      snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
    }

    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(snapshot_paint_image_id_)
            .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
            .set_hdr_metadata(GetHdrMetadata())
            .TakePaintImage();
  }

  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

std::optional<cc::PaintRecord> Canvas2DBitmapProvider::Flush(
    FlushReason reason) {
  if (!Recorder().HasReleasableDrawOps()) {
    return std::nullopt;
  }
  ScopedRasterTimer timer(nullptr, *this, false);
  bool want_to_print = IsPrinting() || reason == FlushReason::kPrinting ||
                       reason == FlushReason::kCanvasPushFrameWhilePrinting;
  bool preserve_recording = want_to_print && clear_frame_;

  clear_frame_ = false;
  cc::PaintRecord recording;
  recording = Recorder().ReleaseMainRecording();
  RasterRecord(recording);
  if (canvas_image_provider_) {
    canvas_image_provider_->ReleaseLockedImages();
    canvas_image_provider_->UnbindTextureBackedImages();
  }

  last_recording_ =
      preserve_recording ? std::optional(recording) : std::nullopt;

  if (delegate_) {
    delegate_->DidFlush();
  }

  return recording;
}

const std::optional<cc::PaintRecord>& Canvas2DBitmapProvider::LastRecording() {
  return last_recording_;
}

sk_sp<SkSurface> Canvas2DBitmapProvider::CreateSkSurface() const {
  TRACE_EVENT0("blink", "Canvas2DBitmapProvider::CreateSkSurface");

  const auto info = SkImageInfo::Make(
      size_.width(), size_.height(), viz::ToClosestSkColorType(format_),
      kPremul_SkAlphaType, color_space_.ToSkColorSpace());
  const auto props = GetSkSurfaceProps();
  return SkSurfaces::Raster(info, &props);
}

SkSurfaceProps Canvas2DBitmapProvider::GetSkSurfaceProps() const {
  const bool can_use_lcd_text = GetAlphaType() == kOpaque_SkAlphaType;
  return skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
}

void Canvas2DBitmapProvider::RestoreBackBuffer(const cc::PaintImage& image) {
  DCHECK_EQ(image.height(), Size().height());
  DCHECK_EQ(image.width(), Size().width());

  auto sk_image = image.GetSwSkImage();
  DCHECK(sk_image);
  SkPixmap map;
  sk_image->peekPixels(&map);
  WritePixels(map.info(), map.addr(), map.rowBytes(), /*x=*/0, /*y=*/0);
}

void Canvas2DBitmapProvider::ApplyAnimatedImageFrameIndexesForId(
    SkCanvas* canvas,
    uint32_t id) {
  CHECK(delegate_);
  SetAnimatedImageFrameIndexes(delegate_->GetAnimatedImageFrameIndexes(id));
}

void Canvas2DBitmapProvider::ClearAtCreation() {
  DCHECK(IsValid());
  MemoryManagedPaintRecorder recorder(Size(), this);
  if (GetAlphaType() == kOpaque_SkAlphaType) {
    recorder.getRecordingCanvas().clear(SkColors::kBlack);
  } else {
    recorder.getRecordingCanvas().clear(SkColors::kTransparent);
  }

  RasterRecord(recorder.ReleaseMainRecording());
}

void Canvas2DBitmapProvider::RasterRecord(cc::PaintRecord last_recording) {
  RasterRecord([this, &last_recording](cc::PaintCanvas& canvas) {
    cc::PlaybackCallbacks::CustomDataRasterCallback custom_callback;
    if (this->delegate_) {
      // base::Unretained(this) is safe here because the callback will only be
      // invoked during the scope of skia_canvas_->drawPicture().
      custom_callback = base::BindRepeating(
          &Canvas2DBitmapProvider::ApplyAnimatedImageFrameIndexesForId,
          base::Unretained(this));
    }
    skia_canvas_->drawPicture(std::move(last_recording), custom_callback);
  });
}

void Canvas2DBitmapProvider::RasterRecord(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback) {
  if (!skia_canvas_) {
    skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
        GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
  }
  draw_callback(*skia_canvas_);
}

bool Canvas2DBitmapProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "Canvas2DBitmapProvider::WritePixels");
  DCHECK(IsValid());
  DCHECK(!Recorder().HasRecordedDrawOps());

  if (!skia_canvas_) {
    skia_canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
        GetSkSurface()->getCanvas(), GetOrCreateSWCanvasImageProvider());
  }

  bool wrote_pixels = GetSkSurface()->getCanvas()->writePixels(
      orig_info, pixels, row_bytes, x, y);

  if (wrote_pixels) {
    // WritePixels content is not saved in recording. Calling WritePixels
    // therefore invalidates `last_recording_` because it's now
    // missing that information.
    last_recording_ = std::nullopt;
  }
  return wrote_pixels;
}

std::unique_ptr<Canvas2DBitmapProvider> Canvas2DBitmapProvider::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    CanvasResourceProviderDelegate* delegate) {
  auto provider =
      base::WrapUnique<Canvas2DBitmapProvider>(new Canvas2DBitmapProvider(
          size, format, alpha_type, color_space, hdr_metadata, delegate));
  if (provider->IsValid()) {
    provider->ClearAtCreation();
    // The ClearAtCreation() call cannot turn a Canvas2DBitmapProvider invalid.
    CHECK(provider->IsValid());
    return provider;
  }
  return nullptr;
}

std::unique_ptr<Canvas2DBitmapProvider>
Canvas2DBitmapProvider::CreateForTesting(
    gfx::Size size,
    const Canvas2DColorParams& color_params) {
  return Canvas2DBitmapProvider::CreateWithClear(
      size, color_params.GetSharedImageFormat(), color_params.GetAlphaType(),
      color_params.GetGfxColorSpace(), color_params.GetGfxHdrMetadata());
}

}  // namespace blink
