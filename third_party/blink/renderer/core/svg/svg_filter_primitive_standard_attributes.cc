/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_filter_primitive.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(
    const QualifiedName& tag_name,
    Document& document)
    : SVGElement(tag_name, document),
      // Spec: If the x/y attribute is not specified, the effect is as if a
      // value of "0%" were specified.
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent0)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent0)),
      // Spec: If the width/height attribute is not specified, the effect is as
      // if a value of "100%" were specified.
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent100)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent100)),
      result_(MakeGarbageCollected<SVGAnimatedString>(this,
                                                      svg_names::kResultAttr)) {
}

void SVGFilterPrimitiveStandardAttributes::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(result_);
  SVGElement::Trace(visitor);
}

bool SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  DCHECK(attr_name == svg_names::kColorInterpolationFiltersAttr);
  DCHECK(GetLayoutObject());
  EColorInterpolation color_interpolation =
      GetLayoutObject()->StyleRef().ColorInterpolationFilters();
  InterpolationSpace resolved_interpolation_space =
      SVGFilterBuilder::ResolveInterpolationSpace(color_interpolation);
  if (resolved_interpolation_space == effect->OperatingInterpolationSpace())
    return false;
  effect->SetOperatingInterpolationSpace(resolved_interpolation_space);
  return true;
}

void SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr ||
      attr_name == svg_names::kHeightAttr ||
      attr_name == svg_names::kResultAttr) {
    Invalidate();
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

void SVGFilterPrimitiveStandardAttributes::ChildrenChanged(
    const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (!change.ByParser())
    Invalidate();
}

static gfx::RectF DefaultFilterPrimitiveSubregion(FilterEffect* filter_effect) {
  // https://drafts.fxtf.org/filter-effects/#FilterPrimitiveSubRegion
  DCHECK(filter_effect->GetFilter());

  // <feTurbulence>, <feFlood> and <feImage> don't have input effects, so use
  // the filter region as default subregion. <feTile> does have an input
  // reference, but due to its function (and special-cases) its default
  // resolves to the filter region.
  if (filter_effect->GetFilterEffectType() == kFilterEffectTypeTile ||
      !filter_effect->NumberOfEffectInputs())
    return filter_effect->GetFilter()->FilterRegion();

  // "x, y, width and height default to the union (i.e., tightest fitting
  // bounding box) of the subregions defined for all referenced nodes."
  gfx::RectF subregion_union;
  for (const auto& input_effect : filter_effect->InputEffects()) {
    // "If ... one or more of the referenced nodes is a standard input
    // ... the default subregion is 0%, 0%, 100%, 100%, where as a
    // special-case the percentages are relative to the dimensions of the
    // filter region..."
    if (input_effect->GetFilterEffectType() == kFilterEffectTypeSourceInput)
      return filter_effect->GetFilter()->FilterRegion();
    subregion_union.Union(input_effect->FilterPrimitiveSubregion());
  }
  return subregion_union;
}

void SVGFilterPrimitiveStandardAttributes::SetStandardAttributes(
    FilterEffect* filter_effect,
    SVGUnitTypes::SVGUnitType primitive_units,
    const gfx::RectF& reference_box) const {
  DCHECK(filter_effect);

  gfx::RectF subregion = DefaultFilterPrimitiveSubregion(filter_effect);
  gfx::RectF primitive_boundaries =
      LayoutSVGResourceContainer::ResolveRectangle(*this, primitive_units,
                                                   reference_box);

  if (x()->IsSpecified())
    subregion.set_x(primitive_boundaries.x());
  if (y()->IsSpecified())
    subregion.set_y(primitive_boundaries.y());
  if (width()->IsSpecified())
    subregion.set_width(primitive_boundaries.width());
  if (height()->IsSpecified())
    subregion.set_height(primitive_boundaries.height());

  filter_effect->SetFilterPrimitiveSubregion(subregion);
}

LayoutObject* SVGFilterPrimitiveStandardAttributes::CreateLayoutObject(
    const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGFilterPrimitive>(this);
}

bool SVGFilterPrimitiveStandardAttributes::LayoutObjectIsNeeded(
    const DisplayStyle& style) const {
  if (IsA<SVGFilterElement>(parentNode()))
    return SVGElement::LayoutObjectIsNeeded(style);

  return false;
}

void SVGFilterPrimitiveStandardAttributes::Invalidate() {
  if (auto* filter = DynamicTo<SVGFilterElement>(parentElement()))
    filter->InvalidateFilterChain();
}

void SVGFilterPrimitiveStandardAttributes::PrimitiveAttributeChanged(
    const QualifiedName& attribute) {
  if (auto* filter = DynamicTo<SVGFilterElement>(parentElement()))
    filter->PrimitiveAttributeChanged(*this, attribute);
}

void InvalidateFilterPrimitiveParent(SVGElement& element) {
  auto* svg_parent =
      DynamicTo<SVGFilterPrimitiveStandardAttributes>(element.parentElement());
  if (!svg_parent)
    return;
  svg_parent->Invalidate();
}

SVGAnimatedPropertyBase*
SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else if (attribute_name == svg_names::kResultAttr) {
    return result_.Get();
  } else {
    return SVGElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(), y_.Get(), width_.Get(),
                                   height_.Get(), result_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink
