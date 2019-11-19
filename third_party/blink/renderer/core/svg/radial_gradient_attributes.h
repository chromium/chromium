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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_RADIAL_GRADIENT_ATTRIBUTES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_RADIAL_GRADIENT_ATTRIBUTES_H_

#include "third_party/blink/renderer/core/svg/gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {
struct RadialGradientAttributes final : GradientAttributes {
  DISALLOW_NEW();

 public:
  RadialGradientAttributes()
      : cx_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        cy_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        r_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kOther)),
        fx_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kWidth)),
        fy_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kHeight)),
        fr_(MakeGarbageCollected<SVGLength>(SVGLengthMode::kOther)),
        cx_set_(false),
        cy_set_(false),
        r_set_(false),
        fx_set_(false),
        fy_set_(false),
        fr_set_(false) {
    cx_->SetValueAsString("50%");
    cy_->SetValueAsString("50%");
    r_->SetValueAsString("50%");
  }

  SVGLength* Cx() const { return cx_.Get(); }
  SVGLength* Cy() const { return cy_.Get(); }
  SVGLength* R() const { return r_.Get(); }
  SVGLength* Fx() const { return fx_.Get(); }
  SVGLength* Fy() const { return fy_.Get(); }
  SVGLength* Fr() const { return fr_.Get(); }

  void SetCx(SVGLength* value) {
    cx_ = value;
    cx_set_ = true;
  }
  void SetCy(SVGLength* value) {
    cy_ = value;
    cy_set_ = true;
  }
  void SetR(SVGLength* value) {
    r_ = value;
    r_set_ = true;
  }
  void SetFx(SVGLength* value) {
    fx_ = value;
    fx_set_ = true;
  }
  void SetFy(SVGLength* value) {
    fy_ = value;
    fy_set_ = true;
  }
  void SetFr(SVGLength* value) {
    fr_ = value;
    fr_set_ = true;
  }

  bool HasCx() const { return cx_set_; }
  bool HasCy() const { return cy_set_; }
  bool HasR() const { return r_set_; }
  bool HasFx() const { return fx_set_; }
  bool HasFy() const { return fy_set_; }
  bool HasFr() const { return fr_set_; }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(cx_);
    visitor->Trace(cy_);
    visitor->Trace(r_);
    visitor->Trace(fx_);
    visitor->Trace(fy_);
    visitor->Trace(fr_);
  }

 private:
  // Properties
  Member<SVGLength> cx_;
  Member<SVGLength> cy_;
  Member<SVGLength> r_;
  Member<SVGLength> fx_;
  Member<SVGLength> fy_;
  Member<SVGLength> fr_;

  // Property states
  bool cx_set_ : 1;
  bool cy_set_ : 1;
  bool r_set_ : 1;
  bool fx_set_ : 1;
  bool fy_set_ : 1;
  bool fr_set_ : 1;
};

// Wrapper object for the RadialGradientAttributes part object.
class RadialGradientAttributesWrapper final
    : public GarbageCollected<RadialGradientAttributesWrapper> {
 public:
  RadialGradientAttributesWrapper() = default;

  RadialGradientAttributes& Attributes() { return attributes_; }
  void Set(const RadialGradientAttributes& attributes) {
    attributes_ = attributes;
  }
  void Trace(blink::Visitor* visitor) { visitor->Trace(attributes_); }

 private:
  RadialGradientAttributes attributes_;
};

}  // namespace blink

#endif
