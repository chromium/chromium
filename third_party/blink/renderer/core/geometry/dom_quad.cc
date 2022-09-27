// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_quad.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/geometry_util.h"

namespace blink {
namespace {

class DOMQuadPoint final : public DOMPoint {
 public:
  static DOMQuadPoint* Create(double x,
                              double y,
                              double z,
                              double w,
                              DOMQuad* quad) {
    return MakeGarbageCollected<DOMQuadPoint>(x, y, z, w, quad);
  }

  static DOMQuadPoint* FromPoint(const DOMPointInit* other, DOMQuad* quad) {
    return MakeGarbageCollected<DOMQuadPoint>(other->x(), other->y(),
                                              other->z(), other->w(), quad);
  }

  DOMQuadPoint(double x, double y, double z, double w, DOMQuad* quad)
      : DOMPoint(x, y, z, w), quad_(quad) {}

  void setX(double x) override {
    DOMPoint::setX(x);
    if (quad_)
      quad_->set_needs_bounds_calculation(true);
  }

  void setY(double y) override {
    DOMPoint::setY(y);
    if (quad_)
      quad_->set_needs_bounds_calculation(true);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(quad_);
    DOMPoint::Trace(visitor);
  }

 private:
  WeakMember<DOMQuad> quad_;
};

double NanSafeMin4(double a, double b, double c, double d) {
  using geometry_util::NanSafeMin;
  return NanSafeMin(NanSafeMin(a, b), NanSafeMin(c, d));
}

double NanSafeMax4(double a, double b, double c, double d) {
  using geometry_util::NanSafeMax;
  return NanSafeMax(NanSafeMax(a, b), NanSafeMax(c, d));
}

}  // namespace

DOMQuad* DOMQuad::Create(const DOMPointInit* p1,
                         const DOMPointInit* p2,
                         const DOMPointInit* p3,
                         const DOMPointInit* p4) {
  return MakeGarbageCollected<DOMQuad>(p1, p2, p3, p4);
}

DOMQuad* DOMQuad::fromRect(const DOMRectInit* other) {
  return MakeGarbageCollected<DOMQuad>(other->x(), other->y(), other->width(),
                                       other->height());
}

DOMQuad* DOMQuad::fromQuad(const DOMQuadInit* other) {
  return MakeGarbageCollected<DOMQuad>(
      other->hasP1() ? other->p1() : DOMPointInit::Create(),
      other->hasP2() ? other->p2() : DOMPointInit::Create(),
      other->hasP3() ? other->p3() : DOMPointInit::Create(),
      other->hasP4() ? other->p4() : DOMPointInit::Create());
}

DOMRect* DOMQuad::getBounds() {
  if (needs_bounds_calculation_)
    CalculateBounds();
  return DOMRect::Create(x_, y_, width_, height_);
}

void DOMQuad::CalculateBounds() {
  x_ = NanSafeMin4(p1()->x(), p2()->x(), p3()->x(), p4()->x());
  y_ = NanSafeMin4(p1()->y(), p2()->y(), p3()->y(), p4()->y());
  width_ = NanSafeMax4(p1()->x(), p2()->x(), p3()->x(), p4()->x()) - x_;
  height_ = NanSafeMax4(p1()->y(), p2()->y(), p3()->y(), p4()->y()) - y_;
  needs_bounds_calculation_ = false;
}

DOMQuad::DOMQuad(const DOMPointInit* p1,
                 const DOMPointInit* p2,
                 const DOMPointInit* p3,
                 const DOMPointInit* p4)
    : p1_(DOMQuadPoint::FromPoint(p1, this)),
      p2_(DOMQuadPoint::FromPoint(p2, this)),
      p3_(DOMQuadPoint::FromPoint(p3, this)),
      p4_(DOMQuadPoint::FromPoint(p4, this)),
      needs_bounds_calculation_(true) {}

DOMQuad::DOMQuad(double x, double y, double width, double height)
    : p1_(DOMQuadPoint::Create(x, y, 0, 1, this)),
      p2_(DOMQuadPoint::Create(x + width, y, 0, 1, this)),
      p3_(DOMQuadPoint::Create(x + width, y + height, 0, 1, this)),
      p4_(DOMQuadPoint::Create(x, y + height, 0, 1, this)),
      needs_bounds_calculation_(true) {}

ScriptValue DOMQuad::toJSONForBinding(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.Add("p1", p1());
  result.Add("p2", p2());
  result.Add("p3", p3());
  result.Add("p4", p4());
  return result.GetScriptValue();
}

}  // namespace blink
