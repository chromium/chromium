// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RECORD_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RECORD_PATTERN_H_

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/pattern.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// TODO(enne): rename this
class PLATFORM_EXPORT PaintRecordPattern final : public Pattern {
 public:
  static scoped_refptr<PaintRecordPattern>
  Create(sk_sp<PaintRecord>, const FloatRect& record_bounds, RepeatMode);

  ~PaintRecordPattern() override;

 protected:
  sk_sp<PaintShader> CreateShader(const SkMatrix&) const override;

 private:
  PaintRecordPattern(sk_sp<PaintRecord>,
                     const FloatRect& record_bounds,
                     RepeatMode);

  sk_sp<PaintRecord> tile_record_;
  FloatRect tile_record_bounds_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_RECORD_PATTERN_H_
