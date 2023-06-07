/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/text_link_colors.h"

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

constexpr Color kDefaultLinkColorLight = Color::FromRGB(0, 0, 238);
constexpr Color kDefaultLinkColorDark = Color::FromRGB(158, 158, 255);
constexpr Color kDefaultVisitedLinkColorLight = Color::FromRGB(85, 26, 139);
constexpr Color kDefaultVisitedLinkColorDark = Color::FromRGB(208, 173, 240);
constexpr Color kDefaultActiveLinkColorLight = Color::FromRGB(255, 0, 0);
constexpr Color kDefaultActiveLinkColorDark = Color::FromRGB(255, 158, 158);

TextLinkColors::TextLinkColors() : text_color_(Color::kBlack) {
  ResetLinkColor();
  ResetVisitedLinkColor();
  ResetActiveLinkColor();
}

void TextLinkColors::SetTextColor(const Color& color) {
  text_color_ = color;
  has_custom_text_color_ = true;
}

Color TextLinkColors::TextColor() const {
  return TextColor(mojom::blink::ColorScheme::kLight);
}

Color TextLinkColors::TextColor(mojom::blink::ColorScheme color_scheme) const {
  return has_custom_text_color_
             ? text_color_
             : color_scheme == mojom::blink::ColorScheme::kLight
                   ? Color::kBlack
                   : Color::kWhite;
}

void TextLinkColors::SetLinkColor(const Color& color) {
  link_color_ = color;
  has_custom_link_color_ = true;
}

const Color& TextLinkColors::LinkColor() const {
  return LinkColor(mojom::blink::ColorScheme::kLight);
}

const Color& TextLinkColors::LinkColor(
    mojom::blink::ColorScheme color_scheme) const {
  return has_custom_link_color_
             ? link_color_
             : color_scheme == mojom::blink::ColorScheme::kLight
                   ? kDefaultLinkColorLight
                   : kDefaultLinkColorDark;
}

void TextLinkColors::SetVisitedLinkColor(const Color& color) {
  visited_link_color_ = color;
  has_custom_visited_link_color_ = true;
}

const Color& TextLinkColors::VisitedLinkColor() const {
  return VisitedLinkColor(mojom::blink::ColorScheme::kLight);
}

const Color& TextLinkColors::VisitedLinkColor(
    mojom::blink::ColorScheme color_scheme) const {
  return has_custom_visited_link_color_
             ? visited_link_color_
             : color_scheme == mojom::blink::ColorScheme::kLight
                   ? kDefaultVisitedLinkColorLight
                   : kDefaultVisitedLinkColorDark;
}

void TextLinkColors::SetActiveLinkColor(const Color& color) {
  active_link_color_ = color;
  has_custom_active_link_color_ = true;
}

const Color& TextLinkColors::ActiveLinkColor() const {
  return ActiveLinkColor(mojom::blink::ColorScheme::kLight);
}

const Color& TextLinkColors::ActiveLinkColor(
    mojom::blink::ColorScheme color_scheme) const {
  return has_custom_active_link_color_
             ? active_link_color_
             : color_scheme == mojom::blink::ColorScheme::kLight
                   ? kDefaultActiveLinkColorLight
                   : kDefaultActiveLinkColorDark;
}

Color TextLinkColors::ColorFromCSSValue(const CSSValue& value,
                                        Color current_color,
                                        mojom::blink::ColorScheme color_scheme,
                                        bool for_visited_link) const {
  if (auto* color_value = DynamicTo<cssvalue::CSSColor>(value)) {
    Color result_color = color_value->Value();
    result_color.ResolveNonFiniteValues();
    return result_color;
  }

  if (auto* color_mix_value = DynamicTo<cssvalue::CSSColorMixValue>(value)) {
    Color c1 = ColorFromCSSValue(color_mix_value->Color1(), current_color,
                                 color_scheme, for_visited_link);
    Color c2 = ColorFromCSSValue(color_mix_value->Color2(), current_color,
                                 color_scheme, for_visited_link);

    double alpha_multiplier;
    double mix_amount;
    if (cssvalue::CSSColorMixValue::NormalizePercentages(
            color_mix_value->Percentage1(), color_mix_value->Percentage2(),
            mix_amount, alpha_multiplier)) {
      return Color::FromColorMix(color_mix_value->ColorInterpolationSpace(),
                                 color_mix_value->HueInterpolationMethod(), c1,
                                 c2, mix_amount, alpha_multiplier);
    } else {
      // TODO(crbug.com/1362022): Not sure what is appropriate to return when
      // both mix amounts are zero.
      return Color();
    }
  }

  if (auto* pair = DynamicTo<CSSLightDarkValuePair>(value)) {
    const CSSValue& color_value =
        color_scheme == mojom::blink::ColorScheme::kLight ? pair->First()
                                                          : pair->Second();
    return ColorFromCSSValue(color_value, current_color, color_scheme,
                             for_visited_link);
  }

  CSSValueID value_id = To<CSSIdentifierValue>(value).GetValueID();
  switch (value_id) {
    case CSSValueID::kInvalid:
      NOTREACHED();
      return Color();
    case CSSValueID::kInternalQuirkInherit:
      return TextColor(color_scheme);
    case CSSValueID::kWebkitLink:
      return for_visited_link ? VisitedLinkColor(color_scheme)
                              : LinkColor(color_scheme);
    case CSSValueID::kWebkitActivelink:
      return ActiveLinkColor(color_scheme);
    case CSSValueID::kWebkitFocusRingColor:
      return LayoutTheme::GetTheme().FocusRingColor(color_scheme);
    case CSSValueID::kCurrentcolor:
      return current_color;
    default:
      return StyleColor::ColorFromKeyword(value_id, color_scheme);
  }
}

}  // namespace blink
