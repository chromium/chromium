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
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_mpath_element.h"
#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

bool TargetCanHaveMotionTransform(const SVGElement& target) {
  // We don't have a special attribute name to verify the animation type. Check
  // the element name instead.
  if (!target.IsSVGGraphicsElement())
    return false;
  // Spec: SVG 1.1 section 19.2.15
  // FIXME: svgTag is missing. Needs to be checked, if transforming <svg> could
  // cause problems.
  return IsSVGGElement(target) || IsSVGDefsElement(target) ||
         IsSVGUseElement(target) || IsSVGImageElement(target) ||
         IsSVGSwitchElement(target) || IsSVGPathElement(target) ||
         IsSVGRectElement(target) || IsSVGCircleElement(target) ||
         IsSVGEllipseElement(target) || IsSVGLineElement(target) ||
         IsSVGPolylineElement(target) || IsSVGPolygonElement(target) ||
         IsSVGTextElement(target) || IsSVGClipPathElement(target) ||
         IsSVGMaskElement(target) || IsSVGAElement(target) ||
         IsSVGForeignObjectElement(target);
}
}

inline SVGAnimateMotionElement::SVGAnimateMotionElement(Document& document)
    : SVGAnimationElement(svg_names::kAnimateMotionTag, document),
      has_to_point_at_end_of_duration_(false) {
  SetCalcMode(kCalcModePaced);
}

DEFINE_NODE_FACTORY(SVGAnimateMotionElement)

SVGAnimateMotionElement::~SVGAnimateMotionElement() = default;

bool SVGAnimateMotionElement::HasValidTarget() {
  return SVGAnimationElement::HasValidTarget() &&
         TargetCanHaveMotionTransform(*targetElement());
}

void SVGAnimateMotionElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kPathAttr) {
    path_ = Path();
    BuildPathFromString(params.new_value, path_);
    UpdateAnimationPath();
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
  bool found_m_path = false;

  for (SVGMPathElement* mpath = Traversal<SVGMPathElement>::FirstChild(*this);
       mpath; mpath = Traversal<SVGMPathElement>::NextSibling(*mpath)) {
    if (SVGPathElement* path_element = mpath->PathElement()) {
      animation_path_ = path_element->AttributePath();
      found_m_path = true;
      break;
    }
  }

  if (!found_m_path && FastHasAttribute(svg_names::kPathAttr))
    animation_path_ = path_;

  UpdateAnimationMode();
}

template <typename CharType>
static bool ParsePointInternal(const String& string, FloatPoint& point) {
  const CharType* ptr = string.GetCharacters<CharType>();
  const CharType* end = ptr + string.length();

  if (!SkipOptionalSVGSpaces(ptr, end))
    return false;

  float x = 0;
  if (!ParseNumber(ptr, end, x))
    return false;

  float y = 0;
  if (!ParseNumber(ptr, end, y))
    return false;

  point = FloatPoint(x, y);

  // disallow anything except spaces at the end
  return !SkipOptionalSVGSpaces(ptr, end);
}

static bool ParsePoint(const String& string, FloatPoint& point) {
  if (string.IsEmpty())
    return false;
  if (string.Is8Bit())
    return ParsePointInternal<LChar>(string, point);
  return ParsePointInternal<UChar>(string, point);
}

void SVGAnimateMotionElement::ResetAnimatedType() {
  SVGElement* target_element = this->targetElement();
  if (!target_element || !TargetCanHaveMotionTransform(*target_element))
    return;
  if (AffineTransform* transform = target_element->AnimateMotionTransform())
    transform->MakeIdentity();
}

void SVGAnimateMotionElement::ClearAnimatedType() {
  SVGElement* target_element = this->targetElement();
  if (!target_element)
    return;

  AffineTransform* transform = target_element->AnimateMotionTransform();
  if (!transform)
    return;

  transform->MakeIdentity();

  if (LayoutObject* target_layout_object = target_element->GetLayoutObject())
    InvalidateForAnimateMotionTransformChange(*target_layout_object);
}

bool SVGAnimateMotionElement::CalculateToAtEndOfDurationValue(
    const String& to_at_end_of_duration_string) {
  ParsePoint(to_at_end_of_duration_string, to_point_at_end_of_duration_);
  has_to_point_at_end_of_duration_ = true;
  return true;
}

bool SVGAnimateMotionElement::CalculateFromAndToValues(
    const String& from_string,
    const String& to_string) {
  has_to_point_at_end_of_duration_ = false;
  ParsePoint(from_string, from_point_);
  ParsePoint(to_string, to_point_);
  return true;
}

