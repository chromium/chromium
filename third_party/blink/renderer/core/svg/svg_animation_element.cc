/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_animation_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/element_smil_animations.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_animate_motion_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

SVGAnimationElement::SVGAnimationElement(const QualifiedName& tag_name,
                                         Document& document)
    : SVGSMILElement(tag_name, document),
      animation_valid_(AnimationValidity::kUnknown),
      registered_animation_(false),
      calc_mode_(kCalcModeLinear),
      animation_mode_(kNoAnimation) {
  UseCounter::Count(document, WebFeature::kSVGAnimationElement);
}

bool SVGAnimationElement::ParseValues(const String& value,
                                      Vector<String>& result) {
  // Per the SMIL specification, leading and trailing white space, and white
  // space before and after semicolon separators, is allowed and will be
  // ignored.
  // http://www.w3.org/TR/SVG11/animate.html#ValuesAttribute
  result.clear();
  Vector<String> parse_list;
  value.Split(';', true, parse_list);
  unsigned last = parse_list.size() - 1;
  for (unsigned i = 0; i <= last; ++i) {
    parse_list[i] = parse_list[i].StripWhiteSpace(IsHTMLSpace<UChar>);
    if (parse_list[i].empty()) {
      // Tolerate trailing ';'
      if (i < last)
        goto fail;
    } else {
      result.push_back(parse_list[i]);
    }
  }

  return true;
fail:
  result.clear();
  return false;
}

static bool IsInZeroToOneRange(float value) {
  return value >= 0 && value <= 1;
}

static bool ParseKeyTimes(const String& string,
                          HeapVector<float>& result,
                          bool verify_order) {
  result.clear();
  Vector<String> parse_list;
  string.Split(';', true, parse_list);
  for (unsigned n = 0; n < parse_list.size(); ++n) {
    String time_string = parse_list[n].StripWhiteSpace();
    bool ok;
    float time = time_string.ToFloat(&ok);
    if (!ok || !IsInZeroToOneRange(time))
      goto fail;
    if (verify_order) {
      if (!n) {
        if (time)
          goto fail;
      } else if (time < result.back()) {
        goto fail;
      }
    }
    result.push_back(time);
  }
  return true;
fail:
  result.clear();
  return false;
}

template <typename CharType>
static bool ParseKeySplinesInternal(const CharType* ptr,
                                    const CharType* end,
                                    Vector<gfx::CubicBezier>& result) {
  SkipOptionalSVGSpaces(ptr, end);

  while (ptr < end) {
    float cp1x = 0;
    if (!ParseNumber(ptr, end, cp1x))
      return false;

    float cp1y = 0;
    if (!ParseNumber(ptr, end, cp1y))
      return false;

    float cp2x = 0;
    if (!ParseNumber(ptr, end, cp2x))
      return false;

    float cp2y = 0;
    if (!ParseNumber(ptr, end, cp2y, kDisallowWhitespace))
      return false;

    SkipOptionalSVGSpaces(ptr, end);

    if (ptr < end && *ptr == ';')
      ptr++;
    SkipOptionalSVGSpaces(ptr, end);

    // The values of cpx1 cpy1 cpx2 cpy2 must all be in the range 0 to 1.
    if (!IsInZeroToOneRange(cp1x) || !IsInZeroToOneRange(cp1y) ||
        !IsInZeroToOneRange(cp2x) || !IsInZeroToOneRange(cp2y))
      return false;

    result.push_back(gfx::CubicBezier(cp1x, cp1y, cp2x, cp2y));
  }

  return ptr == end;
}

static bool ParseKeySplines(const String& string,
                            Vector<gfx::CubicBezier>& result) {
  result.clear();
  if (string.empty())
    return true;
  bool parsed = WTF::VisitCharacters(string, [&](auto chars) {
    return ParseKeySplinesInternal(chars.data(), chars.data() + chars.size(),
                                   result);
  });
  if (!parsed) {
    result.clear();
    return false;
  }
  return true;
}

