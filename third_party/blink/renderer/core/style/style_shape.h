// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SHAPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SHAPE_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/basic_shapes.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// The computed version of the shape() function
// https://drafts.csswg.org/css-shapes-2/#shape-function
// It stores each shape segment as a struct with the appropriate Length or enum
// values.
class StyleShape final : public BasicShape {
 public:
  struct Segment {
    enum Type { kLine, kMove };
    enum PointOrigin { kReferenceBox, kPreviousSegment };

    bool operator==(const Segment& other) const = default;

    Type type;
    PointOrigin end_point_origin;
    LengthPoint end_point;
  };

  static scoped_refptr<StyleShape> Create(WindRule wind_rule,
                                          const LengthPoint& origin,
                                          Vector<Segment> segments) {
    return base::AdoptRef(
        new StyleShape(wind_rule, origin, std::move(segments)));
  }

  ShapeType GetType() const override { return kStyleShapeType; }
  void GetPath(Path&,
               const gfx::RectF& bounding_box,
               float zoom) const override;

  WindRule GetWindRule() const { return wind_rule_; }
  const LengthPoint& GetOrigin() const { return origin_; }
  const Vector<Segment>& Segments() const { return segments_; }

 protected:
  bool IsEqualAssumingSameType(const BasicShape&) const override;

 private:
  StyleShape(WindRule wind_rule,
             const LengthPoint& origin,
             Vector<Segment> segments);

  void ResolvePath(Path&, const gfx::SizeF&) const;
  WindRule wind_rule_;
  LengthPoint origin_;
  Vector<Segment> segments_;
};

template <>
struct DowncastTraits<StyleShape> {
  static bool AllowFrom(const BasicShape& value) {
    return value.GetType() == BasicShape::kStyleShapeType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_SHAPE_H_
