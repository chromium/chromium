/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text_path.h"

#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_path_element.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

PathPositionMapper::PathPositionMapper(const Path& path,
                                       float computed_path_length,
                                       float start_offset)
    : position_calculator_(path),
      path_length_(computed_path_length),
      path_start_offset_(start_offset) {}

PathPositionMapper::PositionType PathPositionMapper::PointAndNormalAtLength(
    float length,
    PointAndTangent& point_and_tangent) {
  if (length < 0)
    return kBeforePath;
  if (length > path_length_)
    return kAfterPath;
  DCHECK_GE(length, 0);
  DCHECK_LE(length, path_length_);

  point_and_tangent = position_calculator_.PointAndNormalAtLength(length);
  return kOnPath;
}

LayoutSVGTextPath::LayoutSVGTextPath(Element* element)
    : LayoutSVGInline(element) {}

bool LayoutSVGTextPath::IsChildAllowed(LayoutObject* child,
                                       const ComputedStyle&) const {
  NOT_DESTROYED();
  if (child->IsText())
    return SVGLayoutSupport::IsLayoutableTextNode(child);

  return child->IsSVGInline() && !child->IsSVGTextPath();
}

std::unique_ptr<PathPositionMapper> LayoutSVGTextPath::LayoutPath() const {
  NOT_DESTROYED();
  const auto& text_path_element = To<SVGTextPathElement>(*GetNode());
  Element* target_element = SVGURIReference::TargetElementFromIRIString(
      text_path_element.HrefString(), text_path_element.OriginatingTreeScope());

  const auto* path_element = DynamicTo<SVGPathElement>(target_element);
  if (!path_element)
    return nullptr;

  Path path_data = path_element->AsPath();
  if (path_data.IsEmpty())
    return nullptr;

  // Spec: The 'transform' attribute on the referenced 'path' ...
  // element represents a supplemental transformation relative to the current
  // user coordinate system for the current 'text' element, including any
  // adjustments to the current user coordinate system due to a possible
  // 'transform' property on the current 'text' element.
  // https://svgwg.org/svg2-draft/text.html#TextPathElement
  path_data.Transform(
      path_element->CalculateTransform(SVGElement::kIncludeMotionTransform));

  // Determine the length to resolve any percentage 'startOffset'
  // against - either 'pathLength' (author path length) or the
  // computed length of the path.
  float computed_path_length = path_data.length();
  float author_path_length = path_element->AuthorPathLength();
  float offset_scale = 1;
  if (!std::isnan(author_path_length)) {
    offset_scale = SVGGeometryElement::PathLengthScaleFactor(
        computed_path_length, author_path_length);
  } else {
    author_path_length = computed_path_length;
  }

  const SVGLengthConversionData conversion_data(*this);
  float path_start_offset =
      text_path_element.startOffset()->CurrentValue()->Value(
          conversion_data, author_path_length);
  path_start_offset *= offset_scale;

  return std::make_unique<PathPositionMapper>(path_data, computed_path_length,
                                              path_start_offset);
}

}  // namespace blink
