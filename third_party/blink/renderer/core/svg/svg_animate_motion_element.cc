/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_animate_motion_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_value.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_clip_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_defs_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_line_element.h"
#include "third_party/blink/renderer/core/svg/svg_mpath_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_point.h"
#include "third_party/blink/renderer/core/svg/svg_polygon_element.h"
#include "third_party/blink/renderer/core/svg/svg_polyline_element.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/core/svg/svg_switch_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

namespace {

bool TargetCanHaveMotionTransform(const SVGElement& target) {
  // We don't have a special attribute name to verify the animation type. Check
  // the element name instead.
  if (IsA<SVGClipPathElement>(target)) {
    return true;
  }
  if (!IsA<SVGGraphicsElement>(target)) {
    return false;
  }
  // Spec: SVG 1.1 section 19.2.15
  // FIXME: svgTag is missing. Needs to be checked, if transforming <svg> could
  // cause problems.
  return IsA<SVGGElement>(target) || IsA<SVGDefsElement>(target) ||
         IsA<SVGUseElement>(target) || IsA<SVGImageElement>(target) ||
         IsA<SVGSwitchElement>(target) || IsA<SVGPathElement>(target) ||
         IsA<SVGRectElement>(target) || IsA<SVGCircleElement>(target) ||
         IsA<SVGEllipseElement>(target) || IsA<SVGLineElement>(target) ||
         IsA<SVGPolylineElement>(target) || IsA<SVGPolygonElement>(target) ||
         IsA<SVGTextElement>(target) || IsA<SVGAElement>(target) ||
         IsA<SVGForeignObjectElement>(target);
}

template <typename CharType>
std::optional<gfx::PointF> ParsePointInternal(base::span<const CharType> span) {
  if (!SkipOptionalSVGSpaces(span)) {
    return std::nullopt;
  }

  float x = 0;
  if (!ParseNumber(span, x)) {
    return std::nullopt;
  }

  float y = 0;
  if (!ParseNumber(span, y)) {
    return std::nullopt;
  }

  // disallow anything except spaces at the end
  if (SkipOptionalSVGSpaces(span)) {
    return std::nullopt;
  }
  return gfx::PointF(x, y);
}

base::expected<gfx::PointF, SVGParseStatus> ParsePoint(const String& string) {
  std::optional<gfx::PointF> point;
  if (!string.empty()) {
    point = VisitCharacters(
        string, [&](auto chars) { return ParsePointInternal(chars); });

    if (point.has_value()) {
      return point.value();
    }
  }
  return base::unexpected(SVGParseStatus::kParsingFailed);
}

// Used to collect the points of each path segment for discrete calcMode.
struct PointCollector {
  Vector<gfx::PointF>& points;
  gfx::PointF subpath_start;

  void CollectPoint(const PathElement& element) {
    switch (element.type) {
      case kPathElementMoveToPoint:
        subpath_start = element.points[0];
        points.push_back(element.points[0]);
        break;
      case kPathElementAddLineToPoint:
        points.push_back(element.points[0]);
        break;
      case kPathElementAddQuadCurveToPoint:
        points.push_back(element.points[1]);
        break;
      case kPathElementAddCurveToPoint:
        points.push_back(element.points[2]);
        break;
      case kPathElementCloseSubpath:
        points.push_back(subpath_start);
        break;
    }
  }
};

}  // namespace

SVGAnimateMotionElement::SVGAnimateMotionElement(Document& document)
    : SVGAnimationElement(svg_names::kAnimateMotionTag, document) {
  SetCalcMode(kCalcModePaced);
}

SVGAnimateMotionElement::~SVGAnimateMotionElement() = default;

bool SVGAnimateMotionElement::HasValidAnimation() const {
  return TargetCanHaveMotionTransform(*targetElement());
}

void SVGAnimateMotionElement::WillChangeAnimationTarget() {
  SVGAnimationElement::WillChangeAnimationTarget();
  UnregisterAnimation(svg_names::kAnimateMotionTag);
}

void SVGAnimateMotionElement::DidChangeAnimationTarget() {
  // Use our QName as the key to RegisterAnimation to get a separate sandwich
  // for animateMotion.
  RegisterAnimation(svg_names::kAnimateMotionTag);
  SVGAnimationElement::DidChangeAnimationTarget();
}

void SVGAnimateMotionElement::ChildMPathChanged() {
  AnimationAttributeChanged();
}

void SVGAnimateMotionElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kPathAttr) {
    path_ = BuildPathFromString(params.new_value);
    AnimationAttributeChanged();
    return;
  }

  SVGAnimationElement::ParseAttribute(params);
}

