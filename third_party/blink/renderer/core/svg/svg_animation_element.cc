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

#include "third_party/blink/renderer/core/svg/svg_animation_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

SVGAnimationElement::SVGAnimationElement(const QualifiedName& tag_name,
                                         Document& document)
    : SVGSMILElement(tag_name, document),
      animation_valid_(false),
      calc_mode_(kCalcModeLinear),
      animation_mode_(kNoAnimation) {
  DCHECK(RuntimeEnabledFeatures::SMILEnabled());
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
    if (parse_list[i].IsEmpty()) {
      // Tolerate trailing ';'
      if (i < last)
        goto fail;
    } else {
      parse_list[i] = parse_list[i].StripWhiteSpace(IsHTMLSpace<UChar>);
      result.push_back(parse_list[i]);
    }
  }

  return true;
fail:
  result.clear();
  return false;
}

static bool ParseKeyTimes(const String& string,
                          Vector<float>& result,
                          bool verify_order) {
  result.clear();
  Vector<String> parse_list;
  string.Split(';', true, parse_list);
  for (unsigned n = 0; n < parse_list.size(); ++n) {
    String time_string = parse_list[n].StripWhiteSpace();
    bool ok;
    float time = time_string.ToFloat(&ok);
    if (!ok || time < 0 || time > 1)
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
static bool ParseKeySplinesInternal(const String& string,
                                    Vector<gfx::CubicBezier>& result) {
  const CharType* ptr = string.GetCharacters<CharType>();
  const CharType* end = ptr + string.length();

  SkipOptionalSVGSpaces(ptr, end);

  while (ptr < end) {
    float pos_a = 0;
    if (!ParseNumber(ptr, end, pos_a))
      return false;

    float pos_b = 0;
    if (!ParseNumber(ptr, end, pos_b))
      return false;

    float pos_c = 0;
    if (!ParseNumber(ptr, end, pos_c))
      return false;

    float pos_d = 0;
    if (!ParseNumber(ptr, end, pos_d, kDisallowWhitespace))
      return false;

    SkipOptionalSVGSpaces(ptr, end);

    if (ptr < end && *ptr == ';')
      ptr++;
    SkipOptionalSVGSpaces(ptr, end);

    result.push_back(gfx::CubicBezier(pos_a, pos_b, pos_c, pos_d));
  }

  return ptr == end;
}

static bool ParseKeySplines(const String& string,
                            Vector<gfx::CubicBezier>& result) {
  result.clear();
  if (string.IsEmpty())
    return true;
  bool parsed = true;
  if (string.Is8Bit())
    parsed = ParseKeySplinesInternal<LChar>(string, result);
  else
    parsed = ParseKeySplinesInternal<UChar>(string, result);
  if (!parsed) {
    result.clear();
    return false;
  }
  return true;
}

void SVGAnimationElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == svg_names::kValuesAttr) {
    if (!ParseValues(params.new_value, values_)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
      return;
    }
    UpdateAnimationMode();
    return;
  }

  if (name == svg_names::kKeyTimesAttr) {
    if (!ParseKeyTimes(params.new_value, key_times_, true)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
    }
    return;
  }

  if (name == svg_names::kKeyPointsAttr) {
    if (IsSVGAnimateMotionElement(*this)) {
      // This is specified to be an animateMotion attribute only but it is
      // simpler to put it here where the other timing calculatations are.
      if (!ParseKeyTimes(params.new_value, key_points_, false)) {
        ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                    params.new_value);
      }
    }
    return;
  }

  if (name == svg_names::kKeySplinesAttr) {
    if (!ParseKeySplines(params.new_value, key_splines_)) {
      ReportAttributeParsingError(SVGParseStatus::kParsingFailed, name,
                                  params.new_value);
    }
    return;
  }

  if (name == svg_names::kCalcModeAttr) {
    SetCalcMode(params.new_value);
    return;
  }

  if (name == svg_names::kFromAttr || name == svg_names::kToAttr ||
      name == svg_names::kByAttr) {
    UpdateAnimationMode();
    return;
  }

  SVGSMILElement::ParseAttribute(params);
}

void SVGAnimationElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kValuesAttr || attr_name == svg_names::kByAttr ||
      attr_name == svg_names::kFromAttr || attr_name == svg_names::kToAttr ||
      attr_name == svg_names::kCalcModeAttr ||
      attr_name == svg_names::kKeySplinesAttr ||
      attr_name == svg_names::kKeyPointsAttr ||
      attr_name == svg_names::kKeyTimesAttr) {
    AnimationAttributeChanged();
    return;
  }

  SVGSMILElement::SvgAttributeChanged(attr_name);
}