void SVGAnimationElement::Trace(Visitor* visitor) const {
  visitor->Trace(key_times_from_attribute_);
  visitor->Trace(key_times_for_paced_);
  visitor->Trace(key_points_);
  SVGSMILElement::Trace(visitor);
}

void SVGAnimationElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == svg_names::kValuesAttr) {
    if (!ParseValues(params.new_value, values_)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
    }
    AnimationAttributeChanged();
    return;
  }

  if (name == svg_names::kKeyTimesAttr) {
    if (!ParseKeyTimes(params.new_value, key_times_from_attribute_, true)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
    }
    AnimationAttributeChanged();
    return;
  }

  if (name == svg_names::kKeyPointsAttr) {
    if (IsA<SVGAnimateMotionElement>(*this)) {
      // This is specified to be an animateMotion attribute only but it is
      // simpler to put it here where the other timing calculatations are.
      if (!ParseKeyTimes(params.new_value, key_points_, false)) {
        ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                    params.new_value);
      }
    }
    AnimationAttributeChanged();
    return;
  }

  if (name == svg_names::kKeySplinesAttr) {
    if (!ParseKeySplines(params.new_value, key_splines_)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
    }
    AnimationAttributeChanged();
    return;
  }

  if (name == svg_names::kCalcModeAttr) {
    SetCalcMode(params.new_value);
    AnimationAttributeChanged();
    return;
  }

  if (name == svg_names::kFromAttr || name == svg_names::kToAttr ||
      name == svg_names::kByAttr) {
    AnimationAttributeChanged();
    return;
  }

  SVGSMILElement::ParseAttribute(params);
}

void SVGAnimationElement::AnimationAttributeChanged() {
  // Assumptions may not hold after an attribute change.
  animation_valid_ = AnimationValidity::kUnknown;
  last_values_animation_from_ = String();
  last_values_animation_to_ = String();
}

void SVGAnimationElement::UnregisterAnimation(
    const QualifiedName& attribute_name) {
  if (!registered_animation_)
    return;
  DCHECK(targetElement());
  SVGElement* target = targetElement();
  if (ElementSMILAnimations* smil_animations = target->GetSMILAnimations())
    smil_animations->RemoveAnimation(attribute_name, this);
  registered_animation_ = false;
}

void SVGAnimationElement::RegisterAnimation(
    const QualifiedName& attribute_name) {
  DCHECK(!registered_animation_);
  if (!HasValidTarget() || !HasValidAnimation())
    return;
  SVGElement* target = targetElement();
  ElementSMILAnimations& smil_animations = target->EnsureSMILAnimations();
  smil_animations.AddAnimation(attribute_name, this);
  registered_animation_ = true;
}

void SVGAnimationElement::WillChangeAnimationTarget() {
  SVGSMILElement::WillChangeAnimationTarget();
  AnimationAttributeChanged();
}

float SVGAnimationElement::getStartTime(ExceptionState& exception_state) const {
  SMILTime start_time = IntervalBegin();
  if (!start_time.IsFinite()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No current interval.");
    return 0;
  }
  return ClampTo<float>(start_time.InSecondsF());
}

float SVGAnimationElement::getCurrentTime() const {
  return ClampTo<float>(Elapsed().InSecondsF());
}

float SVGAnimationElement::getSimpleDuration(
    ExceptionState& exception_state) const {
  SMILTime duration = SimpleDuration();
  if (!duration.IsFinite()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No simple duration defined.");
    return 0;
  }
  return ClampTo<float>(duration.InSecondsF());
}

void SVGAnimationElement::beginElementAt(float offset) {
  DCHECK(std::isfinite(offset));
  AddInstanceTimeAndUpdate(kBegin, Elapsed() + SMILTime::FromSecondsD(offset),
                           SMILTimeOrigin::kScript);
}

void SVGAnimationElement::endElementAt(float offset) {
  DCHECK(std::isfinite(offset));
  AddInstanceTimeAndUpdate(kEnd, Elapsed() + SMILTime::FromSecondsD(offset),
                           SMILTimeOrigin::kScript);
}

