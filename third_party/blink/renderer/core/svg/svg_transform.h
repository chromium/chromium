/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TRANSFORM_H_

#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FloatSize;
class SVGTransformTearOff;

enum class SVGTransformType {
  kUnknown = 0,
  kMatrix = 1,
  kTranslate = 2,
  kScale = 3,
  kRotate = 4,
  kSkewx = 5,
  kSkewy = 6
};

class SVGTransform final : public SVGPropertyBase {
 public:
  typedef SVGTransformTearOff TearOffType;

  enum ConstructionMode {
    kConstructIdentityTransform,
    kConstructZeroTransform
  };

  SVGTransform();
  explicit SVGTransform(SVGTransformType,
                        ConstructionMode = kConstructIdentityTransform);
  explicit SVGTransform(const AffineTransform&);
  SVGTransform(SVGTransformType,
               float,
               const FloatPoint&,
               const AffineTransform&);
  ~SVGTransform() override;

  SVGTransform* Clone() const;
  SVGPropertyBase* CloneForAnimation(const String&) const override;

  SVGTransformType TransformType() const { return transform_type_; }

  const AffineTransform& Matrix() const { return matrix_; }

  // |onMatrixChange| must be called after modifications via |mutableMatrix|.
  AffineTransform* MutableMatrix() { return &matrix_; }
  void OnMatrixChange();

  float Angle() const { return angle_; }
  FloatPoint RotationCenter() const { return center_; }

  void SetMatrix(const AffineTransform&);
  void SetTranslate(float tx, float ty);
  void SetScale(float sx, float sy);
  void SetRotate(float angle, float cx, float cy);
  void SetSkewX(float angle);
  void SetSkewY(float angle);

  // Internal use only (animation system)
  FloatPoint Translate() const;
  FloatSize Scale() const;

  String ValueAsString() const override;

  void Add(SVGPropertyBase*, SVGElement*) override;
  void CalculateAnimatedValue(const SVGAnimateElement&,
                              float percentage,
                              unsigned repeat_count,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* to_at_end_of_duration_value,
                              SVGElement* context_element) override;
  float CalculateDistance(SVGPropertyBase* to,
                          SVGElement* context_element) override;

  static AnimatedPropertyType ClassType() { return kAnimatedTransform; }
  AnimatedPropertyType GetType() const override { return ClassType(); }

 private:
  SVGTransformType transform_type_;
  float angle_;
  FloatPoint center_;
  AffineTransform matrix_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TRANSFORM_H_
