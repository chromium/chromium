// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FONT_SIZE_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FONT_SIZE_STYLE_H_

namespace blink {

// FontSizeStyle contains the subset of ComputedStyle/ComputedStyleBuilder
// which is needed to resolve units within CSSToLengthConversionData and
// friends.
class CORE_EXPORT FontSizeStyle {
  STACK_ALLOCATED();

 public:
  FontSizeStyle(const Font& font,
                const Length& specified_line_height,
                float effective_zoom)
      : font_(font),
        specified_line_height_(specified_line_height),
        effective_zoom_(effective_zoom) {}

  const Font& GetFont() const { return font_; }
  const Length& SpecifiedLineHeight() const { return specified_line_height_; }
  float SpecifiedFontSize() const {
    return font_.GetFontDescription().SpecifiedSize();
  }
  float EffectiveZoom() const { return effective_zoom_; }

 private:
  const Font& font_;
  const Length& specified_line_height_;
  const float effective_zoom_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FONT_SIZE_STYLE_H_
