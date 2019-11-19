/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PATTERN_ATTRIBUTES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PATTERN_ATTRIBUTES_H_

#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class SVGPatternElement;

class PatternAttributes final {
  DISALLOW_NEW();

 public:
  PatternAttributes()
      : x_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        y_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        width_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        height_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        view_box_(),
        preserve_aspect_ratio_(MakeGarbageCollected<SVGPreserveAspectRatio>()),
        pattern_units_(SVGUnitTypes::kSvgUnitTypeObjectboundingbox),
        pattern_content_units_(SVGUnitTypes::kSvgUnitTypeUserspaceonuse),
        pattern_content_element_(nullptr),
        x_set_(false),
        y_set_(false),
        width_set_(false),
        height_set_(false),
        view_box_set_(false),
        preserve_aspect_ratio_set_(false),
        pattern_units_set_(false),
        pattern_content_units_set_(false),
        pattern_transform_set_(false),
        pattern_content_element_set_(false) {}

  SVGLength* X() const { return x_.Get(); }
  SVGLength* Y() const { return y_.Get(); }
  SVGLength* Width() const { return width_.Get(); }
  SVGLength* Height() const { return height_.Get(); }
  FloatRect ViewBox() const { return view_box_; }
  SVGPreserveAspectRatio* PreserveAspectRatio() const {
    return preserve_aspect_ratio_.Get();
  }
  SVGUnitTypes::SVGUnitType PatternUnits() const { return pattern_units_; }
  SVGUnitTypes::SVGUnitType PatternContentUnits() const {
    return pattern_content_units_;
  }
  AffineTransform PatternTransform() const { return pattern_transform_; }
  const SVGPatternElement* PatternContentElement() const {
    return pattern_content_element_;
  }

  void SetX(SVGLength* value) {
    x_ = value;
    x_set_ = true;
  }

  void SetY(SVGLength* value) {
    y_ = value;
    y_set_ = true;
  }

  void SetWidth(SVGLength* value) {
    width_ = value;
    width_set_ = true;
  }

  void SetHeight(SVGLength* value) {
    height_ = value;
    height_set_ = true;
  }

  void SetViewBox(const FloatRect& value) {
    view_box_ = value;
    view_box_set_ = true;
  }

  void SetPreserveAspectRatio(SVGPreserveAspectRatio* value) {
    preserve_aspect_ratio_ = value;
    preserve_aspect_ratio_set_ = true;
  }

  void SetPatternUnits(SVGUnitTypes::SVGUnitType value) {
    pattern_units_ = value;
    pattern_units_set_ = true;
  }

  void SetPatternContentUnits(SVGUnitTypes::SVGUnitType value) {
    pattern_content_units_ = value;
    pattern_content_units_set_ = true;
  }

  void SetPatternTransform(const AffineTransform& value) {
    pattern_transform_ = value;
    pattern_transform_set_ = true;
  }

  void SetPatternContentElement(const SVGPatternElement* value) {
    pattern_content_element_ = value;
    pattern_content_element_set_ = true;
  }

  bool HasX() const { return x_set_; }
  bool HasY() const { return y_set_; }
  bool HasWidth() const { return width_set_; }
  bool HasHeight() const { return height_set_; }
  bool HasViewBox() const { return view_box_set_; }
  bool HasPreserveAspectRatio() const { return preserve_aspect_ratio_set_; }
  bool HasPatternUnits() const { return pattern_units_set_; }
  bool HasPatternContentUnits() const { return pattern_content_units_set_; }
  bool HasPatternTransform() const { return pattern_transform_set_; }
  bool HasPatternContentElement() const { return pattern_content_element_set_; }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(width_);
    visitor->Trace(height_);
    visitor->Trace(preserve_aspect_ratio_);
    visitor->Trace(pattern_content_element_);
  }

 private:
  // Properties
  Member<SVGLength> x_;
  Member<SVGLength> y_;
  Member<SVGLength> width_;
  Member<SVGLength> height_;
  FloatRect view_box_;
  Member<SVGPreserveAspectRatio> preserve_aspect_ratio_;
  SVGUnitTypes::SVGUnitType pattern_units_;
  SVGUnitTypes::SVGUnitType pattern_content_units_;
  AffineTransform pattern_transform_;
  Member<const SVGPatternElement> pattern_content_element_;

  // Property states
  bool x_set_ : 1;
  bool y_set_ : 1;
  bool width_set_ : 1;
  bool height_set_ : 1;
  bool view_box_set_ : 1;
  bool preserve_aspect_ratio_set_ : 1;
  bool pattern_units_set_ : 1;
  bool pattern_content_units_set_ : 1;
  bool pattern_transform_set_ : 1;
  bool pattern_content_element_set_ : 1;
};

// Wrapper object for the PatternAttributes part object.
class PatternAttributesWrapper
    : public GarbageCollected<PatternAttributesWrapper> {
 public:
  PatternAttributesWrapper() = default;

  PatternAttributes& Attributes() { return attributes_; }
  void Set(const PatternAttributes& attributes) { attributes_ = attributes; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(attributes_); }

 private:
  PatternAttributes attributes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_PATTERN_ATTRIBUTES_H_
