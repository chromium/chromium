/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

DrawLooperBuilder::DrawLooperBuilder() = default;

DrawLooperBuilder::~DrawLooperBuilder() = default;

sk_sp<SkDrawLooper> DrawLooperBuilder::DetachDrawLooper() {
  return sk_draw_looper_builder_.detach();
}

void DrawLooperBuilder::AddUnmodifiedContent() {
  SkLayerDrawLooper::LayerInfo info;
  sk_draw_looper_builder_.addLayerOnTop(info);
}

void DrawLooperBuilder::AddShadow(const FloatSize& offset,
                                  float blur,
                                  const Color& color,
                                  ShadowTransformMode shadow_transform_mode,
                                  ShadowAlphaMode shadow_alpha_mode) {
  DCHECK_GE(blur, 0);

  // Detect when there's no effective shadow.
  if (!color.Alpha())
    return;

  SkColor sk_color = color.Rgb();

  SkLayerDrawLooper::LayerInfo info;

  switch (shadow_alpha_mode) {
    case kShadowRespectsAlpha:
      info.fColorMode = SkBlendMode::kDst;
      break;
    case kShadowIgnoresAlpha:
      info.fColorMode = SkBlendMode::kSrc;
      break;
    default:
      NOTREACHED();
  }

  if (blur)
    info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;  // our blur
  info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  info.fOffset.set(offset.Width(), offset.Height());
  info.fPostTranslate = (shadow_transform_mode == kShadowIgnoresTransforms);

  SkPaint* paint = sk_draw_looper_builder_.addLayerOnTop(info);

  if (blur) {
    const auto sigma = BlurRadiusToStdDev(blur);
    const bool respectCTM = shadow_transform_mode != kShadowIgnoresTransforms;
    paint->setMaskFilter(
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma, respectCTM));
  }

  paint->setColorFilter(SkColorFilters::Blend(sk_color, SkBlendMode::kSrcIn));
}

}  // namespace blink
