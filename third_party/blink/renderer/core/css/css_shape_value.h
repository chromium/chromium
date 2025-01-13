// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHAPE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHAPE_VALUE_H_
#include <initializer_list>

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// https://drafts.csswg.org/css-shapes-2/#typedef-shape-command
// The sequence of <shape-command>s represent further path data commands.
// Each command’s starting point is the previous command’s ending point.
class CSSShapeCommand : public GarbageCollected<CSSShapeCommand> {
 public:
  CSSValueID GetType() const { return type_; }
  CSSValueID GetEndPointOrigin() const { return end_point_origin_; }
  const CSSValue& GetEndPoint() const { return *end_point_; }

  String CSSText() const;
  bool operator==(const CSSShapeCommand& other) const;
  virtual void Trace(Visitor* visitor) const { visitor->Trace(end_point_); }

  CSSShapeCommand(CSSValueID type, CSSValueID origin, const CSSValue& end_point)
      : type_(type), end_point_origin_(origin), end_point_(end_point) {
    CHECK(type == CSSValueID::kMove || type == CSSValueID::kLine ||
          type == CSSValueID::kHline || type == CSSValueID::kVline ||
          type == CSSValueID::kArc);
    CHECK(origin == CSSValueID::kTo || origin == CSSValueID::kBy);
  }

  static const CSSShapeCommand* Close() {
    return MakeGarbageCollected<CSSShapeCommand>();
  }

  // This should be private, but can't because of MakeGarbageCollected.
  CSSShapeCommand() : type_(CSSValueID::kClose) {}

 private:
  // Either kMove or kLine.
  CSSValueID type_;

  // Either kBy or kTo.
  // See https://drafts.csswg.org/css-shapes-2/#typedef-shape-command-end-point
  // https://drafts.csswg.org/css-shapes-2/#valdef-shape-to is relative to the
  // reference box, https://drafts.csswg.org/css-shapes-2/#valdef-shape-by is
  // relative to the end of the previous command.
  CSSValueID end_point_origin_;
  Member<const CSSValue> end_point_;
};

class CSSShapeArcCommand : public CSSShapeCommand {
 public:
  CSSShapeArcCommand(CSSValueID origin,
                     const CSSValue& end_point,
                     const CSSPrimitiveValue& angle,
                     const CSSValuePair& radius,
                     CSSValueID size,
                     CSSValueID sweep)
      : CSSShapeCommand(CSSValueID::kArc, origin, end_point),
        angle_(angle),
        radius_(radius),
        size_(size),
        sweep_(sweep) {
    CHECK(sweep == CSSValueID::kCw || sweep == CSSValueID::kCcw);
    CHECK(size == CSSValueID::kLarge || size == CSSValueID::kSmall);
  }
  const CSSPrimitiveValue& Angle() const { return *angle_; }
  const CSSValuePair& Radius() const { return *radius_; }
  CSSValueID Size() const { return size_; }
  CSSValueID Sweep() const { return sweep_; }
  bool operator==(const CSSShapeArcCommand& other) const {
    return CSSShapeCommand::operator==(other) && sweep_ == other.sweep_ &&
           size_ == other.size_ && radius_ == other.radius_ &&
           angle_ == other.angle_;
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(angle_);
    visitor->Trace(radius_);
    CSSShapeCommand::Trace(visitor);
  }

 private:
  Member<const CSSPrimitiveValue> angle_;
  Member<const CSSValuePair> radius_;
  CSSValueID size_;
  CSSValueID sweep_;
};

class CSSShapeValue : public CSSValue {
 public:
  CSSShapeValue(WindRule wind_rule,
                const CSSValuePair& origin,
                HeapVector<Member<const CSSShapeCommand>> commands)
      : CSSValue(kShapeClass),
        wind_rule_(wind_rule),
        origin_(origin),
        commands_(std::move(commands)) {}
  String CustomCSSText() const;

  WindRule GetWindRule() const { return wind_rule_; }
  const CSSValuePair& GetOrigin() const { return *origin_; }
  const HeapVector<Member<const CSSShapeCommand>>& Commands() const {
    return commands_;
  }

  bool Equals(const CSSShapeValue& other) const {
    return wind_rule_ == other.wind_rule_ && *origin_ == *other.origin_ &&
           commands_ == other.commands_;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  WindRule wind_rule_;
  Member<const CSSValuePair> origin_;
  HeapVector<Member<const CSSShapeCommand>> commands_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSShapeValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsShapeValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHAPE_VALUE_H_
