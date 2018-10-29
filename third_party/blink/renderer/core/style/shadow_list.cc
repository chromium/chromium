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

#include "third_party/blink/renderer/core/style/shadow_list.h"

#include <memory>
#include "third_party/blink/renderer/platform/geometry/float_rect.h"

namespace blink {

FloatRectOutsets ShadowList::RectOutsetsIncludingOriginal() const {
  FloatRectOutsets outsets;
  for (const ShadowData& shadow : Shadows()) {
    if (shadow.Style() == kInset)
      continue;
    outsets.Unite(shadow.RectOutsets());
  }
  return outsets;
}

void ShadowList::AdjustRectForShadow(FloatRect& rect) const {
  rect.Expand(RectOutsetsIncludingOriginal());
}

sk_sp<SkDrawLooper> ShadowList::CreateDrawLooper(
    DrawLooperBuilder::ShadowAlphaMode alpha_mode,
    const Color& current_color,
    bool is_horizontal) const {
  DrawLooperBuilder draw_looper_builder;
  for (wtf_size_t i = Shadows().size(); i--;) {
    const ShadowData& shadow = Shadows()[i];
    float shadow_x = is_horizontal ? shadow.X() : shadow.Y();
    float shadow_y = is_horizontal ? shadow.Y() : -shadow.X();
    draw_looper_builder.AddShadow(FloatSize(shadow_x, shadow_y), shadow.Blur(),
                                  shadow.GetColor().Resolve(current_color),
                                  DrawLooperBuilder::kShadowRespectsTransforms,
                                  alpha_mode);
  }
  draw_looper_builder.AddUnmodifiedContent();
  return draw_looper_builder.DetachDrawLooper();
}

}  // namespace blink
