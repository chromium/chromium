// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_PAINT_FILTER_EFFECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_PAINT_FILTER_EFFECT_H_

#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"

namespace blink {

class PLATFORM_EXPORT PaintFilterEffect : public FilterEffect {
 public:
  PaintFilterEffect(Filter*, const PaintFlags&);
  ~PaintFilterEffect() override;

  FilterEffectType GetFilterEffectType() const override {
    return kFilterEffectTypeSourceInput;
  }

  WTF::TextStream& ExternalRepresentation(WTF::TextStream&,
                                          int indention) const override;
  sk_sp<PaintFilter> CreateImageFilter() override;

 private:
  PaintFlags flags_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_PAINT_FILTER_EFFECT_H_
