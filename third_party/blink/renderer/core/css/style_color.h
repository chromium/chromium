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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_COLOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_COLOR_H_

#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT StyleColor {
  DISALLOW_NEW();

 public:
  StyleColor() = default;
  explicit StyleColor(Color color)
      : color_(color), color_keyword_(CSSValueID::kInvalid) {}
  explicit StyleColor(CSSValueID keyword) : color_keyword_(keyword) {}
  // We need to store the color and keyword for system colors to be able to
  // distinguish system colors from a normal color. System colors won't be
  // overridden by forced colors mode, even if forced-color-adjust is 'auto'.
  StyleColor(Color color, CSSValueID keyword)
      : color_(color), color_keyword_(keyword) {}
  static StyleColor CurrentColor() { return StyleColor(); }

  bool IsCurrentColor() const {
    return color_keyword_ == CSSValueID::kCurrentcolor;
  }
  bool IsSystemColorIncludingDeprecated() const {
    return IsSystemColorIncludingDeprecated(color_keyword_);
  }
  bool IsSystemColor() const { return IsSystemColor(color_keyword_); }
  Color GetColor() const {
    // System colors will fail the IsNumeric check, as they store a keyword, but
    // they also have a stored color that may need to be accessed directly. For
    // example in FilterEffectBuilder::BuildFilterEffect for shadow colors.
    DCHECK(IsNumeric() || IsSystemColorIncludingDeprecated());
    return color_;
  }
  CSSValueID GetColorKeyword() const {
    DCHECK(!IsNumeric());
    return color_keyword_;
  }
  bool HasColorKeyword() const {
    return color_keyword_ != CSSValueID::kInvalid;
  }

  Color Resolve(const Color& current_color,
                mojom::blink::ColorScheme color_scheme,
                bool* is_current_color = nullptr,
                bool is_forced_color = false) const;

  // Resolve and override the resolved color's alpha channel as specified by
  // |alpha|.
  Color ResolveWithAlpha(Color current_color,
                         mojom::blink::ColorScheme color_scheme,
                         int alpha,
                         bool* is_current_color = nullptr,
                         bool is_forced_color = false) const;

  bool IsNumeric() const {
    return EffectiveColorKeyword() == CSSValueID::kInvalid;
  }

  static Color ColorFromKeyword(CSSValueID,
                                mojom::blink::ColorScheme color_scheme);
  static bool IsColorKeyword(CSSValueID);
  static bool IsSystemColorIncludingDeprecated(CSSValueID);
  static bool IsSystemColor(CSSValueID);

  inline bool operator==(const StyleColor& other) const {
    DCHECK(IsValid());
    DCHECK(other.IsValid());
    return color_ == other.color_ && color_keyword_ == other.color_keyword_;
  }

  inline bool operator!=(const StyleColor& other) const {
    return !(*this == other);
  }

 protected:
  inline bool IsValid() const {
    // At least one of color_keyword_ and color_ should retain its default
    // value.
    return EffectiveColorKeyword() == CSSValueID::kInvalid ||
           color_ == Color() ||
           IsSystemColorIncludingDeprecated(EffectiveColorKeyword());
  }

  Color color_;
  CSSValueID color_keyword_ = CSSValueID::kCurrentcolor;

 private:
  CSSValueID EffectiveColorKeyword() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_COLOR_H_
