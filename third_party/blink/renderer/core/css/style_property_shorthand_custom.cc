/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "third_party/blink/renderer/core/style_property_shorthand.h"

#include "base/stl_util.h"

namespace blink {

const StylePropertyShorthand& animationShorthandForParsing() {
  // When we parse the animation shorthand we need to look for animation-name
  // last because otherwise it might match against the keywords for fill mode,
  // timing functions and infinite iteration. This means that animation names
  // that are the same as keywords (e.g. 'forwards') won't always match in the
  // shorthand. In that case the authors should be using longhands (or
  // reconsidering their approach). This is covered by the animations spec
  // bug: https://www.w3.org/Bugs/Public/show_bug.cgi?id=14790
  // And in the spec (editor's draft) at:
  // https://drafts.csswg.org/css-animations/#animation-shorthand-property
  static const CSSProperty* kAnimationPropertiesForParsing[] = {
      &GetCSSPropertyAnimationDuration(),
      &GetCSSPropertyAnimationTimingFunction(),
      &GetCSSPropertyAnimationDelay(),
      &GetCSSPropertyAnimationIterationCount(),
      &GetCSSPropertyAnimationDirection(),
      &GetCSSPropertyAnimationFillMode(),
      &GetCSSPropertyAnimationPlayState(),
      &GetCSSPropertyAnimationName()};
  static constexpr StylePropertyShorthand
      webkit_animation_longhands_for_parsing(
          CSSPropertyID::kAnimation, kAnimationPropertiesForParsing,
          base::size(kAnimationPropertiesForParsing));
  return webkit_animation_longhands_for_parsing;
}

// Similar to animations, we have property after timing-function and delay after
// duration
const StylePropertyShorthand& transitionShorthandForParsing() {
  static const CSSProperty* kTransitionProperties[] = {
      &GetCSSPropertyTransitionDuration(),
      &GetCSSPropertyTransitionTimingFunction(),
      &GetCSSPropertyTransitionDelay(), &GetCSSPropertyTransitionProperty()};
  static StylePropertyShorthand transition_longhands(
      CSSPropertyID::kTransition, kTransitionProperties,
      base::size(kTransitionProperties));
  return transition_longhands;
}

unsigned indexOfShorthandForLonghand(
    CSSPropertyID shorthand_id,
    const Vector<StylePropertyShorthand, 4>& shorthands) {
  for (unsigned i = 0; i < shorthands.size(); ++i) {
    if (shorthands.at(i).id() == shorthand_id)
      return i;
  }
  NOTREACHED();
  return 0;
}

}  // namespace blink
