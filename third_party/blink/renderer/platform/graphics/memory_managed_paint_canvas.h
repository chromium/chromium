// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_

#include "cc/paint/paint_canvas.h"
#include "cc/paint/record_paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// MemoryManagedPaintCanvas overrides the potentially memory intensive image
// drawing methods of PaintCanvas and keeps track of how much memory is
// being pinned between flushes. This allows the rendering context to flush if
// too much memory is used.

class PLATFORM_EXPORT MemoryManagedPaintCanvas final
    : public cc::InspectableRecordPaintCanvas {
 public:
  // Base class for clients that receive callbacks from
  // MemoryManagedPaintCanvas.
  class Client {
   public:
    virtual void DidPinImage(size_t bytes) = 0;
  };

  MemoryManagedPaintCanvas(const gfx::Size& size, Client* client);
  explicit MemoryManagedPaintCanvas(const cc::RecordPaintCanvas&) = delete;
  ~MemoryManagedPaintCanvas() override;

  cc::PaintRecord ReleaseAsRecord() override;

  void drawImage(const cc::PaintImage& image,
                 SkScalar left,
                 SkScalar top,
                 const SkSamplingOptions&,
                 const cc::PaintFlags* flags) override;
  void drawImageRect(const cc::PaintImage& image,
                     const SkRect& src,
                     const SkRect& dst,
                     const SkSamplingOptions&,
                     const cc::PaintFlags* flags,
                     SkCanvas::SrcRectConstraint constraint) override;

  bool IsCachingImage(const cc::PaintImage::ContentId content_id) const;

 private:
  void UpdateMemoryUsage(const cc::PaintImage& image);

  HashSet<cc::PaintImage::ContentId,
          IntWithZeroKeyHashTraits<cc::PaintImage::ContentId>>
      cached_image_ids_;

  Client* client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_