SVGAnimateMotionElement::RotateMode SVGAnimateMotionElement::GetRotateMode()
    const {
  DEFINE_STATIC_LOCAL(const AtomicString, auto_val, ("auto"));
  DEFINE_STATIC_LOCAL(const AtomicString, auto_reverse, ("auto-reverse"));
  const AtomicString& rotate = getAttribute(svg_names::kRotateAttr);
  if (rotate == auto_val)
    return kRotateAuto;
  if (rotate == auto_reverse)
    return kRotateAutoReverse;
  return kRotateAngle;
}

void SVGAnimateMotionElement::UpdateAnimationPath() {
  animation_path_ = Path();
  SetAlwaysRevalidateAnimationValue(false);

  for (SVGMPathElement* mpath = Traversal<SVGMPathElement>::FirstChild(*this);
       mpath; mpath = Traversal<SVGMPathElement>::NextSibling(*mpath)) {
    if (SVGPathElement* path_element = mpath->PathElement()) {
      // The style of the path referenced by mpath may change independently.
      SetAlwaysRevalidateAnimationValue(true);
      if (path_element->NeedsStyleRecalc()) {
        path_element->GetDocument().UpdateStyleAndLayoutTreeForElement(
            path_element, DocumentUpdateReason::kSMILAnimation);
      }
      animation_path_ = path_element->AsPath();
      return;
    }
  }

  if (FastHasAttribute(svg_names::kPathAttr))
    animation_path_ = path_;
}

SMILAnimationValue SVGAnimateMotionElement::CreateAnimationValue() const {
  DCHECK(targetElement());
  DCHECK(TargetCanHaveMotionTransform(*targetElement()));
  return SMILAnimationValue();
}

void SVGAnimateMotionElement::ClearAnimationValue() {
  SVGElement* target_element = targetElement();
  DCHECK(target_element);
  target_element->ClearAnimatedMotionTransform();
}

wtf_size_t SVGAnimateMotionElement::DiscretePathKeyframeCount() const {
  CHECK(RuntimeEnabledFeatures::SvgAnimateMotionDiscreteCalcModeEnabled());
  return discrete_points_.size();
}

bool SVGAnimateMotionElement::CalculatePathValues() {
  discrete_points_.clear();
  if (RuntimeEnabledFeatures::SvgAnimateMotionDiscreteCalcModeEnabled() &&
      GetCalcMode() == kCalcModeDiscrete && !animation_path_.IsEmpty()) {
    PointCollector collector{discrete_points_, {}};
    animation_path_.Apply([&collector](const PathElement& element) {
      collector.CollectPoint(element);
    });
  }
  return true;
}

void SVGAnimateMotionElement::UpdateKeyframeValues(const Keyframe& keyframe) {
  DCHECK(targetElement());
  if (GetAnimationMode() == kPathAnimation) {
    // Discrete path animation: keyframe index refers to a cached point.
    discrete_path_index_ = keyframe.from_index;
  } else {
    from_point_ = values_[keyframe.from_index];
    to_point_ = values_[keyframe.to_index];
  }
}

bool SVGAnimateMotionElement::CalculateFromAndToValues(
    const String& from_string,
    const String& to_string) {
  base::expected<gfx::PointF, SVGParseStatus> from_point_parse_result =
      ParsePoint(from_string);
  base::expected<gfx::PointF, SVGParseStatus> to_point_parse_result =
      ParsePoint(to_string);

  if ((!from_string.empty() && !from_point_parse_result.has_value()) ||
      !to_point_parse_result.has_value()) {
    return false;
  }

  from_point_ = MakeGarbageCollected<SVGPoint>(
      from_point_parse_result.value_or(gfx::PointF()));
  to_point_ = MakeGarbageCollected<SVGPoint>(to_point_parse_result.value());
  return true;
}

bool SVGAnimateMotionElement::CalculateFromAndByValues(
    const String& from_string,
    const String& by_string) {
  if (!CalculateFromAndToValues(from_string, by_string)) {
    return false;
  }

  // Apply 'from' to 'to' to get 'by' semantics. If the animation mode
  // is 'by', |from_string| will be the empty string and yield a point
  // of (0,0).
  to_point_->SetValue(to_point_->Value() +
                      from_point_->Value().OffsetFromOrigin());
  return true;
}

bool SVGAnimateMotionElement::CalculateValues(const Vector<String>& values) {
  values_.clear();
  for (const auto& value : values) {
    base::expected<gfx::PointF, SVGParseStatus> point_parse_result =
        ParsePoint(value);
    if (!point_parse_result.has_value()) {
      return false;
    }
    values_.push_back(
        MakeGarbageCollected<SVGPoint>(point_parse_result.value()));
  }
  return true;
}

