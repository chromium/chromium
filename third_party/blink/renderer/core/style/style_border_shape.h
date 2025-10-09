// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BORDER_SHAPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BORDER_SHAPE_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// https://drafts.csswg.org/css-borders-4/#border-shape
class StyleBorderShape : public GarbageCollected<StyleBorderShape> {
 public:
  // A border shape always has an inner and outer shape, though in case they are
  // identical certain operations such as filling between them can be skipped.
  explicit StyleBorderShape(BasicShape& outer,
                            BasicShape* inner = nullptr,
                            CoordBox outer_coord_box = CoordBox::kBorderBox,
                            CoordBox inner_coord_box = CoordBox::kBorderBox)
      : outer_(&outer),
        inner_(inner ? inner : &outer),
        outer_coord_box_(outer_coord_box),
        inner_coord_box_(inner_coord_box) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(outer_);
    visitor->Trace(inner_);
  }

  bool HasSeparateInnerShape() const {
    return !base::ValuesEquivalent(inner_, outer_);
  }

  const BasicShape& OuterShape() const { return *outer_; }
  const BasicShape& InnerShape() const { return *inner_; }

  CoordBox OuterCoordBox() const { return outer_coord_box_; }
  CoordBox InnerCoordBox() const { return inner_coord_box_; }

  bool operator==(const StyleBorderShape& o) const {
    return base::ValuesEquivalent(outer_, o.outer_) &&
           base::ValuesEquivalent(inner_, o.inner_) &&
           outer_coord_box_ == o.outer_coord_box_ &&
           inner_coord_box_ == o.inner_coord_box_;
  }

 private:
  Member<BasicShape> outer_;
  Member<BasicShape> inner_;
  CoordBox outer_coord_box_;
  CoordBox inner_coord_box_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BORDER_SHAPE_H_
