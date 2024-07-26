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

#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"

namespace blink {

SVGElementResourceClient* SVGResources::GetClient(const LayoutObject& object) {
  return To<SVGElement>(object.GetNode())->GetSVGResourceClient();
}

SVGElementResourceClient& SVGResources::EnsureClient(
    const LayoutObject& object) {
  return To<SVGElement>(object.GetNode())->EnsureSVGResourceClient();
}

gfx::RectF SVGResources::ReferenceBoxForEffects(
    const LayoutObject& layout_object,
    GeometryBox geometry_box,
    ForeignObjectQuirk foreign_object_quirk) {
  // Text "sub-elements" (<tspan>, <textpath>, <a>) should use the entire
  // <text>s object bounding box rather then their own.
  // https://svgwg.org/svg2-draft/text.html#ObjectBoundingBoxUnitsTextObjects
  const LayoutObject* obb_layout_object = &layout_object;
  if (layout_object.IsSVGInline()) {
    obb_layout_object =
        LayoutSVGText::LocateLayoutSVGTextAncestor(&layout_object);
  }
  DCHECK(obb_layout_object);

  gfx::RectF box;
  switch (geometry_box) {
    case GeometryBox::kPaddingBox:
    case GeometryBox::kContentBox:
    case GeometryBox::kFillBox:
      box = obb_layout_object->ObjectBoundingBox();
      break;
    case GeometryBox::kMarginBox:
    case GeometryBox::kBorderBox:
    case GeometryBox::kStrokeBox:
      box = obb_layout_object->StrokeBoundingBox();
      break;
    case GeometryBox::kViewBox: {
      const SVGViewportResolver viewport_resolver(obb_layout_object);
      box.set_size(viewport_resolver.ResolveViewport());
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (foreign_object_quirk == ForeignObjectQuirk::kEnabled &&
      obb_layout_object->IsSVGForeignObject()) {
    // For SVG foreign objects, remove the position part of the bounding box.
    // The position is already baked into the transform, and we don't want to
    // re-apply the offset when, e.g., using "objectBoundingBox" for
    // clipPathUnits. Similarly, the reference box should have zoom applied.
    // This simple approach only works because foreign objects do not support
    // strokes.
    box.set_origin(gfx::PointF());
    box.Scale(obb_layout_object->StyleRef().EffectiveZoom());
  }

  return box;
}

void SVGResources::UpdateEffects(LayoutObject& object,
                                 StyleDifference diff,
                                 const ComputedStyle* old_style) {
  const bool had_client = GetClient(object);
  const ComputedStyle& style = object.StyleRef();
  if (auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style.ClipPath())) {
    reference_clip->AddClient(EnsureClient(object));
  }
  if (style.HasFilter())
    style.Filter().AddClient(EnsureClient(object));
  // FilterChanged() includes changes from more than just the 'filter'
  // property, so explicitly check that a filter existed or exists.
  if (diff.FilterChanged() &&
      (style.HasFilter() || (old_style && old_style->HasFilter()))) {
    // We either created one above, or had one already.
    DCHECK(GetClient(object));
    if (RuntimeEnabledFeatures::SvgTransformOptimizationEnabled()) {
      GetClient(object)->InvalidateFilterData();
    } else {
      object.SetNeedsPaintPropertyUpdate();
      GetClient(object)->MarkFilterDataDirty();
    }
  }
  if (!old_style || !had_client)
    return;
  SVGElementResourceClient* client = GetClient(object);
  if (auto* old_reference_clip =
          DynamicTo<ReferenceClipPathOperation>(old_style->ClipPath())) {
    old_reference_clip->RemoveClient(*client);
  }
  if (old_style->HasFilter())
    old_style->Filter().RemoveClient(*client);
}

void SVGResources::ClearEffects(const LayoutObject& object) {
  const ComputedStyle* style = object.Style();
  if (!style)
    return;
  SVGElementResourceClient* client = GetClient(object);
  if (!client)
    return;
  if (auto* old_reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style->ClipPath())) {
    old_reference_clip->RemoveClient(*client);
  }
  if (style->HasFilter()) {
    style->Filter().RemoveClient(*client);
    // TODO(fs): We need to invalidate filter data here because the resource
    // client is owned by the Element - thus staying alive with it even when
    // the LayoutObject is detached. Move ownership to the LayoutObject.
    client->InvalidateFilterData();
  }
}

void SVGResources::UpdatePaints(const LayoutObject& object,
                                const ComputedStyle* old_style,
                                const ComputedStyle& style) {
  const bool had_client = GetClient(object);
  if (StyleSVGResource* paint_resource = style.FillPaint().Resource())
    paint_resource->AddClient(EnsureClient(object));
  if (StyleSVGResource* paint_resource = style.StrokePaint().Resource())
    paint_resource->AddClient(EnsureClient(object));
  if (had_client)
    ClearPaints(object, old_style);
}

void SVGResources::ClearPaints(const LayoutObject& object,
                               const ComputedStyle* style) {
  if (!style)
    return;
  SVGResourceClient* client = GetClient(object);
  if (!client)
    return;
  if (StyleSVGResource* paint_resource = style->FillPaint().Resource())
    paint_resource->RemoveClient(*client);
  if (StyleSVGResource* paint_resource = style->StrokePaint().Resource())
    paint_resource->RemoveClient(*client);
}

void SVGResources::UpdateMarkers(const LayoutObject& object,
                                 const ComputedStyle* old_style) {
  const bool had_client = GetClient(object);
  const ComputedStyle& style = object.StyleRef();
  if (StyleSVGResource* marker_resource = style.MarkerStartResource())
    marker_resource->AddClient(EnsureClient(object));
  if (StyleSVGResource* marker_resource = style.MarkerMidResource())
    marker_resource->AddClient(EnsureClient(object));
  if (StyleSVGResource* marker_resource = style.MarkerEndResource())
    marker_resource->AddClient(EnsureClient(object));
  if (had_client)
    ClearMarkers(object, old_style);
}

void SVGResources::ClearMarkers(const LayoutObject& object,
                                const ComputedStyle* style) {
  if (!style)
    return;
  SVGResourceClient* client = GetClient(object);
  if (!client)
    return;
  if (StyleSVGResource* marker_resource = style->MarkerStartResource())
    marker_resource->RemoveClient(*client);
  if (StyleSVGResource* marker_resource = style->MarkerMidResource())
    marker_resource->RemoveClient(*client);
  if (StyleSVGResource* marker_resource = style->MarkerEndResource())
    marker_resource->RemoveClient(*client);
}

class SVGElementResourceClient::FilterData final
    : public GarbageCollected<SVGElementResourceClient::FilterData> {
 public:
  FilterData(FilterEffect* last_effect, SVGFilterGraphNodeMap* node_map)
      : last_effect_(last_effect), node_map_(node_map) {}

  bool HasEffects() const { return last_effect_ != nullptr; }
  sk_sp<PaintFilter> BuildPaintFilter() {
    return paint_filter_builder::Build(last_effect_.Get(),
                                       kInterpolationSpaceSRGB);
  }

  // Perform a finegrained invalidation of the filter chain for the
  // specified filter primitive and attribute. Returns false if no
  // further invalidation is required, otherwise true.
  bool Invalidate(SVGFilterPrimitiveStandardAttributes& primitive,
                  const QualifiedName& attribute) {
    if (FilterEffect* effect = node_map_->EffectForElement(primitive)) {
      if (!primitive.SetFilterEffectAttribute(effect, attribute))
        return false;  // No change
      node_map_->InvalidateDependentEffects(effect);
    }
    return true;
  }

  void Dispose() {
    node_map_ = nullptr;
    if (last_effect_)
      last_effect_->DisposeImageFiltersRecursive();
    last_effect_ = nullptr;
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(last_effect_);
    visitor->Trace(node_map_);
  }

 private:
  Member<FilterEffect> last_effect_;
  Member<SVGFilterGraphNodeMap> node_map_;
};

SVGElementResourceClient::SVGElementResourceClient(SVGElement* element)
    : element_(element), filter_data_dirty_(false) {}

namespace {

template <typename ContainerType>
bool ContainsResource(const ContainerType* container, SVGResource* resource) {
  return container && container->Resource() == resource;
}

bool ContainsResource(const FilterOperations& operations,
                      SVGResource* resource) {
  return base::ranges::any_of(
      operations.Operations(), [resource](const FilterOperation* operation) {
        return ContainsResource(DynamicTo<ReferenceFilterOperation>(operation),
                                resource);
      });
}

}  // namespace

void SVGElementResourceClient::ResourceContentChanged(SVGResource* resource) {
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;

  const ComputedStyle& style = layout_object->StyleRef();
  if (style.HasFilter() && ContainsResource(style.Filter(), resource)) {
    InvalidateFilterData();
    layout_object->SetShouldDoFullPaintInvalidation();
  }

  if (auto* container = DynamicTo<LayoutSVGResourceContainer>(layout_object)) {
    container->RemoveAllClientsFromCache();
    return;
  }

  if (ContainsResource(style.FillPaint().Resource(), resource) ||
      ContainsResource(style.StrokePaint().Resource(), resource)) {
    // Since LayoutSVGInlineTexts don't have SVGResources (they use their
    // parent's), they will not be notified of changes to paint servers. So
    // if the client is one that could have a LayoutSVGInlineText use a
    // paint invalidation reason that will force paint invalidation of the
    // entire <text>/<tspan>/... subtree.
    layout_object->SetSubtreeShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kSVGResource);
  }

