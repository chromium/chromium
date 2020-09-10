/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_MOTION_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_MOTION_ELEMENT_H_

#include "third_party/blink/renderer/core/svg/svg_animation_element.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

class SVGAnimateMotionElement final : public SVGAnimationElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SVGAnimateMotionElement(Document&);
  ~SVGAnimateMotionElement() override;

  void UpdateAnimationPath();

 private:
  bool HasValidAnimation() const override;
  void WillChangeAnimationTarget() override;
  void DidChangeAnimationTarget() override;

  void ParseAttribute(const AttributeModificationParams&) override;

  void ResetAnimatedType(bool needs_underlying_value) override;
  void ClearAnimatedType() override;
  bool CalculateToAtEndOfDurationValue(
      const String& to_at_end_of_duration_string) override;
  bool CalculateFromAndToValues(const String& from_string,
                                const String& to_string) override;
  bool CalculateFromAndByValues(const String& from_string,
                                const String& by_string) override;
  void CalculateAnimatedValue(float percentage,
                              unsigned repeat_count,
                              SVGSMILElement* result_element) const override;
  void ApplyResultsToTarget() override;
  float CalculateDistance(const String& from_string,
                          const String& to_string) override;

  enum RotateMode { kRotateAngle, kRotateAuto, kRotateAutoReverse };
  RotateMode GetRotateMode() const;

  void UpdateAnimationMode() override;

  void InvalidateForAnimateMotionTransformChange(LayoutObject& target);

  // Note: we do not support percentage values for to/from coords as the spec
  // implies we should (opera doesn't either)
  FloatPoint from_point_;
  FloatPoint to_point_;
  FloatPoint to_point_at_end_of_duration_;

  Path path_;
  Path animation_path_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_MOTION_ELEMENT_H_