void SVGAnimationElement::InvalidatedValuesCache() {
  last_values_animation_from_ = String();
  last_values_animation_to_ = String();
}

void SVGAnimationElement::AnimationAttributeChanged() {
  // Assumptions may not hold after an attribute change.
  animation_valid_ = false;
  InvalidatedValuesCache();
  SetInactive();
}

float SVGAnimationElement::getStartTime(ExceptionState& exception_state) const {
  SMILTime start_time = IntervalBegin();
  if (!start_time.IsFinite()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No current interval.");
    return 0;
  }
  return clampTo<float>(start_time.Value());
}

float SVGAnimationElement::getCurrentTime() const {
  return clampTo<float>(Elapsed().Value());
}

float SVGAnimationElement::getSimpleDuration(
    ExceptionState& exception_state) const {
  SMILTime duration = SimpleDuration();
  if (!duration.IsFinite()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "No simple duration defined.");
    return 0;
  }
  return clampTo<float>(duration.Value());
}

void SVGAnimationElement::beginElementAt(float offset) {
  DCHECK(std::isfinite(offset));
  AddInstanceTime(kBegin, Elapsed() + offset,
                  SMILTimeWithOrigin::kScriptOrigin);
}

void SVGAnimationElement::endElementAt(float offset) {
  DCHECK(std::isfinite(offset));
  AddInstanceTime(kEnd, Elapsed() + offset, SMILTimeWithOrigin::kScriptOrigin);
}

void SVGAnimationElement::UpdateAnimationMode() {
  // http://www.w3.org/TR/2001/REC-smil-animation-20010904/#AnimFuncValues
  if (hasAttribute(svg_names::kValuesAttr))
    SetAnimationMode(kValuesAnimation);
  else if (!ToValue().IsEmpty())
    SetAnimationMode(FromValue().IsEmpty() ? kToAnimation : kFromToAnimation);
  else if (!ByValue().IsEmpty())
    SetAnimationMode(FromValue().IsEmpty() ? kByAnimation : kFromByAnimation);
  else
    SetAnimationMode(kNoAnimation);
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
    if (IsSVGAnimateMotionElement(*this))
      UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModeLinear);
    // else linear is the default.
    SetCalcMode(kCalcModeLinear);
  } else if (calc_mode == paced) {
    if (!IsSVGAnimateMotionElement(*this))
      UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModePaced);
    // else paced is the default.
    SetCalcMode(kCalcModePaced);
  } else if (calc_mode == spline) {
    UseCounter::Count(GetDocument(), WebFeature::kSVGCalcModeSpline);
    SetCalcMode(kCalcModeSpline);
  } else {
    SetCalcMode(IsSVGAnimateMotionElement(*this) ? kCalcModePaced
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

bool SVGAnimationElement::IsAdditive() {
  DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
  const AtomicString& value = FastGetAttribute(svg_names::kAdditiveAttr);
  return value == sum || GetAnimationMode() == kByAnimation;
}

bool SVGAnimationElement::IsAccumulated() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sum, ("sum"));
  const AtomicString& value = FastGetAttribute(svg_names::kAccumulateAttr);
  return value == sum && GetAnimationMode() != kToAnimation;
}

void SVGAnimationElement::CalculateKeyTimesForCalcModePaced() {
  DCHECK_EQ(GetCalcMode(), kCalcModePaced);
  DCHECK_EQ(GetAnimationMode(), kValuesAnimation);

  unsigned values_count = values_.size();
  DCHECK_GE(values_count, 1u);
  if (values_count == 1)
    return;

  // FIXME, webkit.org/b/109010: m_keyTimes should not be modified in this
  // function.
  key_times_.clear();

  Vector<float> key_times_for_paced;
  float total_distance = 0;
  key_times_for_paced.push_back(0);
  for (unsigned n = 0; n < values_count - 1; ++n) {
    // Distance in any units
    float distance = CalculateDistance(values_[n], values_[n + 1]);
    if (distance < 0)
      return;
    total_distance += distance;
    key_times_for_paced.push_back(distance);
  }
  if (!total_distance)
    return;

  // Normalize.
  for (unsigned n = 1; n < key_times_for_paced.size() - 1; ++n)
    key_times_for_paced[n] =
        key_times_for_paced[n - 1] + key_times_for_paced[n] / total_distance;
  key_times_for_paced[key_times_for_paced.size() - 1] = 1;

  // Use key times calculated based on pacing instead of the user provided ones.
  key_times_ = key_times_for_paced;
}

