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
                            GeometryBox outer_box = GeometryBox::kBorderBox,
                            GeometryBox inner_box = GeometryBox::kBorderBox)
      : outer_(&outer),
        inner_(inner ? inner : &outer),
        outer_box_(outer_box),
        inner_box_(inner_box) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(outer_);
    visitor->Trace(inner_);
  }

  bool HasSeparateInnerShape() const {
    return !base::ValuesEquivalent(inner_, outer_) || inner_box_ != outer_box_;
  }

  const BasicShape& OuterShape() const { return *outer_; }
  const BasicShape& InnerShape() const { return *inner_; }

  GeometryBox OuterBox() const { return outer_box_; }
  GeometryBox InnerBox() const { return inner_box_; }

  bool operator==(const StyleBorderShape& o) const {
    return base::ValuesEquivalent(outer_, o.outer_) &&
           base::ValuesEquivalent(inner_, o.inner_) &&
           outer_box_ == o.outer_box_ && inner_box_ == o.inner_box_;
  }

 private:
  Member<BasicShape> outer_;
  Member<BasicShape> inner_;
  GeometryBox outer_box_;
  GeometryBox inner_box_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_BORDER_SHAPE_H_