AnimationMode SVGAnimationElement::CalculateAnimationMode() {
  // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#AnimFuncValues
  if (hasAttribute(svg_names::kValuesAttr)) {
    return kValuesAnimation;
  }
  if (!ToValue().empty()) {
    return FromValue().empty() ? kToAnimation : kFromToAnimation;
  }
  if (!ByValue().empty()) {
    return FromValue().empty() ? kByAnimation : kFromByAnimation;
  }
  return kNoAnimation;
}

void SVGAnimationElement::SetCalcMode(const AtomicString& calc_mode) {
  DEFINE_STATIC_LOCAL(const AtomicString, discrete, ("discrete"));
  DEFINE_STATIC_LOCAL(const AtomicString, linear, ("linear"));
  DEFINE_STATIC_LOCAL(const AtomicString, paced, ("paced"));
  DEFINE_STATIC_LOCAL(const AtomicString, spline, ("spline"));
  if (calc_mode == discrete) {
    UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModeDiscrete);
    SetCalcMode(kCalcModeDiscrete);
  } else if (calc_mode == linear) {
    if (IsA<SVGAnimateMotionElement>(*this))
      UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModeLinear);
    // else linear is the default.
    SetCalcMode(kCalcModeLinear);
  } else if (calc_mode == paced) {
    if (!IsA<SVGAnimateMotionElement>(*this))
      UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModePaced);
    // else paced is the default.
    SetCalcMode(kCalcModePaced);
  } else if (calc_mode == spline) {
    UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModeSpline);
    SetCalcMode(kCalcModeSpline);
  } else {
    SetCalcMode(IsA<SVGAnimateMotionElement>(*this) ? kCalcModePaced
                                                    : kCalcModeLinear);
  }
}

String SVGAnimationElement::ToValue() const {
  return FastGetAttribute(svg_names::kToAttr);
}

String SVGAnimationElement::ByValue() const {
  return FastGetAttribute(svg_names::kByAttr);
}

String SVGAnimationElement::FromValue() const {
  return FastGetAttribute(svg_names::kFromAttr);
}

bool SVGAnimationElement::IsAdditive() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
  const AtomicString& value = FastGetAttribute(svg_names::kAdditiveAttr);
  return value == sum;
}

bool SVGAnimationElement::IsAccumulated() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
  const AtomicString& value = FastGetAttribute(svg_names::kAccumulateAttr);
  return value == sum;
}

void SVGAnimationElement::CalculateKeyTimesForCalcModePaced() {
  DCHECK_EQ(GetCalcMode(), kCalcModePaced);
  DCHECK_EQ(GetAnimationMode(), kValuesAnimation);

  unsigned values_count = values_.size();
  DCHECK_GE(values_count, 1u);
  if (values_count == 1) {
    // Don't swap lists.
    use_paced_key_times_ = false;
    return;
  }
  // Clear the list and use it, even if the rest of the function fail
  use_paced_key_times_ = true;
  key_times_for_paced_.clear();

  HeapVector<float> calculated_key_times;
  float total_distance = 0;
  calculated_key_times.push_back(0);
  for (unsigned n = 0; n < values_count - 1; ++n) {
    // Distance in any units
    float distance = CalculateDistance(values_[n], values_[n + 1]);
    if (distance < 0) {
      return;
    }
    total_distance += distance;
    calculated_key_times.push_back(distance);
  }
  if (!std::isfinite(total_distance) || !total_distance)
    return;

  // Normalize.
  for (unsigned n = 1; n < calculated_key_times.size() - 1; ++n) {
    calculated_key_times[n] =
        calculated_key_times[n - 1] + calculated_key_times[n] / total_distance;
  }
  calculated_key_times.back() = 1.0f;
  key_times_for_paced_.swap(calculated_key_times);
}

static inline double SolveEpsilon(double duration) {
  return 1 / (200 * duration);
}

