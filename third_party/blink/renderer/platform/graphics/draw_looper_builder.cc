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
#include "cc/paint/draw_looper.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

DrawLooperBuilder::DrawLooperBuilder() = default;

DrawLooperBuilder::~DrawLooperBuilder() = default;

sk_sp<cc::DrawLooper> DrawLooperBuilder::DetachDrawLooper() {
  return draw_looper_builder_.Detach();
}

void DrawLooperBuilder::AddUnmodifiedContent() {
  draw_looper_builder_.AddUnmodifiedContent(/*add_on_top=*/true);
}

void DrawLooperBuilder::AddShadow(const gfx::Vector2dF& offset,
                                  float blur,
                                  const Color& color,
                                  ShadowTransformMode shadow_transform_mode,
                                  ShadowAlphaMode shadow_alpha_mode) {
  DCHECK_GE(blur, 0);

  // Detect when there's no effective shadow.
  if (color.IsFullyTransparent()) {
    return;
  }

  uint32_t flags = 0;
  if (shadow_alpha_mode == kShadowIgnoresAlpha) {
    flags |= cc::DrawLooper::kOverrideAlphaFlag;
  }
  if (shadow_transform_mode == kShadowIgnoresTransforms) {
    flags |= cc::DrawLooper::kPostTransformFlag;
  }

  draw_looper_builder_.AddShadow({offset.x(), offset.y()},
                                 BlurRadiusToStdDev(blur), color.toSkColor4f(),
                                 flags,
                                 /*add_on_top=*/true);
}

}  // namespace blink
