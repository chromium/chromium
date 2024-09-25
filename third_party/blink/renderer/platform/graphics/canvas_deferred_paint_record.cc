// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"

#include "ui/gfx/geometry/size_f.h"

namespace blink {

CanvasDeferredPaintRecord::~CanvasDeferredPaintRecord() = default;

CanvasDeferredPaintRecord::CanvasDeferredPaintRecord() = default;

void CanvasDeferredPaintRecord::SetPaintRecord(cc::PaintRecord record,
                                               gfx::SizeF size) {
  paint_record_ = record;
  size_ = size;
}

void CanvasDeferredPaintRecord::Clear() {
  paint_record_.empty();
  size_ = gfx::SizeF(0, 0);
}

gfx::SizeF CanvasDeferredPaintRecord::GetSize() const {
  return size_;
}

}  // namespace blink