unsigned SVGAnimationElement::CalculateKeyTimesIndex(float percent) const {
  unsigned index;
  unsigned key_times_count = KeyTimes().size();
  // For linear and spline animations, the last value must be '1'. In those
  // cases we don't need to consider the last value, since |percent| is never
  // greater than one.
  if (key_times_count && GetCalcMode() != kCalcModeDiscrete)
    key_times_count--;
  for (index = 1; index < key_times_count; ++index) {
    if (KeyTimes()[index] > percent)
      break;
  }
  return --index;
}

float SVGAnimationElement::CalculatePercentForSpline(
    float percent,
    unsigned spline_index) const {
  DCHECK_EQ(GetCalcMode(), kCalcModeSpline);
  SECURITY_DCHECK(spline_index < key_splines_.size());
  gfx::CubicBezier bezier = key_splines_[spline_index];
  SMILTime duration = SimpleDuration();
  if (!duration.IsFinite())
    duration = SMILTime::FromSecondsD(100.0);
  return ClampTo<float>(
      bezier.SolveWithEpsilon(percent, SolveEpsilon(duration.InSecondsF())));
}

float SVGAnimationElement::CalculatePercentFromKeyPoints(float percent) const {
  DCHECK(GetCalcMode() != kCalcModePaced ||
         GetAnimationMode() == kPathAnimation);
  DCHECK_GT(KeyTimes().size(), 1u);
  DCHECK(!key_points_.empty());
  DCHECK_EQ(key_points_.size(), KeyTimes().size());

  if (percent == 1)
    return key_points_[key_points_.size() - 1];

  unsigned index = CalculateKeyTimesIndex(percent);
  float from_key_point = key_points_[index];

  if (GetCalcMode() == kCalcModeDiscrete)
    return from_key_point;

  DCHECK_LT(index + 1, KeyTimes().size());
  float from_percent = KeyTimes()[index];
  float to_percent = KeyTimes()[index + 1];
  float to_key_point = key_points_[index + 1];
  float key_point_percent =
      (percent - from_percent) / (to_percent - from_percent);

  if (GetCalcMode() == kCalcModeSpline) {
    DCHECK_EQ(key_splines_.size(), key_points_.size() - 1);
    key_point_percent = CalculatePercentForSpline(key_point_percent, index);
  }
  return (to_key_point - from_key_point) * key_point_percent + from_key_point;
}

float SVGAnimationElement::CalculatePercentForFromTo(float percent) const {
  if (GetCalcMode() == kCalcModeDiscrete && KeyTimes().size() == 2)
    return percent > KeyTimes()[1] ? 1 : 0;

  return percent;
}

float SVGAnimationElement::CurrentValuesFromKeyPoints(float percent,
                                                      String& from,
                                                      String& to) const {
  DCHECK_NE(GetCalcMode(), kCalcModePaced);
  DCHECK(!key_points_.empty());
  DCHECK_EQ(key_points_.size(), KeyTimes().size());
  float effective_percent = CalculatePercentFromKeyPoints(percent);
  unsigned index =
      effective_percent == 1
          ? values_.size() - 2
          : static_cast<unsigned>(effective_percent * (values_.size() - 1));
  from = values_[index];
  to = values_[index + 1];
  return effective_percent;
}

