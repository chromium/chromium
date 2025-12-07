// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_canvas.h"

#include "base/memory/ptr_util.h"

namespace blink {

MemoryManagedPaintCanvas::MemoryManagedPaintCanvas(const gfx::Size& size)
    : cc::InspectableRecordPaintCanvas(size) {}

MemoryManagedPaintCanvas::MemoryManagedPaintCanvas(
    CreateChildCanvasTag,
    const MemoryManagedPaintCanvas& parent)
    : cc::InspectableRecordPaintCanvas(CreateChildCanvasTag(), parent) {}

MemoryManagedPaintCanvas::~MemoryManagedPaintCanvas() = default;

std::unique_ptr<MemoryManagedPaintCanvas>
MemoryManagedPaintCanvas::CreateChildCanvas() {
  // Using `new` to access a non-public constructor.
  auto canvas = base::WrapUnique(
      new MemoryManagedPaintCanvas(CreateChildCanvasTag(), *this));
  if (!IsDrawLinesAsPathsEnabled()) {
    canvas->DisableLineDrawingAsPaths();
  }
  return canvas;
}

cc::PaintRecord MemoryManagedPaintCanvas::ReleaseAsRecord() {
  cached_image_ids_.clear();
  image_bytes_used_ = 0;
  return cc::InspectableRecordPaintCanvas::ReleaseAsRecord();
}

void MemoryManagedPaintCanvas::drawImage(const cc::PaintImage& image,
                                         SkScalar left,
                                         SkScalar top,
                                         const SkSamplingOptions& sampling,
                                         const cc::PaintFlags* flags) {
  DCHECK(!image.IsPaintWorklet());
  cc::InspectableRecordPaintCanvas::drawImage(image, left, top, sampling,
                                              flags);
  UpdateMemoryUsage(image);
}

void MemoryManagedPaintCanvas::drawImageRect(
    const cc::PaintImage& image,
    const SkRect& src,
    const SkRect& dst,
    const SkSamplingOptions& sampling,
    const cc::PaintFlags* flags,
    SkCanvas::SrcRectConstraint constraint) {
  cc::InspectableRecordPaintCanvas::drawImageRect(image, src, dst, sampling,
                                                  flags, constraint);
  UpdateMemoryUsage(image);
}

void MemoryManagedPaintCanvas::UpdateMemoryUsage(const cc::PaintImage& image) {
  if (image.IsDeferredPaintRecord()) {
    return;
  }
  if (cached_image_ids_.Contains(image.GetContentIdForFrame(0u))) {
    return;
  }

  cached_image_ids_.insert(image.GetContentIdForFrame(0u));
  image_bytes_used_ += image.GetSkImageInfo().computeMinByteSize();
}

bool MemoryManagedPaintCanvas::IsCachingImage(
    const cc::PaintImage::ContentId content_id) const {
  return cached_image_ids_.Contains(content_id);
}

}  // namespace blink
