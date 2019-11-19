// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FE_BOX_REFLECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FE_BOX_REFLECT_H_

#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// Used to implement the -webkit-box-reflect property as a filter.
class PLATFORM_EXPORT FEBoxReflect final : public FilterEffect {
 public:
  FEBoxReflect(Filter*, const BoxReflection&);

  // FilterEffect implementation
  WTF::TextStream& ExternalRepresentation(WTF::TextStream&,
                                          int indentation) const final;

 private:
  ~FEBoxReflect() final;

  FloatRect MapEffect(const FloatRect&) const final;

  sk_sp<PaintFilter> CreateImageFilter() final;

  BoxReflection reflection_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FE_BOX_REFLECT_H_
