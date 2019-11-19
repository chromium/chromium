/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_FRAGMENT_H_

#include "third_party/blink/renderer/core/layout/line/glyph_overflow.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A SVGTextFragment describes a text fragment of a LayoutSVGInlineText which
// can be laid out at once.
struct SVGTextFragment {
  DISALLOW_NEW();
  SVGTextFragment()
      : character_offset(0),
        metrics_list_offset(0),
        length(0),
        is_text_on_path(false),
        is_vertical(false),
        x(0),
        y(0),
        width(0),
        height(0),
        length_adjust_scale(1),
        length_adjust_bias(0) {}

  enum TransformType {
    kTransformRespectingTextLength,
    kTransformIgnoringTextLength
  };

  FloatRect BoundingBox(float baseline) const {
    FloatRect fragment_rect(x, y - baseline, width, height);
    if (!IsTransformed())
      return fragment_rect;
    return BuildNormalFragmentTransform().MapRect(fragment_rect);
  }

  FloatRect OverflowBoundingBox(float baseline) const {
    FloatRect fragment_rect(
        x - glyph_overflow.left, y - baseline - glyph_overflow.top,
        width + glyph_overflow.left + glyph_overflow.right,
        height + glyph_overflow.top + glyph_overflow.bottom);
    if (!IsTransformed())
      return fragment_rect;
    return BuildNormalFragmentTransform().MapRect(fragment_rect);
  }

  FloatQuad BoundingQuad(float baseline) const {
    FloatQuad fragment_quad(FloatRect(x, y - baseline, width, height));
    if (!IsTransformed())
      return fragment_quad;
    return BuildNormalFragmentTransform().MapQuad(fragment_quad);
  }

  AffineTransform BuildFragmentTransform(
      TransformType type = kTransformRespectingTextLength) const {
    if (type == kTransformIgnoringTextLength) {
      AffineTransform result = transform;
      TransformAroundOrigin(result);
      return result;
    }
    return BuildNormalFragmentTransform();
  }

  bool AffectedByTextLength() const { return length_adjust_scale != 1; }

  bool IsTransformed() const {
    return AffectedByTextLength() || !transform.IsIdentity();
  }

  // The first laid out character starts at LayoutSVGInlineText::characters() +
  // characterOffset.
  unsigned character_offset;
  unsigned metrics_list_offset;
  unsigned length : 30;
  unsigned is_text_on_path : 1;
  unsigned is_vertical : 1;

  float x;
  float y;
  float width;
  float height;

  GlyphOverflow glyph_overflow;

  // Includes rotation/glyph-orientation-(horizontal|vertical) transforms, as
  // well as orientation related shifts
  // (see SVGTextLayoutEngine, which builds this transformation).
  AffineTransform transform;

  // Contains lengthAdjust related transformations, which are not allowed to
  // influence the SVGTextQuery code.
  float length_adjust_scale;
  float length_adjust_bias;

 private:
  AffineTransform BuildNormalFragmentTransform() const {
    if (is_text_on_path)
      return BuildTransformForTextOnPath();
    return BuildTransformForTextOnLine();
  }

  void TransformAroundOrigin(AffineTransform& result) const {
    // Returns (translate(x, y) * result) * translate(-x, -y).
    result.SetE(result.E() + x);
    result.SetF(result.F() + y);
    result.Translate(-x, -y);
  }

  AffineTransform BuildTransformForTextOnPath() const {
    // For text-on-path layout, multiply the transform with the
    // lengthAdjustTransform before orienting the resulting transform.
    // T(x,y) * M(transform) * M(lengthAdjust) * T(-x,-y)
    AffineTransform result = !AffectedByTextLength()
                                 ? transform
                                 : transform * LengthAdjustTransform();
    if (!result.IsIdentity())
      TransformAroundOrigin(result);
    return result;
  }

  AffineTransform LengthAdjustTransform() const {
    AffineTransform result;
    if (!AffectedByTextLength())
      return result;
    // Load a transform assuming horizontal direction, then swap if vertical.
    result.SetMatrix(length_adjust_scale, 0, 0, 1, length_adjust_bias, 0);
    if (is_vertical) {
      result.SetD(result.A());
      result.SetA(1);
      result.SetF(result.E());
      result.SetE(0);
    }
    return result;
  }

  AffineTransform BuildTransformForTextOnLine() const {
    // For text-on-line layout, orient the transform first, then multiply
    // the lengthAdjustTransform with the oriented transform.
    // M(lengthAdjust) * T(x,y) * M(transform) * T(-x,-y)
    if (transform.IsIdentity())
      return LengthAdjustTransform();

    AffineTransform result = transform;
    TransformAroundOrigin(result);
    result.PreMultiply(LengthAdjustTransform());
    return result;
  }
};

}  // namespace blink

#endif
