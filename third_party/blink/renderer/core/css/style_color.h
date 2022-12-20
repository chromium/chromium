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

#include <memory>
#include "base/check.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT StyleColor {
  DISALLOW_NEW();

 public:
  // When color-mix functions contain colors that cannot be resolved until used
  // value time (such as "currentcolor"), we need to store them here and
  // resolve them to individual colors later.
  class UnresolvedColorMix;
  union ColorOrUnresolvedColorMix {
    ColorOrUnresolvedColorMix() : color(Color::kTransparent) {}
    explicit ColorOrUnresolvedColorMix(Color color) : color(color) {}
    explicit ColorOrUnresolvedColorMix(const StyleColor style_color);
    explicit ColorOrUnresolvedColorMix(UnresolvedColorMix color_mix);
    // Since an instance ColorOrUnresolvedColorMix does not know whether it
    // contains a color or an UnresolvedColorMix, release of
    // unresolved_color_mix is left to StyleColor::~StyleColor().
    ~ColorOrUnresolvedColorMix() {}

    Color color;
    std::unique_ptr<UnresolvedColorMix> unresolved_color_mix;
  };
  class UnresolvedColorMix {
   public:
    enum class UnderlyingColorType {
      kColor,
      kColorMix,
      kCurrentColor,
    };

    UnresolvedColorMix(const cssvalue::CSSColorMixValue* in,
                       const StyleColor& c1,
                       const StyleColor& c2);
    UnresolvedColorMix();
    UnresolvedColorMix(const UnresolvedColorMix& other);
    UnresolvedColorMix& operator=(const UnresolvedColorMix& other);
    Color Resolve(const Color& current_color) const;

   private:
    Color::ColorInterpolationSpace color_interpolation_space_ =
        Color::ColorInterpolationSpace::kNone;
    Color::HueInterpolationMethod hue_interpolation_method_ =
        Color::HueInterpolationMethod::kShorter;
    ColorOrUnresolvedColorMix color1_;
    ColorOrUnresolvedColorMix color2_;
    double percentage_ = 0.0;
    double alpha_multiplier_ = 1.0;
    UnderlyingColorType color1_type_ = UnderlyingColorType::kColor;
    UnderlyingColorType color2_type_ = UnderlyingColorType::kColor;
  };

  StyleColor() = default;
  explicit StyleColor(Color color)
      : color_keyword_(CSSValueID::kInvalid),
        color_or_unresolved_color_mix_(color) {}
  explicit StyleColor(CSSValueID keyword) : color_keyword_(keyword) {}
  explicit StyleColor(UnresolvedColorMix color_mix)
      : color_keyword_(CSSValueID::kColorMix),
        color_or_unresolved_color_mix_(color_mix) {}
  // We need to store the color and keyword for system colors to be able to
  // distinguish system colors from a normal color. System colors won't be
  // overridden by forced colors mode, even if forced-color-adjust is 'auto'.
  StyleColor(Color color, CSSValueID keyword)
      : color_keyword_(keyword), color_or_unresolved_color_mix_(color) {}

  // All copy/move/assignment operators are necessary to handle the potential
  // unique pointer in color_or_unresolved_color_mix_.
  StyleColor(const StyleColor& other);
  StyleColor& operator=(const StyleColor& other);
  StyleColor& operator=(StyleColor&& other);
  StyleColor(StyleColor&&);
  ~StyleColor();

  static StyleColor CurrentColor() { return StyleColor(); }

  bool IsCurrentColor() const {
    return color_keyword_ == CSSValueID::kCurrentcolor;
  }
  bool IsUnresolvedColorMixFunction() const {
    return color_keyword_ == CSSValueID::kColorMix;
  }
  bool IsSystemColorIncludingDeprecated() const {
    return IsSystemColorIncludingDeprecated(color_keyword_);
  }
  bool IsSystemColor() const { return IsSystemColor(color_keyword_); }
  UnresolvedColorMix GetUnresolvedColorMix() const {
    DCHECK(IsUnresolvedColorMixFunction());
    return *color_or_unresolved_color_mix_.unresolved_color_mix;
  }
  Color GetColor() const;

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
    if (color_keyword_ != other.color_keyword_) {
      return false;
    }

    if (IsCurrentColor() && other.IsCurrentColor()) {
      return true;
    }

    if (IsUnresolvedColorMixFunction()) {
      return color_or_unresolved_color_mix_.unresolved_color_mix ==
             other.color_or_unresolved_color_mix_.unresolved_color_mix;
    }

    return color_or_unresolved_color_mix_.color ==
           other.color_or_unresolved_color_mix_.color;
  }

  inline bool operator!=(const StyleColor& other) const {
    return !(*this == other);
  }

 protected:
  CSSValueID color_keyword_ = CSSValueID::kCurrentcolor;
  ColorOrUnresolvedColorMix color_or_unresolved_color_mix_;

 private:
  CSSValueID EffectiveColorKeyword() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_COLOR_H_
