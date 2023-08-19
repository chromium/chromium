// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FONT_PALETTE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FONT_PALETTE_H_

#include <memory>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CORE_EXPORT InterpolableFontPalette final : public InterpolableValue {
 public:
  explicit InterpolableFontPalette(scoped_refptr<FontPalette> mix_value);

  static std::unique_ptr<InterpolableFontPalette> Create(
      scoped_refptr<FontPalette> font_palette);

  scoped_refptr<FontPalette> GetFontPalette() const;

  // InterpolableValue implementation:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;
  bool IsFontPalette() const final { return true; }
  bool Equals(const InterpolableValue& other) const final;
  // Scale() and Add() methods are only used for additive animations, but
  // font-palette is not additive, since the <color> type is not additive,
  // compare https://drafts.csswg.org/css-values-4/#combine-colors. Therefore
  // these methods should not do anything.
  void Scale(double scale) final {}
  void Add(const InterpolableValue& other) final {}
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  InterpolableFontPalette* RawClone() const final;
  InterpolableFontPalette* RawCloneAndZero() const final;

  scoped_refptr<FontPalette> font_palette_;
};

template <>
struct DowncastTraits<InterpolableFontPalette> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsFontPalette();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_FONT_PALETTE_H_
