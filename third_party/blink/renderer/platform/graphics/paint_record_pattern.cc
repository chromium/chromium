// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_record_pattern.h"

#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

scoped_refptr<PaintRecordPattern> PaintRecordPattern::Create(
    sk_sp<PaintRecord> record,
    const FloatRect& record_bounds,
    RepeatMode repeat_mode) {
  return base::AdoptRef(
      new PaintRecordPattern(std::move(record), record_bounds, repeat_mode));
}

PaintRecordPattern::PaintRecordPattern(sk_sp<PaintRecord> record,
                                       const FloatRect& record_bounds,
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
    const SkMatrix& local_matrix) {
  return PaintShader::MakePaintRecord(tile_record_, tile_record_bounds_,
                                      SkTileMode::kRepeat, SkTileMode::kRepeat,
                                      &local_matrix);
}

}  // namespace blink
