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
#include "third_party/blink/renderer/core/svg/svg_polygon_element.h"
#include "third_party/blink/renderer/core/svg/svg_polyline_element.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/core/svg/svg_switch_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
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
    path_ = Path();
    BuildPathFromString(params.new_value, path_);
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

  for (SVGMPathElement* mpath = Traversal<SVGMPathElement>::FirstChild(*this);
       mpath; mpath = Traversal<SVGMPathElement>::NextSibling(*mpath)) {
    if (SVGPathElement* path_element = mpath->PathElement()) {
      animation_path_ = path_element->AttributePath();
      return;
    }
  }

  if (FastHasAttribute(svg_names::kPathAttr))
    animation_path_ = path_;
}

template <typename CharType>
static bool ParsePointInternal(const CharType* ptr,
                               const CharType* end,
                               gfx::PointF& point) {
  if (!SkipOptionalSVGSpaces(ptr, end))
    return false;

  float x = 0;
  if (!ParseNumber(ptr, end, x))
    return false;

  float y = 0;
  if (!ParseNumber(ptr, end, y))
    return false;

  point = gfx::PointF(x, y);

  // disallow anything except spaces at the end
  return !SkipOptionalSVGSpaces(ptr, end);
}

static bool ParsePoint(const String& string, gfx::PointF& point) {
  if (string.empty())
    return false;
  return WTF::VisitCharacters(string, [&](auto chars) {
    return ParsePointInternal(chars.data(), chars.data() + chars.size(), point);
  });
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

bool SVGAnimateMotionElement::CalculateToAtEndOfDurationValue(
    const String& to_at_end_of_duration_string) {
  ParsePoint(to_at_end_of_duration_string, to_point_at_end_of_duration_);
  return true;
}

void SVGAnimateMotionElement::CalculateFromAndToValues(
    const String& from_string,
    const String& to_string) {
  ParsePoint(from_string, from_point_);
  ParsePoint(to_string, to_point_);
  // TODO(fs): Looks like this would clobber the at-end-of-duration
  // value for a cumulative 'values' animation.
  to_point_at_end_of_duration_ = to_point_;
}

void SVGAnimateMotionElement::CalculateFromAndByValues(
    const String& from_string,
    const String& by_string) {
  CalculateFromAndToValues(from_string, by_string);
  // Apply 'from' to 'to' to get 'by' semantics. If the animation mode
  // is 'by', |from_string| will be the empty string and yield a point
  // of (0,0).
  to_point_ += from_point_.OffsetFromOrigin();
  to_point_at_end_of_duration_ = to_point_;
}

void SVGAnimateMotionElement::CalculateAnimationValue(
    SMILAnimationValue& animation_value,
    float percentage,
    unsigned repeat_count) const {
  SMILAnimationEffectParameters parameters = ComputeEffectParameters();

  PointAndTangent position;
  if (GetAnimationMode() != kPathAnimation) {
    position.point =
        gfx::PointF(ComputeAnimatedNumber(parameters, percentage, repeat_count,
                                          from_point_.x(), to_point_.x(),
                                          to_point_at_end_of_duration_.x()),
                    ComputeAnimatedNumber(parameters, percentage, repeat_count,
                                          from_point_.y(), to_point_.y(),
                                          to_point_at_end_of_duration_.y()));
    position.tangent_in_degrees =
        Rad2deg((to_point_ - from_point_).SlopeAngleRadians());
  } else {
    DCHECK(!animation_path_.IsEmpty());

    const float path_length = animation_path_.length();
    const float position_on_path = path_length * percentage;
    position = animation_path_.PointAndNormalAtLength(position_on_path);

    // Handle accumulate="sum".
    if (repeat_count && parameters.is_cumulative) {
      const gfx::PointF position_at_end_of_duration =
          animation_path_.PointAtLength(path_length);
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

float SVGAnimateMotionElement::CalculateDistance(const String& from_string,
                                                 const String& to_string) {
  gfx::PointF from;
  gfx::PointF to;
  if (!ParsePoint(from_string, from))
    return -1;
  if (!ParsePoint(to_string, to))
    return -1;
  return (to - from).Length();
}

AnimationMode SVGAnimateMotionElement::CalculateAnimationMode() {
  UpdateAnimationPath();

  if (!animation_path_.IsEmpty()) {
    return kPathAnimation;
  }
  return SVGAnimationElement::CalculateAnimationMode();
}

}  // namespace blink