static inline double SolveEpsilon(double duration) {
  return 1 / (200 * duration);
}

unsigned SVGAnimationElement::CalculateKeyTimesIndex(float percent) const {
  unsigned index;
  unsigned key_times_count = key_times_.size();
  // For linear and spline animations, the last value must be '1'. In those
  // cases we don't need to consider the last value, since |percent| is never
  // greater than one.
  if (key_times_count && GetCalcMode() != kCalcModeDiscrete)
    key_times_count--;
  for (index = 1; index < key_times_count; ++index) {
    if (key_times_[index] > percent)
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
    duration = 100.0;
  return clampTo<float>(
      bezier.SolveWithEpsilon(percent, SolveEpsilon(duration.Value())));
}

float SVGAnimationElement::CalculatePercentFromKeyPoints(float percent) const {
  DCHECK(!key_points_.IsEmpty());
  DCHECK_NE(GetCalcMode(), kCalcModePaced);
  DCHECK_GT(key_times_.size(), 1u);
  DCHECK_EQ(key_points_.size(), key_times_.size());

  if (percent == 1)
    return key_points_[key_points_.size() - 1];

  unsigned index = CalculateKeyTimesIndex(percent);
  float from_key_point = key_points_[index];

  if (GetCalcMode() == kCalcModeDiscrete)
    return from_key_point;

  DCHECK_LT(index + 1, key_times_.size());
  float from_percent = key_times_[index];
  float to_percent = key_times_[index + 1];
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
  if (GetCalcMode() == kCalcModeDiscrete && key_times_.size() == 2)
    return percent > key_times_[1] ? 1 : 0;

  return percent;
}

void SVGAnimationElement::CurrentValuesFromKeyPoints(float percent,
                                                     float& effective_percent,
                                                     String& from,
                                                     String& to) const {
  DCHECK(!key_points_.IsEmpty());
  DCHECK_EQ(key_points_.size(), key_times_.size());
  DCHECK_NE(GetCalcMode(), kCalcModePaced);
  effective_percent = CalculatePercentFromKeyPoints(percent);
  unsigned index =
      effective_percent == 1
          ? values_.size() - 2
          : static_cast<unsigned>(effective_percent * (values_.size() - 1));
  from = values_[index];
  to = values_[index + 1];
}

void SVGAnimationElement::CurrentValuesForValuesAnimation(
    float percent,
    float& effective_percent,
    String& from,
    String& to) {
  unsigned values_count = values_.size();
  DCHECK(animation_valid_);
  DCHECK_GE(values_count, 1u);

  if (percent == 1 || values_count == 1) {
    from = values_[values_count - 1];
    to = values_[values_count - 1];
    effective_percent = 1;
    return;
  }

  CalcMode calc_mode = this->GetCalcMode();
  if (IsSVGAnimateElement(*this)) {
    SVGAnimateElement& animate_element = ToSVGAnimateElement(*this);
    if (!animate_element.AnimatedPropertyTypeSupportsAddition())
      calc_mode = kCalcModeDiscrete;
  }
  if (!key_points_.IsEmpty() && calc_mode != kCalcModePaced)
    return CurrentValuesFromKeyPoints(percent, effective_percent, from, to);

  unsigned key_times_count = key_times_.size();
  DCHECK(!key_times_count || values_count == key_times_count);
  DCHECK(!key_times_count || (key_times_count > 1 && !key_times_[0]));

  unsigned index = CalculateKeyTimesIndex(percent);
  if (calc_mode == kCalcModeDiscrete) {
    if (!key_times_count)
      index = static_cast<unsigned>(percent * values_count);
    from = values_[index];
    to = values_[index];
    effective_percent = 0;
    return;
  }

  float from_percent;
  float to_percent;
  if (key_times_count) {
    from_percent = key_times_[index];
    to_percent = key_times_[index + 1];
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
  effective_percent = (percent - from_percent) / (to_percent - from_percent);

  if (calc_mode == kCalcModeSpline) {
    DCHECK_EQ(key_splines_.size(), values_.size() - 1);
    effective_percent = CalculatePercentForSpline(effective_percent, index);
  }
}

void SVGAnimationElement::StartedActiveInterval() {
  animation_valid_ = false;

  if (!IsValid() || !HasValidTarget())
    return;

  // These validations are appropriate for all animation modes.
  if (FastHasAttribute(svg_names::kKeyPointsAttr) &&
      key_points_.size() != key_times_.size())
    return;

  AnimationMode animation_mode = this->GetAnimationMode();
  CalcMode calc_mode = this->GetCalcMode();
  if (calc_mode == kCalcModeSpline) {
    unsigned splines_count = key_splines_.size();
    if (!splines_count ||
        (FastHasAttribute(svg_names::kKeyPointsAttr) &&
         key_points_.size() - 1 != splines_count) ||
        (animation_mode == kValuesAnimation &&
         values_.size() - 1 != splines_count) ||
        (FastHasAttribute(svg_names::kKeyTimesAttr) &&
         key_times_.size() - 1 != splines_count))
      return;
  }

  String from = FromValue();
  String to = ToValue();
  String by = ByValue();
  if (animation_mode == kNoAnimation)
    return;
  if ((animation_mode == kFromToAnimation ||
       animation_mode == kFromByAnimation || animation_mode == kToAnimation ||
       animation_mode == kByAnimation) &&
      (FastHasAttribute(svg_names::kKeyPointsAttr) &&
       FastHasAttribute(svg_names::kKeyTimesAttr) &&
       (key_times_.size() < 2 || key_times_.size() != key_points_.size())))
    return;
  if (animation_mode == kFromToAnimation) {
    animation_valid_ = CalculateFromAndToValues(from, to);
  } else if (animation_mode == kToAnimation) {
    // For to-animations the from value is the current accumulated value from
    // lower priority animations.
    // The value is not static and is determined during the animation.
    animation_valid_ = CalculateFromAndToValues(g_empty_string, to);
  } else if (animation_mode == kFromByAnimation) {
    animation_valid_ = CalculateFromAndByValues(from, by);
  } else if (animation_mode == kByAnimation) {
    animation_valid_ = CalculateFromAndByValues(g_empty_string, by);
  } else if (animation_mode == kValuesAnimation) {
    animation_valid_ =
        values_.size() >= 1 &&
        (calc_mode == kCalcModePaced ||
         !FastHasAttribute(svg_names::kKeyTimesAttr) ||
         FastHasAttribute(svg_names::kKeyPointsAttr) ||
         (values_.size() == key_times_.size())) &&
        (calc_mode == kCalcModeDiscrete || !key_times_.size() ||
         key_times_.back() == 1) &&
        (calc_mode != kCalcModeSpline ||
         ((key_splines_.size() &&
           (key_splines_.size() == values_.size() - 1)) ||
          key_splines_.size() == key_points_.size() - 1)) &&
        (!FastHasAttribute(svg_names::kKeyPointsAttr) ||
         (key_times_.size() > 1 && key_times_.size() == key_points_.size()));
    if (animation_valid_)
      animation_valid_ = CalculateToAtEndOfDurationValue(values_.back());
    if (calc_mode == kCalcModePaced && animation_valid_)
      CalculateKeyTimesForCalcModePaced();
  } else if (animation_mode == kPathAnimation) {
    animation_valid_ =
        calc_mode == kCalcModePaced ||
        !FastHasAttribute(svg_names::kKeyPointsAttr) ||
        (key_times_.size() > 1 && key_times_.size() == key_points_.size());
  }

  if (animation_valid_ && (IsAdditive() || IsAccumulated()))
    UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILAdditiveAnimation);
}

void SVGAnimationElement::UpdateAnimation(float percent,
                                          unsigned repeat_count,
                                          SVGSMILElement* result_element) {
  if (!animation_valid_ || !targetElement())
    return;

  float effective_percent;
  CalcMode calc_mode = this->GetCalcMode();
  AnimationMode animation_mode = this->GetAnimationMode();
  if (animation_mode == kValuesAnimation) {
    String from;
    String to;
    CurrentValuesForValuesAnimation(percent, effective_percent, from, to);
    if (from != last_values_animation_from_ ||
        to != last_values_animation_to_) {
      animation_valid_ = CalculateFromAndToValues(from, to);
      if (!animation_valid_)
        return;
      last_values_animation_from_ = from;
      last_values_animation_to_ = to;
    }
  } else if (!key_points_.IsEmpty() && calc_mode != kCalcModePaced)
    effective_percent = CalculatePercentFromKeyPoints(percent);
  else if (key_points_.IsEmpty() && calc_mode == kCalcModeSpline &&
           key_times_.size() > 1)
    effective_percent =
        CalculatePercentForSpline(percent, CalculateKeyTimesIndex(percent));
  else if (animation_mode == kFromToAnimation || animation_mode == kToAnimation)
    effective_percent = CalculatePercentForFromTo(percent);
  else
    effective_percent = percent;

  CalculateAnimatedValue(effective_percent, repeat_count, result_element);
}

}  // namespace blink