float SVGAnimationElement::CurrentValuesForValuesAnimation(float percent,
                                                           String& from,
                                                           String& to) const {
  unsigned values_count = values_.size();
  DCHECK_EQ(animation_valid_, AnimationValidity::kValid);
  DCHECK_GE(values_count, 1u);

  if (percent == 1 || values_count == 1) {
    from = values_[values_count - 1];
    to = values_[values_count - 1];
    return 1;
  }

  CalcMode calc_mode = GetCalcMode();
  if (auto* animate_element = DynamicTo<SVGAnimateElement>(this)) {
    if (!animate_element->AnimatedPropertyTypeSupportsAddition())
      calc_mode = kCalcModeDiscrete;
  }
  if (!key_points_.empty() && calc_mode != kCalcModePaced)
    return CurrentValuesFromKeyPoints(percent, from, to);

  unsigned key_times_count = KeyTimes().size();
  DCHECK(!key_times_count || values_count == key_times_count);
  DCHECK(!key_times_count || (key_times_count > 1 && !KeyTimes()[0]));

  unsigned index = CalculateKeyTimesIndex(percent);
  if (calc_mode == kCalcModeDiscrete) {
    if (!key_times_count)
      index = static_cast<unsigned>(percent * values_count);
    from = values_[index];
    to = values_[index];
    return 0;
  }

  float from_percent;
  float to_percent;
  if (key_times_count) {
    from_percent = KeyTimes()[index];
    to_percent = KeyTimes()[index + 1];
  } else {
    index = static_cast<unsigned>(floorf(percent * (values_count - 1)));
    from_percent = static_cast<float>(index) / (values_count - 1);
    to_percent = static_cast<float>(index + 1) / (values_count - 1);
  }

  if (index == values_count - 1)
    --index;
  from = values_[index];
  to = values_[index + 1];
  DCHECK_GT(to_percent, from_percent);
  float effective_percent =
      (percent - from_percent) / (to_percent - from_percent);

  if (calc_mode == kCalcModeSpline) {
    DCHECK_EQ(key_splines_.size(), values_.size() - 1);
    effective_percent = CalculatePercentForSpline(effective_percent, index);
  }
  return effective_percent;
}

bool SVGAnimationElement::UpdateAnimationParameters() {
  if (!IsValid() || !HasValidTarget()) {
    return false;
  }
  animation_mode_ = CalculateAnimationMode();
  if (animation_mode_ == kNoAnimation) {
    return false;
  }
  return CheckAnimationParameters();
}

bool SVGAnimationElement::CheckAnimationParameters() const {
  DCHECK_NE(animation_mode_, kNoAnimation);

  // These validations are appropriate for all animation modes.
  const bool has_key_points = FastHasAttribute(svg_names::kKeyPointsAttr);
  const bool has_key_times = FastHasAttribute(svg_names::kKeyTimesAttr);
  if (has_key_points) {
    // Each value in 'keyPoints' should correspond to a value in 'keyTimes'.
    if (!has_key_times) {
      return false;
    }
    // If 'keyPoints' is specified it should have the same amount of points as
    // 'keyTimes'.
    if (KeyTimes().size() != key_points_.size()) {
      return false;
    }
    // ...and at least two points.
    if (KeyTimes().size() < 2) {
      return false;
    }
  }
  if (GetCalcMode() == kCalcModeSpline) {
    // If 'calcMode' is 'spline', there should be one less spline than there
    // are 'keyTimes' or 'keyPoints'.
    if (key_splines_.empty() ||
        (has_key_points && key_splines_.size() != key_points_.size() - 1) ||
        (has_key_times && key_splines_.size() != KeyTimes().size() - 1))
      return false;
  }
  if (animation_mode_ == kValuesAnimation) {
    if (values_.empty()) {
      return false;
    }
    const CalcMode calc_mode = GetCalcMode();
    // For 'values' animations, there should be exactly as many 'keyTimes' as
    // 'values'.
    if (calc_mode != kCalcModePaced && !has_key_points && has_key_times &&
        values_.size() != KeyTimes().size()) {
      return false;
    }
    // If 'keyTimes' is specified its last value should be 1 (and the first 0)
    // unless 'calcMode' is 'discrete'.
    if (calc_mode != kCalcModeDiscrete && !KeyTimes().empty() &&
        KeyTimes().back() != 1) {
      return false;
    }
    // If 'calcMode' is 'spline', there should be one less spline than there
    // are 'values'.
    if (calc_mode == kCalcModeSpline &&
        key_splines_.size() != values_.size() - 1) {
      return false;
    }
  }
  return true;
}

