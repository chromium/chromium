/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_TO_STYLE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_TO_STYLE_MAP_H_

#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FillLayer;
class CSSValue;
class StyleResolverState;
class NinePieceImage;
class BorderImageLengthBox;

class CSSToStyleMap {
  STATIC_ONLY(CSSToStyleMap);

 public:
  static void MapFillAttachment(StyleResolverState&,
                                FillLayer*,
                                const CSSValue&);
  static void MapFillClip(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillComposite(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillBlendMode(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillOrigin(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillImage(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillRepeatX(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillRepeatY(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillSize(StyleResolverState&, FillLayer*, const CSSValue&);
  static void MapFillPositionX(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillPositionY(StyleResolverState&,
                               FillLayer*,
                               const CSSValue&);
  static void MapFillMaskSourceType(StyleResolverState&,
                                    FillLayer*,
                                    const CSSValue&);

  static double MapAnimationDelay(const CSSValue&);
  static Timing::PlaybackDirection MapAnimationDirection(const CSSValue&);
  static double MapAnimationDuration(const CSSValue&);
  static Timing::FillMode MapAnimationFillMode(const CSSValue&);
  static double MapAnimationIterationCount(const CSSValue&);
  static AtomicString MapAnimationName(const CSSValue&);
  static EAnimPlayState MapAnimationPlayState(const CSSValue&);
  static CSSTransitionData::TransitionProperty MapAnimationProperty(
      const CSSValue&);

  static scoped_refptr<TimingFunction> MapAnimationTimingFunction(
      const CSSValue&);

  static void MapNinePieceImage(StyleResolverState&,
                                CSSPropertyID,
                                const CSSValue&,
                                NinePieceImage&);
  static void MapNinePieceImageSlice(StyleResolverState&,
                                     const CSSValue&,
                                     NinePieceImage&);
  static BorderImageLengthBox MapNinePieceImageQuad(StyleResolverState&,
                                                    const CSSValue&);
  static void MapNinePieceImageRepeat(StyleResolverState&,
                                      const CSSValue&,
                                      NinePieceImage&);
};

}  // namespace blink

#endif
