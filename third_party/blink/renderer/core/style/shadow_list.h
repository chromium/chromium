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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_LIST_H_

#include <memory>
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/geometry/float_rect_outsets.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FloatRect;

typedef Vector<ShadowData, 1> ShadowDataVector;

// These are used to store shadows in specified order, but we usually want to
// iterate over them backwards as the first-specified shadow is painted on top.
class ShadowList : public RefCounted<ShadowList> {
  USING_FAST_MALLOC(ShadowList);

 public:
  // This consumes passed in vector.
  static scoped_refptr<ShadowList> Adopt(ShadowDataVector& shadows) {
    return base::AdoptRef(new ShadowList(shadows));
  }
  const ShadowDataVector& Shadows() const { return shadows_; }
  bool operator==(const ShadowList& o) const { return shadows_ == o.shadows_; }
  bool operator!=(const ShadowList& o) const { return !(*this == o); }

  // Outsets needed to include all shadows in this list, as well as the
  // source (i.e. no outsets will be negative).
  FloatRectOutsets RectOutsetsIncludingOriginal() const;

  void AdjustRectForShadow(FloatRect&) const;

  sk_sp<SkDrawLooper> CreateDrawLooper(DrawLooperBuilder::ShadowAlphaMode,
                                       const Color& current_color,
                                       bool is_horizontal = true) const;

 private:
  ShadowList(ShadowDataVector& shadows) {
    // If we have no shadows, we use a null ShadowList
    DCHECK(!shadows.IsEmpty());
    shadows_.swap(shadows);
    shadows_.ShrinkToFit();
  }
  ShadowDataVector shadows_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SHADOW_LIST_H_