bool SVGAnimationElement::UpdateAnimationValues() {
  switch (GetAnimationMode()) {
    case kFromToAnimation:
      CalculateFromAndToValues(FromValue(), ToValue());
      break;
    case kToAnimation:
      // For to-animations the from value is the current accumulated value from
      // lower priority animations. The value is not static and is determined
      // during the animation.
      CalculateFromAndToValues(g_empty_string, ToValue());
      break;
    case kFromByAnimation:
      CalculateFromAndByValues(FromValue(), ByValue());
      break;
    case kByAnimation:
      CalculateFromAndByValues(g_empty_string, ByValue());
      break;
    case kValuesAnimation:
      if (!CalculateToAtEndOfDurationValue(values_.back())) {
        return false;
      }
      if (GetCalcMode() == kCalcModePaced) {
        CalculateKeyTimesForCalcModePaced();
      }
      break;
    case kPathAnimation:
      break;
    case kNoAnimation:
      NOTREACHED_IN_MIGRATION();
  }
  return true;
}

SMILAnimationEffectParameters SVGAnimationElement::ComputeEffectParameters()
    const {
  SMILAnimationEffectParameters parameters;
  parameters.is_discrete = GetCalcMode() == kCalcModeDiscrete;
  // 'to'-animations are neither additive nor cumulative.
  if (GetAnimationMode() != kToAnimation) {
    parameters.is_additive = IsAdditive() || GetAnimationMode() == kByAnimation;
    parameters.is_cumulative = IsAccumulated();
  }
  return parameters;
}

void SVGAnimationElement::ApplyAnimation(SMILAnimationValue& animation_value) {
  if (animation_valid_ == AnimationValidity::kUnknown) {
    if (UpdateAnimationParameters() && UpdateAnimationValues()) {
      animation_valid_ = AnimationValidity::kValid;

      if (IsAdditive() || GetAnimationMode() == kByAnimation ||
          (IsAccumulated() && GetAnimationMode() != kToAnimation)) {
        UseCounter::Count(&GetDocument(),
                          WebFeature::kSVGSMILAdditiveAnimation);
      }
    } else {
      animation_valid_ = AnimationValidity::kInvalid;
    }
  }
  DCHECK_NE(animation_valid_, AnimationValidity::kUnknown);

  if (animation_valid_ != AnimationValidity::kValid || !targetElement())
    return;

  const ProgressState& progress_state = GetProgressState();
  const float percent = progress_state.progress;

  float effective_percent;
  CalcMode calc_mode = GetCalcMode();
  AnimationMode animation_mode = GetAnimationMode();
  if (animation_mode == kValuesAnimation) {
    String from;
    String to;
    effective_percent = CurrentValuesForValuesAnimation(percent, from, to);
    if (from != last_values_animation_from_ ||
        to != last_values_animation_to_) {
      CalculateFromAndToValues(from, to);
      last_values_animation_from_ = from;
      last_values_animation_to_ = to;
    }
  } else if (!key_points_.empty() && (animation_mode == kPathAnimation ||
                                      calc_mode != kCalcModePaced)) {
    effective_percent = CalculatePercentFromKeyPoints(percent);
  } else if (calc_mode == kCalcModeSpline && key_points_.empty() &&
             KeyTimes().size() > 1) {
    effective_percent =
        CalculatePercentForSpline(percent, CalculateKeyTimesIndex(percent));
  } else if (animation_mode == kFromToAnimation ||
             animation_mode == kToAnimation) {
    effective_percent = CalculatePercentForFromTo(percent);
  } else {
    effective_percent = percent;
  }
  CalculateAnimationValue(animation_value, effective_percent,
                          progress_state.repeat);
}

bool SVGAnimationElement::OverwritesUnderlyingAnimationValue() const {
  // Our animation value is added to the underlying value.
  if (IsAdditive())
    return false;
  // TODO(fs): Remove this. (Is a function of the repeat count and
  // does not depend on the underlying value.)
  if (IsAccumulated())
    return false;
  // Animation is from the underlying value by (adding) the specified value.
  if (GetAnimationMode() == kByAnimation)
    return false;
  // Animation is from the underlying value to the specified value.
  if (GetAnimationMode() == kToAnimation)
    return false;
  // No animation...
  if (GetAnimationMode() == kNoAnimation)
    return false;
  return true;
}

}  // namespace blink
