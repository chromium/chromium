// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Represents scaling information of each item.
struct FitTextScale : public GarbageCollected<FitTextScale> {
  float scale = 1.0f;
  // `font` is non-null only if the method is `font-size`.
  Member<Font> font;
  bool is_scaled_inline_only = false;

  void Trace(Visitor* visitor) const { visitor->Trace(font); }
};

// Information to help TextMetrics computation.
// * If the method is `scale`, paint_scale == total_scale.
//   scaled_font is nullptr.
// * If the method is `font-size`, <scale factor of scaled_font> *
//   paint_scale == total_scale.
struct FitTextBlockScale {
  float paint_scale = 1.0f;
  float total_scale = 1.0f;
  const Font* scaled_font = nullptr;

  constexpr static FitTextBlockScale* kFixed = nullptr;

  STACK_ALLOCATED();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_
