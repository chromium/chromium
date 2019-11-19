/*
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

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_alpha.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

class FilterInputKeywords {
 public:
  static const AtomicString& GetSourceGraphic() {
    DEFINE_STATIC_LOCAL(const AtomicString, source_graphic_name,
                        ("SourceGraphic"));
    return source_graphic_name;
  }

  static const AtomicString& SourceAlpha() {
    DEFINE_STATIC_LOCAL(const AtomicString, source_alpha_name, ("SourceAlpha"));
    return source_alpha_name;
  }

  static const AtomicString& FillPaint() {
    DEFINE_STATIC_LOCAL(const AtomicString, fill_paint_name, ("FillPaint"));
    return fill_paint_name;
  }

  static const AtomicString& StrokePaint() {
    DEFINE_STATIC_LOCAL(const AtomicString, stroke_paint_name, ("StrokePaint"));
    return stroke_paint_name;
  }
};

}  // namespace

SVGFilterGraphNodeMap::SVGFilterGraphNodeMap() = default;

void SVGFilterGraphNodeMap::AddBuiltinEffect(FilterEffect* effect) {
  effect_references_.insert(effect, FilterEffectSet());
}

void SVGFilterGraphNodeMap::AddPrimitive(LayoutObject* object,
                                         FilterEffect* effect) {
  // The effect must be a newly created filter effect.
  DCHECK(!effect_references_.Contains(effect));
  DCHECK(!object || !effect_renderer_.Contains(object));
  effect_references_.insert(effect, FilterEffectSet());

  unsigned number_of_input_effects = effect->InputEffects().size();

  // Add references from the inputs of this effect to the effect itself, to
  // allow determining what effects needs to be invalidated when a certain
  // effect changes.
  for (unsigned i = 0; i < number_of_input_effects; ++i)
    EffectReferences(effect->InputEffect(i)).insert(effect);

  // If object is null, that means the element isn't attached for some
  // reason, which in turn mean that certain types of invalidation will not
  // work (the LayoutObject -> FilterEffect mapping will not be defined).
  if (object)
    effect_renderer_.insert(object, effect);
}

void SVGFilterGraphNodeMap::InvalidateDependentEffects(FilterEffect* effect) {
  if (!effect->HasImageFilter())
    return;

  effect->DisposeImageFilters();

  FilterEffectSet& effect_references = EffectReferences(effect);
  for (FilterEffect* effect_reference : effect_references)
    InvalidateDependentEffects(effect_reference);
}

void SVGFilterGraphNodeMap::Trace(blink::Visitor* visitor) {
  visitor->Trace(effect_renderer_);
  visitor->Trace(effect_references_);
}

SVGFilterBuilder::SVGFilterBuilder(FilterEffect* source_graphic,
                                   SVGFilterGraphNodeMap* node_map,
                                   const PaintFlags* fill_flags,
                                   const PaintFlags* stroke_flags)
    : node_map_(node_map) {
  FilterEffect* source_graphic_ref = source_graphic;
  builtin_effects_.insert(FilterInputKeywords::GetSourceGraphic(),
                          source_graphic_ref);
  builtin_effects_.insert(
      FilterInputKeywords::SourceAlpha(),
      MakeGarbageCollected<SourceAlpha>(source_graphic_ref));
  if (fill_flags) {
    builtin_effects_.insert(FilterInputKeywords::FillPaint(),
                            MakeGarbageCollected<PaintFilterEffect>(
                                source_graphic_ref->GetFilter(), *fill_flags));
  }
  if (stroke_flags) {
    builtin_effects_.insert(
        FilterInputKeywords::StrokePaint(),
        MakeGarbageCollected<PaintFilterEffect>(source_graphic_ref->GetFilter(),
                                                *stroke_flags));
  }
  AddBuiltinEffects();
}

void SVGFilterBuilder::AddBuiltinEffects() {
  if (!node_map_)
    return;
  for (const auto& entry : builtin_effects_)
    node_map_->AddBuiltinEffect(entry.value.Get());
}

// Returns the color-interpolation-filters property of the element.
static EColorInterpolation ColorInterpolationForElement(
    SVGElement& element,
    EColorInterpolation parent_color_interpolation) {
  if (const LayoutObject* layout_object = element.GetLayoutObject())
    return layout_object->StyleRef().SvgStyle().ColorInterpolationFilters();

  // No layout has been performed, try to determine the property value
  // "manually" (used by external SVG files.)
  if (const CSSPropertyValueSet* property_set =
          element.PresentationAttributeStyle()) {
    const CSSValue* css_value = property_set->GetPropertyCSSValue(
        CSSPropertyID::kColorInterpolationFilters);
    if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(css_value)) {
      return identifier_value->ConvertTo<EColorInterpolation>();
    }
  }
  // 'auto' is the default (per Filter Effects), but since the property is
  // inherited, propagate the parent's value.
  return parent_color_interpolation;
}

InterpolationSpace SVGFilterBuilder::ResolveInterpolationSpace(
    EColorInterpolation color_interpolation) {
  return color_interpolation == CI_LINEARRGB ? kInterpolationSpaceLinear
                                             : kInterpolationSpaceSRGB;
}

void SVGFilterBuilder::BuildGraph(Filter* filter,
                                  SVGFilterElement& filter_element,
                                  const FloatRect& reference_box) {
  EColorInterpolation filter_color_interpolation =
      ColorInterpolationForElement(filter_element, CI_AUTO);
  SVGUnitTypes::SVGUnitType primitive_units =
      filter_element.primitiveUnits()->CurrentValue()->EnumValue();

  for (SVGElement* element = Traversal<SVGElement>::FirstChild(filter_element);
       element; element = Traversal<SVGElement>::NextSibling(*element)) {
    if (!element->IsFilterEffect())
      continue;

    auto& effect_element = To<SVGFilterPrimitiveStandardAttributes>(*element);
    FilterEffect* effect = effect_element.Build(this, filter);
    if (!effect)
      continue;

    if (node_map_)
      node_map_->AddPrimitive(effect_element.GetLayoutObject(), effect);

    effect_element.SetStandardAttributes(effect, primitive_units,
                                         reference_box);
    EColorInterpolation color_interpolation = ColorInterpolationForElement(
        effect_element, filter_color_interpolation);
    effect->SetOperatingInterpolationSpace(
        ResolveInterpolationSpace(color_interpolation));
    if (effect->InputsTaintOrigin() || effect_element.TaintsOrigin())
      effect->SetOriginTainted();

    Add(AtomicString(effect_element.result()->CurrentValue()->Value()), effect);
  }
}

void SVGFilterBuilder::Add(const AtomicString& id, FilterEffect* effect) {
  if (id.IsEmpty()) {
    last_effect_ = effect;
    return;
  }

  if (builtin_effects_.Contains(id))
    return;

  last_effect_ = effect;
  named_effects_.Set(id, last_effect_);
}

FilterEffect* SVGFilterBuilder::GetEffectById(const AtomicString& id) const {
  if (!id.IsEmpty()) {
    if (FilterEffect* builtin_effect = builtin_effects_.at(id))
      return builtin_effect;

    if (FilterEffect* named_effect = named_effects_.at(id))
      return named_effect;
  }

  if (last_effect_)
    return last_effect_.Get();

  return builtin_effects_.at(FilterInputKeywords::GetSourceGraphic());
}

}  // namespace blink
