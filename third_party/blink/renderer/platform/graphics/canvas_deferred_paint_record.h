// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_DEFERRED_PAINT_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_DEFERRED_PAINT_RECORD_H_

#include "cc/paint/deferred_paint_record.h"
#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

class PLATFORM_EXPORT CanvasDeferredPaintRecord
    : public cc::DeferredPaintRecord {
 public:
  CanvasDeferredPaintRecord();
  void SetPaintRecord(cc::PaintRecord, gfx::SizeF);
  void Clear();
  cc::PaintRecord GetPaintRecord() { return paint_record_; }
  gfx::Transform GetTransform() { return transform_; }

  gfx::SizeF GetSize() const override;

 protected:
  ~CanvasDeferredPaintRecord() override;

 private:
  gfx::Transform transform_;
  gfx::SizeF size_{0, 0};
  cc::PaintRecord paint_record_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_DEFERRED_PAINT_RECORD_H_
