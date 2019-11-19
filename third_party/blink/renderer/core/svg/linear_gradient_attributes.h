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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_LINEAR_GRADIENT_ATTRIBUTES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_LINEAR_GRADIENT_ATTRIBUTES_H_

#include "third_party/blink/renderer/core/svg/gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

struct LinearGradientAttributes : GradientAttributes {
  DISALLOW_NEW();

 public:
  LinearGradientAttributes()
      : x1_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        y1_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        x2_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        y2_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        x1_set_(false),
        y1_set_(false),
        x2_set_(false),
        y2_set_(false) {
    x2_->SetValueAsString("100%");
  }

  SVGLength* X1() const { return x1_.Get(); }
  SVGLength* Y1() const { return y1_.Get(); }
  SVGLength* X2() const { return x2_.Get(); }
  SVGLength* Y2() const { return y2_.Get(); }

  void SetX1(SVGLength* value) {
    x1_ = value;
    x1_set_ = true;
  }
  void SetY1(SVGLength* value) {
    y1_ = value;
    y1_set_ = true;
  }
  void SetX2(SVGLength* value) {
    x2_ = value;
    x2_set_ = true;
  }
  void SetY2(SVGLength* value) {
    y2_ = value;
    y2_set_ = true;
  }

  bool HasX1() const { return x1_set_; }
  bool HasY1() const { return y1_set_; }
  bool HasX2() const { return x2_set_; }
  bool HasY2() const { return y2_set_; }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(x1_);
    visitor->Trace(y1_);
    visitor->Trace(x2_);
    visitor->Trace(y2_);
  }

 private:
  // Properties
  Member<SVGLength> x1_;
  Member<SVGLength> y1_;
  Member<SVGLength> x2_;
  Member<SVGLength> y2_;

  // Property states
  bool x1_set_ : 1;
  bool y1_set_ : 1;
  bool x2_set_ : 1;
  bool y2_set_ : 1;
};

// Wrapper object for the LinearGradientAttributes part object.
class LinearGradientAttributesWrapper final
    : public GarbageCollected<LinearGradientAttributesWrapper> {
 public:
  LinearGradientAttributesWrapper() = default;

  LinearGradientAttributes& Attributes() { return attributes_; }
  void Set(const LinearGradientAttributes& attributes) {
    attributes_ = attributes;
  }
  void Trace(blink::Visitor* visitor) { visitor->Trace(attributes_); }

 private:
  LinearGradientAttributes attributes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_LINEAR_GRADIENT_ATTRIBUTES_H_