  bool needs_layout = false;
  if (ContainsResource(style.MarkerStartResource(), resource) ||
      ContainsResource(style.MarkerMidResource(), resource) ||
      ContainsResource(style.MarkerEndResource(), resource)) {
    needs_layout = true;
    layout_object->SetNeedsBoundariesUpdate();
  }

  const auto* clip_reference =
      DynamicTo<ReferenceClipPathOperation>(style.ClipPath());
  if (ContainsResource(clip_reference, resource)) {
    // TODO(fs): "Downgrade" to non-subtree?
    layout_object->SetSubtreeShouldDoFullPaintInvalidation();
    layout_object->SetNeedsPaintPropertyUpdate();
  }

  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      *layout_object, needs_layout);
}

void SVGElementResourceClient::FilterPrimitiveChanged(
    SVGResource* resource,
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  if (filter_data_ && !filter_data_->Invalidate(primitive, attribute))
    return;  // No change
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;
  layout_object->SetNeedsPaintPropertyUpdate();
  MarkFilterDataDirty();
  LayoutSVGResourceContainer::InvalidateDependentElements(*layout_object,
                                                          false);
  LayoutSVGResourceContainer::InvalidateAncestorChainResources(*layout_object,
                                                               false);
}

SVGElementResourceClient::FilterData*
SVGElementResourceClient::CreateFilterDataWithNodeMap(
    FilterEffectBuilder& builder,
    const ReferenceFilterOperation& reference_filter) {
  auto* node_map = MakeGarbageCollected<SVGFilterGraphNodeMap>();
  Filter* filter =
      builder.BuildReferenceFilter(reference_filter, nullptr, node_map);
  if (!filter)
    return nullptr;
  paint_filter_builder::PopulateSourceGraphicImageFilters(
      filter->GetSourceGraphic(), kInterpolationSpaceSRGB);
  return MakeGarbageCollected<FilterData>(filter->LastEffect(), node_map);
}

