// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_record_pattern.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

scoped_refptr<PaintRecordPattern> PaintRecordPattern::Create(
    PaintRecord record,
    const gfx::RectF& record_bounds,
    RepeatMode repeat_mode) {
  return base::AdoptRef(
      new PaintRecordPattern(std::move(record), record_bounds, repeat_mode));
}

PaintRecordPattern::PaintRecordPattern(PaintRecord record,
                                       const gfx::RectF& record_bounds,
                                       RepeatMode mode)
    : Pattern(mode),
      tile_record_(std::move(record)),
      tile_record_bounds_(record_bounds) {
  // All current clients use RepeatModeXY, so we only support this mode for now.
  DCHECK(IsRepeatXY());

  // FIXME: we don't have a good way to account for DL memory utilization.
}

PaintRecordPattern::~PaintRecordPattern() = default;

sk_sp<PaintShader> PaintRecordPattern::CreateShader(
    const SkMatrix& local_matrix) const {
  return PaintShader::MakePaintRecord(
      tile_record_, gfx::RectFToSkRect(tile_record_bounds_),
      SkTileMode::kRepeat, SkTileMode::kRepeat, &local_matrix);
}

}  // namespace blink
