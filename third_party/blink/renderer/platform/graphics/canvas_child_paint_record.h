// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_RECORD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_RECORD_H_

#include "cc/paint/paint_record.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

struct PLATFORM_EXPORT CanvasChildPaintRecord {
  float scale = 1.f;
  gfx::SizeF box_size;
  cc::PaintRecord record;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_CHILD_PAINT_RECORD_H_
