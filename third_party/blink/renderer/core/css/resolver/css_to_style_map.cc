/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"

#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_repeat_style_value.h"
#include "third_party/blink/renderer/core/css/css_scroll_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_view_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/border_image_length_box.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/fill_layer.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"

namespace blink {

void CSSToStyleMap::MapFillAttachment(StyleResolverState&,
                                      FillLayer* layer,
                                      const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetAttachment(FillLayer::InitialFillAttachment(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  switch (identifier_value->GetValueID()) {
    case CSSValueID::kFixed:
      layer->SetAttachment(EFillAttachment::kFixed);
      break;
    case CSSValueID::kScroll:
      layer->SetAttachment(EFillAttachment::kScroll);
      break;
    case CSSValueID::kLocal:
      layer->SetAttachment(EFillAttachment::kLocal);
      break;
    default:
      return;
  }
}

void CSSToStyleMap::MapFillClip(StyleResolverState&,
                                FillLayer* layer,
                                const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetClip(FillLayer::InitialFillClip(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  layer->SetClip(identifier_value->ConvertTo<EFillBox>());
}

void CSSToStyleMap::MapFillCompositingOperator(StyleResolverState&,
                                               FillLayer* layer,
                                               const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetCompositingOperator(
        FillLayer::InitialFillCompositingOperator(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  layer->SetCompositingOperator(
      identifier_value->ConvertTo<CompositingOperator>());
}

void CSSToStyleMap::MapFillBlendMode(StyleResolverState&,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetBlendMode(FillLayer::InitialFillBlendMode(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  layer->SetBlendMode(identifier_value->ConvertTo<BlendMode>());
}

void CSSToStyleMap::MapFillOrigin(StyleResolverState&,
                                  FillLayer* layer,
                                  const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetOrigin(FillLayer::InitialFillOrigin(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  layer->SetOrigin(identifier_value->ConvertTo<EFillBox>());
}

void CSSToStyleMap::MapFillImage(StyleResolverState& state,
                                 FillLayer* layer,
                                 const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetImage(FillLayer::InitialFillImage(layer->GetType()));
    return;
  }

  CSSPropertyID property = layer->GetType() == EFillLayerType::kBackground
                               ? CSSPropertyID::kBackgroundImage
                               : CSSPropertyID::kMaskImage;
  layer->SetImage(
      state.GetStyleImage(property, state.ResolveLightDarkPair(value)));
}

void CSSToStyleMap::MapFillRepeat(StyleResolverState&,
                                  FillLayer* layer,
                                  const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetRepeat(FillLayer::InitialFillRepeat(layer->GetType()));
    return;
  }

  if (const auto* repeat = DynamicTo<CSSRepeatStyleValue>(value)) {
    layer->SetRepeat({repeat->x()->ConvertTo<EFillRepeat>(),
                      repeat->y()->ConvertTo<EFillRepeat>()});
  }
}

void CSSToStyleMap::MapFillMaskMode(StyleResolverState&,
                                    FillLayer* layer,
                                    const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetMaskMode(FillLayer::InitialFillMaskMode(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value) {
    return;
  }

  layer->SetMaskMode(identifier_value->ConvertTo<EFillMaskMode>());
}

void CSSToStyleMap::MapFillSize(StyleResolverState& state,
                                FillLayer* layer,
                                const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetSizeType(FillLayer::InitialFillSizeType(layer->GetType()));
    layer->SetSizeLength(FillLayer::InitialFillSizeLength(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair()) {
    return;
  }

  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kContain) {
    layer->SetSizeType(EFillSizeType::kContain);
  } else if (identifier_value &&
             identifier_value->GetValueID() == CSSValueID::kCover) {
    layer->SetSizeType(EFillSizeType::kCover);
  } else {
    layer->SetSizeType(EFillSizeType::kSizeLength);
  }

  LengthSize b = FillLayer::InitialFillSizeLength(layer->GetType());

  if (identifier_value &&
      (identifier_value->GetValueID() == CSSValueID::kContain ||
       identifier_value->GetValueID() == CSSValueID::kCover)) {
    layer->SetSizeLength(b);
    return;
  }

  Length first_length;
  Length second_length;

  if (const auto* pair = DynamicTo<CSSValuePair>(value)) {
    first_length =
        StyleBuilderConverter::ConvertLengthOrAuto(state, pair->First());
    second_length =
        StyleBuilderConverter::ConvertLengthOrAuto(state, pair->Second());
  } else {
    DCHECK(value.IsPrimitiveValue() || value.IsIdentifierValue());
    first_length = StyleBuilderConverter::ConvertLengthOrAuto(state, value);
    second_length = Length();
  }

  b.SetWidth(first_length);
  b.SetHeight(second_length);
  layer->SetSizeLength(b);
}

void CSSToStyleMap::MapFillPositionX(StyleResolverState& state,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetPositionX(FillLayer::InitialFillPositionX(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair()) {
    return;
  }

  Length length;
  auto* pair = DynamicTo<CSSValuePair>(value);
  if (pair) {
    length = To<CSSPrimitiveValue>(pair->Second())
                 .ConvertToLength(state.CssToLengthConversionData());
  } else {
    length = StyleBuilderConverter::ConvertPositionLength<CSSValueID::kLeft,
                                                          CSSValueID::kRight>(
        state, value);
  }

  layer->SetPositionX(length);
  if (pair) {
    layer->SetBackgroundXOrigin(To<CSSIdentifierValue>(pair->First())
                                    .ConvertTo<BackgroundEdgeOrigin>());
  }
}

void CSSToStyleMap::MapFillPositionY(StyleResolverState& state,
                                     FillLayer* layer,
                                     const CSSValue& value) {
  if (value.IsInitialValue()) {
    layer->SetPositionY(FillLayer::InitialFillPositionY(layer->GetType()));
    return;
  }

  const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (!identifier_value && !value.IsPrimitiveValue() && !value.IsValuePair()) {
    return;
  }

  Length length;
  auto* pair = DynamicTo<CSSValuePair>(value);
  if (pair) {
    length = To<CSSPrimitiveValue>(pair->Second())
                 .ConvertToLength(state.CssToLengthConversionData());
  } else {
    length = StyleBuilderConverter::ConvertPositionLength<CSSValueID::kTop,
                                                          CSSValueID::kBottom>(
        state, value);
  }

  layer->SetPositionY(length);
  if (pair) {
    layer->SetBackgroundYOrigin(To<CSSIdentifierValue>(pair->First())
                                    .ConvertTo<BackgroundEdgeOrigin>());
  }
}

namespace {

Timing::Delay MapAnimationTimingDelay(const CSSLengthResolver& length_resolver,
                                      const CSSValue& value) {
  if (const auto* primitive = DynamicTo<CSSPrimitiveValue>(value)) {
    return Timing::Delay(
        AnimationTimeDelta(primitive->ComputeSeconds(length_resolver)));
  }

  return Timing::Delay();
}

}  // namespace

Timing::Delay CSSToStyleMap::MapAnimationDelayStart(StyleResolverState& state,
                                                    const CSSValue& value) {
  return MapAnimationTimingDelay(state.CssToLengthConversionData(), value);
}

Timing::Delay CSSToStyleMap::MapAnimationDelayEnd(const CSSValue& value) {
  // Note: using default length resolver here, as this function is only
  // called from the serialization code.
  return MapAnimationTimingDelay(CSSToLengthConversionData(), value);
}

Timing::Delay CSSToStyleMap::MapAnimationDelayEnd(StyleResolverState& state,
                                                  const CSSValue& value) {
  return MapAnimationTimingDelay(state.CssToLengthConversionData(), value);
}

Timing::PlaybackDirection CSSToStyleMap::MapAnimationDirection(
    StyleResolverState& state,
    const CSSValue& value) {
  switch (To<CSSIdentifierValue>(value).GetValueID()) {
    case CSSValueID::kNormal:
      return Timing::PlaybackDirection::NORMAL;
    case CSSValueID::kAlternate:
      return Timing::PlaybackDirection::ALTERNATE_NORMAL;
    case CSSValueID::kReverse:
      return Timing::PlaybackDirection::REVERSE;
    case CSSValueID::kAlternateReverse:
      return Timing::PlaybackDirection::ALTERNATE_REVERSE;
    default:
      NOTREACHED_IN_MIGRATION();
      return Timing::PlaybackDirection::NORMAL;
  }
}

std::optional<double> CSSToStyleMap::MapAnimationDuration(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* identifier = DynamicTo<CSSIdentifierValue>(value);
      identifier && identifier->GetValueID() == CSSValueID::kAuto) {
    return std::nullopt;
  }
  return To<CSSPrimitiveValue>(value).ComputeSeconds();
}

Timing::FillMode CSSToStyleMap::MapAnimationFillMode(StyleResolverState& state,
                                                     const CSSValue& value) {
  switch (To<CSSIdentifierValue>(value).GetValueID()) {
    case CSSValueID::kNone:
      return Timing::FillMode::NONE;
    case CSSValueID::kForwards:
      return Timing::FillMode::FORWARDS;
    case CSSValueID::kBackwards:
      return Timing::FillMode::BACKWARDS;
    case CSSValueID::kBoth:
      return Timing::FillMode::BOTH;
    default:
      NOTREACHED_IN_MIGRATION();
      return Timing::FillMode::NONE;
  }
}

double CSSToStyleMap::MapAnimationIterationCount(StyleResolverState& state,
                                                 const CSSValue& value) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value &&
      identifier_value->GetValueID() == CSSValueID::kInfinite) {
    return std::numeric_limits<double>::infinity();
  }
  return To<CSSPrimitiveValue>(value).GetFloatValue();
}

AtomicString CSSToStyleMap::MapAnimationName(StyleResolverState& state,
                                             const CSSValue& value) {
  if (auto* custom_ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    return AtomicString(custom_ident_value->Value());
  }
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
  return CSSAnimationData::InitialName();
}

CSSTransitionData::TransitionBehavior CSSToStyleMap::MapAnimationBehavior(
    StyleResolverState& state,
    const CSSValue& value) {
  if (auto* ident_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (ident_value->GetValueID()) {
      case CSSValueID::kNormal:
        return CSSTransitionData::TransitionBehavior::kNormal;
      case CSSValueID::kAllowDiscrete:
        return CSSTransitionData::TransitionBehavior::kAllowDiscrete;
      default:
        break;
    }
  }
  return CSSTransitionData::InitialBehavior();
}

StyleTimeline CSSToStyleMap::MapAnimationTimeline(StyleResolverState& state,
                                                  const CSSValue& value) {
  DCHECK(value.IsScopedValue());
  if (auto* ident = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK(ident->GetValueID() == CSSValueID::kAuto ||
           ident->GetValueID() == CSSValueID::kNone);
    return StyleTimeline(ident->GetValueID());
  }
  if (auto* custom_ident = DynamicTo<CSSCustomIdentValue>(value)) {
    return StyleTimeline(MakeGarbageCollected<ScopedCSSName>(
        custom_ident->Value(), custom_ident->GetTreeScope()));
  }
  if (value.IsViewValue()) {
    const auto& view_value = To<cssvalue::CSSViewValue>(value);
    const auto* axis_value = DynamicTo<CSSIdentifierValue>(view_value.Axis());
    TimelineAxis axis = axis_value ? axis_value->ConvertTo<TimelineAxis>()
                                   : StyleTimeline::ViewData::DefaultAxis();
    const auto* inset_value = view_value.Inset();
    TimelineInset inset =
        inset_value ? StyleBuilderConverter::ConvertSingleTimelineInset(
                          state, *inset_value)
                    : TimelineInset();
    return StyleTimeline(StyleTimeline::ViewData(axis, inset));
  }

  DCHECK(value.IsScrollValue());
  const auto& scroll_value = To<cssvalue::CSSScrollValue>(value);
  const auto* axis_value = DynamicTo<CSSIdentifierValue>(scroll_value.Axis());
  const auto* scroller_value =
      DynamicTo<CSSIdentifierValue>(scroll_value.Scroller());

  TimelineAxis axis = axis_value ? axis_value->ConvertTo<TimelineAxis>()
                                 : StyleTimeline::ScrollData::DefaultAxis();
  TimelineScroller scroller =
      scroller_value ? scroller_value->ConvertTo<TimelineScroller>()
                     : StyleTimeline::ScrollData::DefaultScroller();

  return StyleTimeline(StyleTimeline::ScrollData(axis, scroller));
}

EAnimPlayState CSSToStyleMap::MapAnimationPlayState(StyleResolverState& state,
                                                    const CSSValue& value) {
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kPaused) {
    return EAnimPlayState::kPaused;
  }
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kRunning);
  return EAnimPlayState::kPlaying;
}

namespace {

std::optional<TimelineOffset> MapAnimationRange(StyleResolverState& state,
                                                const CSSValue& value,
                                                double default_percent) {
  if (auto* ident = DynamicTo<CSSIdentifierValue>(value);
      ident && ident->GetValueID() == CSSValueID::kNormal) {
    return std::nullopt;
  }
  const auto& list = To<CSSValueList>(value);
  DCHECK_GE(list.length(), 1u);
  DCHECK_LE(list.length(), 2u);
  TimelineOffset::NamedRange range_name = TimelineOffset::NamedRange::kNone;
  Length offset = Length::Percent(default_percent);
  if (list.Item(0).IsIdentifierValue()) {
    range_name = To<CSSIdentifierValue>(list.Item(0))
                     .ConvertTo<TimelineOffset::NamedRange>();
    if (list.length() == 2u) {
      offset = StyleBuilderConverter::ConvertLength(state, list.Item(1));
    }
  } else {
    offset = StyleBuilderConverter::ConvertLength(state, list.Item(0));
  }

  return TimelineOffset(range_name, offset);
}

}  // namespace

std::optional<TimelineOffset> CSSToStyleMap::MapAnimationRangeStart(
    StyleResolverState& state,
    const CSSValue& value) {
  return MapAnimationRange(state, value, 0);
}

std::optional<TimelineOffset> CSSToStyleMap::MapAnimationRangeEnd(
    StyleResolverState& state,
    const CSSValue& value) {
  return MapAnimationRange(state, value, 100);
}

EffectModel::CompositeOperation CSSToStyleMap::MapAnimationComposition(
    StyleResolverState& state,
    const CSSValue& value) {
  switch (To<CSSIdentifierValue>(value).GetValueID()) {
    case CSSValueID::kAdd:
      return EffectModel::kCompositeAdd;
    case CSSValueID::kAccumulate:
      return EffectModel::kCompositeAccumulate;
    case CSSValueID::kReplace:
    default:
      return EffectModel::kCompositeReplace;
  }
}

CSSTransitionData::TransitionProperty CSSToStyleMap::MapAnimationProperty(
    StyleResolverState& state,
    const CSSValue& value) {
  if (const auto* custom_ident_value = DynamicTo<CSSCustomIdentValue>(value)) {
    if (custom_ident_value->IsKnownPropertyID()) {
      return CSSTransitionData::TransitionProperty(
          custom_ident_value->ValueAsPropertyID());
    }
    return CSSTransitionData::TransitionProperty(custom_ident_value->Value());
  }
  if (To<CSSIdentifierValue>(value).GetValueID() == CSSValueID::kAll) {
    return CSSTransitionData::InitialProperty();
  }
  DCHECK_EQ(To<CSSIdentifierValue>(value).GetValueID(), CSSValueID::kNone);
  return CSSTransitionData::TransitionProperty(
      CSSTransitionData::kTransitionNone);
}

scoped_refptr<TimingFunction> CSSToStyleMap::MapAnimationTimingFunction(
    const CSSValue& value) {
  // FIXME: We should probably only call into this function with a valid
  // single timing function value which isn't initial or inherit. We can
  // currently get into here with initial since the parser expands unset
  // properties in shorthands to initial.

  if (const auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kLinear:
        return LinearTimingFunction::Shared();
      case CSSValueID::kEase:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE);
      case CSSValueID::kEaseIn:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN);
      case CSSValueID::kEaseOut:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_OUT);
      case CSSValueID::kEaseInOut:
        return CubicBezierTimingFunction::Preset(
            CubicBezierTimingFunction::EaseType::EASE_IN_OUT);
      case CSSValueID::kStepStart:
        return StepsTimingFunction::Preset(
            StepsTimingFunction::StepPosition::START);
      case CSSValueID::kStepEnd:
        return StepsTimingFunction::Preset(
            StepsTimingFunction::StepPosition::END);
      default:
        NOTREACHED_IN_MIGRATION();
        return CSSTimingData::InitialTimingFunction();
    }
  }

  if (const auto* linear_timing_function =
          DynamicTo<cssvalue::CSSLinearTimingFunctionValue>(value)) {
    return LinearTimingFunction::Create(linear_timing_function->Points());
  }

  if (const auto* cubic_timing_function =
          DynamicTo<cssvalue::CSSCubicBezierTimingFunctionValue>(value)) {
    return CubicBezierTimingFunction::Create(
        cubic_timing_function->X1(), cubic_timing_function->Y1(),
        cubic_timing_function->X2(), cubic_timing_function->Y2());
  }

  const auto& steps_timing_function =
      To<cssvalue::CSSStepsTimingFunctionValue>(value);
  return StepsTimingFunction::Create(steps_timing_function.NumberOfSteps(),
                                     steps_timing_function.GetStepPosition());
}

scoped_refptr<TimingFunction> CSSToStyleMap::MapAnimationTimingFunction(
    StyleResolverState& state,
    const CSSValue& value) {
  return MapAnimationTimingFunction(value);
}

void CSSToStyleMap::MapNinePieceImage(StyleResolverState& state,
                                      CSSPropertyID property,
                                      const CSSValue& value,
                                      NinePieceImage& image) {
  // Retrieve the border image value.
  const auto* border_image = DynamicTo<CSSValueList>(value);

  // If we're not a value list, then we are "none" and don't need to alter the
  // empty image at all.
  if (!border_image) {
    return;
  }

  // Set the image (this kicks off the load).
  CSSPropertyID image_property;
  if (property == CSSPropertyID::kWebkitBorderImage) {
    image_property = CSSPropertyID::kBorderImageSource;
  } else if (property == CSSPropertyID::kWebkitMaskBoxImage) {
    image_property = CSSPropertyID::kWebkitMaskBoxImageSource;
  } else {
    image_property = property;
  }

  for (unsigned i = 0; i < border_image->length(); ++i) {
    const CSSValue& current = border_image->Item(i);

    if (current.IsImageValue() || current.IsImageGeneratorValue() ||
        current.IsImageSetValue()) {
      image.SetImage(state.GetStyleImage(image_property, current));
    } else if (current.IsBorderImageSliceValue()) {
      MapNinePieceImageSlice(state, current, image);
    } else if (const auto* slash_list = DynamicTo<CSSValueList>(current)) {
      size_t length = slash_list->length();
      // Map in the image slices.
      if (length && slash_list->Item(0).IsBorderImageSliceValue()) {
        MapNinePieceImageSlice(state, slash_list->Item(0), image);
      }

      // Map in the border slices.
      if (length > 1) {
        image.SetBorderSlices(
            MapNinePieceImageQuad(state, slash_list->Item(1)));
      }

      // Map in the outset.
      if (length > 2) {
        image.SetOutset(MapNinePieceImageQuad(state, slash_list->Item(2)));
      }
    } else if (current.IsPrimitiveValue() || current.IsValuePair()) {
      // Set the appropriate rules for stretch/round/repeat of the slices.
      MapNinePieceImageRepeat(state, current, image);
    }
  }

  if (property == CSSPropertyID::kWebkitBorderImage) {
    ComputedStyleBuilder& builder = state.StyleBuilder();
    // We have to preserve the legacy behavior of -webkit-border-image and make
    // the border slices also set the border widths. We don't need to worry
    // about percentages, since we don't even support those on real borders yet.
    if (image.BorderSlices().Top().IsLength() &&
        image.BorderSlices().Top().length().IsFixed()) {
      builder.SetBorderTopWidth(image.BorderSlices().Top().length().Pixels());
    }
    if (image.BorderSlices().Right().IsLength() &&
        image.BorderSlices().Right().length().IsFixed()) {
      builder.SetBorderRightWidth(
          image.BorderSlices().Right().length().Pixels());
    }
    if (image.BorderSlices().Bottom().IsLength() &&
        image.BorderSlices().Bottom().length().IsFixed()) {
      builder.SetBorderBottomWidth(
          image.BorderSlices().Bottom().length().Pixels());
    }
    if (image.BorderSlices().Left().IsLength() &&
        image.BorderSlices().Left().length().IsFixed()) {
      builder.SetBorderLeftWidth(image.BorderSlices().Left().length().Pixels());
    }
  }
}

static Length ConvertBorderImageSliceSide(
    const CSSLengthResolver& length_resolver,
    const CSSPrimitiveValue& value) {
  if (value.IsPercentage()) {
    return Length::Percent(value.ComputePercentage(length_resolver));
  }
  return Length::Fixed(round(value.ComputeNumber(length_resolver)));
}

void CSSToStyleMap::MapNinePieceImageSlice(StyleResolverState& state,
                                           const CSSValue& value,
                                           NinePieceImage& image) {
  if (!IsA<cssvalue::CSSBorderImageSliceValue>(value)) {
    return;
  }

  // Retrieve the border image value.
  const auto& border_image_slice =
      To<cssvalue::CSSBorderImageSliceValue>(value);

  // Set up a length box to represent our image slices.
  LengthBox box;
  const CSSQuadValue& slices = border_image_slice.Slices();
  box.top_ = ConvertBorderImageSliceSide(state.CssToLengthConversionData(),
                                         To<CSSPrimitiveValue>(*slices.Top()));
  box.bottom_ =
      ConvertBorderImageSliceSide(state.CssToLengthConversionData(),
                                  To<CSSPrimitiveValue>(*slices.Bottom()));
  box.left_ = ConvertBorderImageSliceSide(
      state.CssToLengthConversionData(), To<CSSPrimitiveValue>(*slices.Left()));
  box.right_ =
      ConvertBorderImageSliceSide(state.CssToLengthConversionData(),
                                  To<CSSPrimitiveValue>(*slices.Right()));
  image.SetImageSlices(box);

  // Set our fill mode.
  image.SetFill(border_image_slice.Fill());
}

static BorderImageLength ToBorderImageLength(const StyleResolverState& state,
                                             const CSSValue& value) {
  if (const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    if (primitive_value->IsNumber()) {
      return primitive_value->ComputeNumber(state.CssToLengthConversionData());
    }
  }
  return StyleBuilderConverter::ConvertLengthOrAuto(state, value);
}

BorderImageLengthBox CSSToStyleMap::MapNinePieceImageQuad(
    StyleResolverState& state,
    const CSSValue& value) {
  const auto* slices = DynamicTo<CSSQuadValue>(value);
  if (!slices) {
    return BorderImageLengthBox(Length::Auto());
  }

  // Set up a border image length box to represent our image slices.
  return BorderImageLengthBox(ToBorderImageLength(state, *slices->Top()),
                              ToBorderImageLength(state, *slices->Right()),
                              ToBorderImageLength(state, *slices->Bottom()),
                              ToBorderImageLength(state, *slices->Left()));
}

void CSSToStyleMap::MapNinePieceImageRepeat(StyleResolverState&,
                                            const CSSValue& value,
                                            NinePieceImage& image) {
  CSSValueID first_identifier;
  CSSValueID second_identifier;

  const auto* pair = DynamicTo<CSSValuePair>(value);
  if (pair != nullptr) {
    first_identifier = To<CSSIdentifierValue>(pair->First()).GetValueID();
    second_identifier = To<CSSIdentifierValue>(pair->Second()).GetValueID();
  } else {
    first_identifier = second_identifier =
        To<CSSIdentifierValue>(value).GetValueID();
  }

  ENinePieceImageRule horizontal_rule;
  switch (first_identifier) {
    case CSSValueID::kStretch:
      horizontal_rule = kStretchImageRule;
      break;
    case CSSValueID::kRound:
      horizontal_rule = kRoundImageRule;
      break;
    case CSSValueID::kSpace:
      horizontal_rule = kSpaceImageRule;
      break;
    default:  // CSSValueID::kRepeat
      horizontal_rule = kRepeatImageRule;
      break;
  }
  image.SetHorizontalRule(horizontal_rule);

  ENinePieceImageRule vertical_rule;
  switch (second_identifier) {
    case CSSValueID::kStretch:
      vertical_rule = kStretchImageRule;
      break;
    case CSSValueID::kRound:
      vertical_rule = kRoundImageRule;
      break;
    case CSSValueID::kSpace:
      vertical_rule = kSpaceImageRule;
      break;
    default:  // CSSValueID::kRepeat
      vertical_rule = kRepeatImageRule;
      break;
  }
  image.SetVerticalRule(vertical_rule);
}

}  // namespace blink
