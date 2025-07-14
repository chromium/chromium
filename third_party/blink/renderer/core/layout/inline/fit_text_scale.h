// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_FIT_TEXT_SCALE_H_