void SVGAnimateMotionElement::CalculateAnimationValue(
    SMILAnimationValue& animation_value,
    float percentage,
    unsigned repeat_count) const {
  SMILAnimationEffectParameters parameters = ComputeEffectParameters();

  PointAndTangent position;
  if (GetAnimationMode() != kPathAnimation) {
    // Values-animation accumulates using the last values entry corresponding to
    // the end of duration time.
    const SVGPoint* to_point_at_end_of_duration_value =
        GetAnimationMode() == kValuesAnimation ? values_.back() : to_point_;
    const gfx::PointF& from_point = from_point_->Value();
    const gfx::PointF& to_point = to_point_->Value();
    const gfx::PointF& to_point_at_end_of_duration =
        To<SVGPoint>(*to_point_at_end_of_duration_value).Value();
    position.point =
        gfx::PointF(ComputeAnimatedNumber(parameters, percentage, repeat_count,
                                          from_point.x(), to_point.x(),
                                          to_point_at_end_of_duration.x()),
                    ComputeAnimatedNumber(parameters, percentage, repeat_count,
                                          from_point.y(), to_point.y(),
                                          to_point_at_end_of_duration.y()));
    position.tangent_in_degrees =
        Rad2deg((to_point - from_point).SlopeAngleRadians());
  } else {
    DCHECK(!animation_path_.IsEmpty());

    const float path_length = animation_path_.length();

    if (RuntimeEnabledFeatures::SvgAnimateMotionDiscreteCalcModeEnabled() &&
        GetCalcMode() == kCalcModeDiscrete && !HasKeyPoints()) {
      // Discrete path animation without keyPoints: the keyframe index was
      // already computed by `ApplyAnimation()` and stored via
      // `UpdateKeyframeValues()`.
      position.point = discrete_points_[discrete_path_index_];
    } else {
      // Non-discrete path animation (linear / paced / spline), or discrete
      // with keyPoints (where percentage is already mapped to a path fraction
      // by `CalculatePercentFromKeyPoints()` in `ApplyAnimation()`).
      const float position_on_path = path_length * percentage;
      position = animation_path_.PointAndNormalAtLength(position_on_path);
    }

    // Handle accumulate="sum" for path animation.
    if (repeat_count && parameters.is_cumulative) {
      gfx::PointF position_at_end_of_duration;
      if (RuntimeEnabledFeatures::SvgAnimateMotionDiscreteCalcModeEnabled() &&
          GetCalcMode() == kCalcModeDiscrete && !HasKeyPoints()) {
        position_at_end_of_duration = discrete_points_.back();
      } else {
        position_at_end_of_duration =
            animation_path_.PointAtLength(path_length);
      }
      position.point +=
          gfx::ScalePoint(position_at_end_of_duration, repeat_count)
              .OffsetFromOrigin();
    }
  }

  AffineTransform& transform = animation_value.motion_transform;

  // If additive, we accumulate into the underlying (transform) value.
  if (!parameters.is_additive) {
    transform.MakeIdentity();
  }

  // Apply position.
  transform.Translate(position.point.x(), position.point.y());

  // Apply rotation.
  switch (GetRotateMode()) {
    case kRotateAuto:
      // Already computed above.
      break;
    case kRotateAutoReverse:
      position.tangent_in_degrees += 180;
      break;
    case kRotateAngle:
      // If rotate=<number> was supported, it would be applied here.
      position.tangent_in_degrees = 0;
      break;
  }
  transform.Rotate(position.tangent_in_degrees);
}

void SVGAnimateMotionElement::ApplyResultsToTarget(
    const SMILAnimationValue& animation_value) {
  SVGElement* target_element = targetElement();
  DCHECK(target_element);
  target_element->SetAnimatedMotionTransform(animation_value.motion_transform);
}

float SVGAnimateMotionElement::CalculateDistance(
    const Keyframe& keyframe) const {
  const SVGPoint& from_point = *values_[keyframe.from_index];
  const SVGPoint& to_point = *values_[keyframe.to_index];
  return (to_point.Value() - from_point.Value()).Length();
}

AnimationMode SVGAnimateMotionElement::CalculateAnimationMode() {
  UpdateAnimationPath();

  if (!animation_path_.IsEmpty()) {
    return kPathAnimation;
  }
  return SVGAnimationElement::CalculateAnimationMode();
}

void SVGAnimateMotionElement::Trace(Visitor* visitor) const {
  visitor->Trace(from_point_);
  visitor->Trace(to_point_);
  visitor->Trace(values_);
  SVGAnimationElement::Trace(visitor);
}

}  // namespace blink