bool SVGAnimateMotionElement::CalculateFromAndByValues(
    const String& from_string,
    const String& by_string) {
  has_to_point_at_end_of_duration_ = false;
  if (GetAnimationMode() == kByAnimation && !IsAdditive())
    return false;
  ParsePoint(from_string, from_point_);
  FloatPoint by_point;
  ParsePoint(by_string, by_point);
  to_point_ = FloatPoint(from_point_.X() + by_point.X(),
                         from_point_.Y() + by_point.Y());
  return true;
}

void SVGAnimateMotionElement::CalculateAnimatedValue(float percentage,
                                                     unsigned repeat_count,
                                                     SVGSMILElement*) {
  SVGElement* target_element = this->targetElement();
  DCHECK(target_element);
  AffineTransform* transform = target_element->AnimateMotionTransform();
  if (!transform)
    return;

  if (LayoutObject* target_layout_object = target_element->GetLayoutObject())
    InvalidateForAnimateMotionTransformChange(*target_layout_object);

  if (!IsAdditive())
    transform->MakeIdentity();

  if (GetAnimationMode() != kPathAnimation) {
    FloatPoint to_point_at_end_of_duration = to_point_;
    if (IsAccumulated() && repeat_count && has_to_point_at_end_of_duration_)
      to_point_at_end_of_duration = to_point_at_end_of_duration_;

    float animated_x = 0;
    AnimateAdditiveNumber(percentage, repeat_count, from_point_.X(),
                          to_point_.X(), to_point_at_end_of_duration.X(),
                          animated_x);

    float animated_y = 0;
    AnimateAdditiveNumber(percentage, repeat_count, from_point_.Y(),
                          to_point_.Y(), to_point_at_end_of_duration.Y(),
                          animated_y);

    transform->Translate(animated_x, animated_y);
    return;
  }

  DCHECK(!animation_path_.IsEmpty());

  float position_on_path = animation_path_.length() * percentage;
  FloatPoint position;
  float angle;
  animation_path_.PointAndNormalAtLength(position_on_path, position, angle);

  // Handle accumulate="sum".
  if (IsAccumulated() && repeat_count) {
    FloatPoint position_at_end_of_duration =
        animation_path_.PointAtLength(animation_path_.length());
    position.Move(position_at_end_of_duration.X() * repeat_count,
                  position_at_end_of_duration.Y() * repeat_count);
  }

  transform->Translate(position.X(), position.Y());
  RotateMode rotate_mode = this->GetRotateMode();
  if (rotate_mode != kRotateAuto && rotate_mode != kRotateAutoReverse)
    return;
  if (rotate_mode == kRotateAutoReverse)
    angle += 180;
  transform->Rotate(angle);
}

void SVGAnimateMotionElement::ApplyResultsToTarget() {
  // We accumulate to the target element transform list so there is not much to
  // do here.
  SVGElement* target_element = this->targetElement();
  if (!target_element)
    return;

  AffineTransform* t = target_element->AnimateMotionTransform();
  if (!t)
    return;

  // ...except in case where we have additional instances in <use> trees.
  const HeapHashSet<WeakMember<SVGElement>>& instances =
      target_element->InstancesForElement();
  for (SVGElement* shadow_tree_element : instances) {
    DCHECK(shadow_tree_element);
    AffineTransform* transform = shadow_tree_element->AnimateMotionTransform();
    if (!transform)
      continue;
    transform->SetMatrix(t->A(), t->B(), t->C(), t->D(), t->E(), t->F());
    if (LayoutObject* layout_object = shadow_tree_element->GetLayoutObject())
      InvalidateForAnimateMotionTransformChange(*layout_object);
  }
}

float SVGAnimateMotionElement::CalculateDistance(const String& from_string,
                                                 const String& to_string) {
  FloatPoint from;
  FloatPoint to;
  if (!ParsePoint(from_string, from))
    return -1;
  if (!ParsePoint(to_string, to))
    return -1;
  FloatSize diff = to - from;
  return sqrtf(diff.Width() * diff.Width() + diff.Height() * diff.Height());
}

void SVGAnimateMotionElement::UpdateAnimationMode() {
  if (!animation_path_.IsEmpty())
    SetAnimationMode(kPathAnimation);
  else
    SVGAnimationElement::UpdateAnimationMode();
}

void SVGAnimateMotionElement::InvalidateForAnimateMotionTransformChange(
    LayoutObject& object) {
  object.SetNeedsTransformUpdate();
  // The transform paint property relies on the SVG transform value.
  object.SetNeedsPaintPropertyUpdate();
  MarkForLayoutAndParentResourceInvalidation(object);
}

}  // namespace blink
