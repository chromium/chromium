// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHAPE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SHAPE_VALUE_H_
#include <initializer_list>

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
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
  enum class Type { kLine };

  // See https://drafts.csswg.org/css-shapes-2/#typedef-shape-command-end-point
  // https://drafts.csswg.org/css-shapes-2/#valdef-shape-to is relative to the
  // reference box, https://drafts.csswg.org/css-shapes-2/#valdef-shape-by is
  // relative to the end of the previous command.
  enum class PointOrigin { kReferenceBox, kPreviousCommand };

  String CSSText() const;
  bool operator==(const CSSShapeCommand& other) const;
  void Trace(Visitor* visitor) const { visitor->Trace(end_point_); }

  explicit CSSShapeCommand(Type type,
                           PointOrigin origin,
                           const CSSValue* end_point)
      : type_(type), origin_(origin), end_point_(end_point) {}

 private:
  Type type_;
  PointOrigin origin_;
  Member<const CSSValue> end_point_;
};

class CSSShapeValue : public CSSValue {
 public:
  CSSShapeValue(WindRule wind_rule,
                const CSSValuePair* origin,
                HeapVector<Member<const CSSShapeCommand>> commands)
      : CSSValue(kShapeClass),
        wind_rule_(wind_rule),
        origin_(origin),
        commands_(std::move(commands)) {}
  String CustomCSSText() const;

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