void SVGElementResourceClient::UpdateFilterData(
    CompositorFilterOperations& operations) {
  DCHECK(element_->GetLayoutObject());
  const LayoutObject& object = *element_->GetLayoutObject();
  gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(object);
  if (!operations.IsEmpty() && !filter_data_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;
  const ComputedStyle& style = object.StyleRef();
  FilterEffectBuilder builder(
      reference_box, std::nullopt, 1,
      style.VisitedDependentColor(GetCSSPropertyColor()),
      style.UsedColorScheme());
  builder.SetShorthandScale(1 / style.EffectiveZoom());
  const FilterOperations& filter = style.Filter();
  // If the filter is a single 'url(...)' reference we can optimize some
  // mutations to the referenced filter chain by tracking the filter
  // dependencies and only perform partial invalidations of the filter chain.
  const bool is_single_reference_filter =
      filter.size() == 1 && IsA<ReferenceFilterOperation>(*filter.at(0));
  if (is_single_reference_filter) {
    if (!filter_data_) {
      filter_data_ = CreateFilterDataWithNodeMap(
          builder, To<ReferenceFilterOperation>(*filter.at(0)));
    }
    operations.Clear();
    if (filter_data_) {
      // If the referenced filter exists but does not contain any primitives,
      // then the rendering of the element should be disabled.
      if (filter_data_->HasEffects()) {
        // BuildPaintFilter() can return null which means pass-through.
        operations.AppendReferenceFilter(filter_data_->BuildPaintFilter());
      } else {
        // Create a filter chain that yields transparent black.
        operations.AppendOpacityFilter(0);
      }
    }
  } else {
    // Drop any existing filter data since the filter is no longer
    // cacheable.
    if (FilterData* filter_data = filter_data_.Release())
      filter_data->Dispose();

    operations = builder.BuildFilterOperations(filter);
  }
  operations.SetReferenceBox(reference_box);
  filter_data_dirty_ = false;
}

void SVGElementResourceClient::InvalidateFilterData() {
  // If we performed an "optimized" invalidation via FilterPrimitiveChanged(),
  // we could have set |filter_data_dirty_| but not cleared |filter_data_|.
  if (filter_data_dirty_ && !filter_data_)
    return;
  if (FilterData* filter_data = filter_data_.Release())
    filter_data->Dispose();
  if (LayoutObject* layout_object = element_->GetLayoutObject()) {
    layout_object->SetNeedsPaintPropertyUpdate();
    MarkFilterDataDirty();
  }
}

void SVGElementResourceClient::MarkFilterDataDirty() {
  DCHECK(element_->GetLayoutObject());
  DCHECK(element_->GetLayoutObject()->NeedsPaintPropertyUpdate());
  filter_data_dirty_ = true;
}

void SVGElementResourceClient::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(filter_data_);
  SVGResourceClient::Trace(visitor);
}

SVGResourceInvalidator::SVGResourceInvalidator(LayoutObject& object)
    : object_(object) {}

void SVGResourceInvalidator::InvalidateEffects() {
  const ComputedStyle& style = object_.StyleRef();
  if (style.HasFilter()) {
    if (SVGElementResourceClient* client = SVGResources::GetClient(object_))
      client->InvalidateFilterData();
  }
  if (style.HasClipPath() || style.HasMask()) {
    object_.SetShouldDoFullPaintInvalidation();
    object_.SetNeedsPaintPropertyUpdate();
  }
}

void SVGResourceInvalidator::InvalidatePaints() {
  SVGElementResourceClient* client = SVGResources::GetClient(object_);
  if (!client)
    return;
  bool needs_invalidation = false;
  const ComputedStyle& style = object_.StyleRef();
  if (auto* fill = GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
          *client, style.FillPaint().Resource())) {
    fill->RemoveClientFromCache(*client);
    needs_invalidation = true;
  }
  if (auto* stroke = GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
          *client, style.StrokePaint().Resource())) {
    stroke->RemoveClientFromCache(*client);
    needs_invalidation = true;
  }
  if (!needs_invalidation)
    return;
  object_.SetSubtreeShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kSVGResource);
}

}  // namespace blink
