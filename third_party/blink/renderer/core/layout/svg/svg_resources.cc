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

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/svg/layout_svg_foreign_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_filter_primitive_standard_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_pattern_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

SVGResources::SVGResources() : linked_resource_(nullptr) {}

SVGElementResourceClient* SVGResources::GetClient(const LayoutObject& object) {
  return To<SVGElement>(object.GetNode())->GetSVGResourceClient();
}

FloatRect SVGResources::ReferenceBoxForEffects(
    const LayoutObject& layout_object) {
  // For SVG foreign objects, remove the position part of the bounding box. The
  // position is already baked into the transform, and we don't want to re-apply
  // the offset when, e.g., using "objectBoundingBox" for clipPathUnits.
  // Use the frame size since it should have the proper zoom applied.
  if (auto* foreign = DynamicTo<LayoutSVGForeignObject>(layout_object))
    return FloatRect(FloatPoint::Zero(), FloatSize(foreign->Size()));

  // Text "sub-elements" (<tspan>, <textpath>, <a>) should use the entire
  // <text>s object bounding box rather then their own.
  // https://svgwg.org/svg2-draft/text.html#ObjectBoundingBoxUnitsTextObjects
  const LayoutObject* obb_layout_object = &layout_object;
  if (layout_object.IsSVGInline()) {
    obb_layout_object =
        LayoutSVGText::LocateLayoutSVGTextAncestor(&layout_object);
  }
  DCHECK(obb_layout_object);
  return obb_layout_object->ObjectBoundingBox();
}

static HashSet<AtomicString>& ClipperFilterMaskerTags() {
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, tag_list,
      ({
          // "container elements":
          // http://www.w3.org/TR/SVG11/intro.html#TermContainerElement
          // "graphics elements" :
          // http://www.w3.org/TR/SVG11/intro.html#TermGraphicsElement
          svg_names::kATag.LocalName(), svg_names::kCircleTag.LocalName(),
          svg_names::kEllipseTag.LocalName(), svg_names::kGTag.LocalName(),
          svg_names::kImageTag.LocalName(), svg_names::kLineTag.LocalName(),
          svg_names::kMarkerTag.LocalName(), svg_names::kMaskTag.LocalName(),
          svg_names::kPathTag.LocalName(), svg_names::kPolygonTag.LocalName(),
          svg_names::kPolylineTag.LocalName(), svg_names::kRectTag.LocalName(),
          svg_names::kSVGTag.LocalName(), svg_names::kTextTag.LocalName(),
          svg_names::kUseTag.LocalName(),
          // Not listed in the definitions is the clipPath element, the SVG spec
          // says though:
          // The "clipPath" element or any of its children can specify property
          // "clip-path".
          // So we have to add kClipPathTag here, otherwhise clip-path on
          // clipPath will fail. (Already mailed SVG WG, waiting for a solution)
          svg_names::kClipPathTag.LocalName(),
          // Not listed in the definitions are the text content elements, though
          // filter/clipper/masker on tspan/text/.. is allowed.
          // (Already mailed SVG WG, waiting for a solution)
          svg_names::kTextPathTag.LocalName(), svg_names::kTSpanTag.LocalName(),
          // Not listed in the definitions is the foreignObject element, but
          // clip-path is a supported attribute.
          svg_names::kForeignObjectTag.LocalName(),
          // Elements that we ignore, as it doesn't make any sense.
          // defs, pattern, switch (FIXME: Mail SVG WG about these)
          // symbol (is converted to a svg element, when referenced by use, we
          // can safely ignore it.)
      }));
  return tag_list;
}

bool SVGResources::SupportsMarkers(const SVGElement& element) {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tag_list,
                      ({
                          svg_names::kLineTag.LocalName(),
                          svg_names::kPathTag.LocalName(),
                          svg_names::kPolygonTag.LocalName(),
                          svg_names::kPolylineTag.LocalName(),
                      }));
  return tag_list.Contains(element.localName());
}

static HashSet<AtomicString>& FillAndStrokeTags() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tag_list,
                      ({
                          svg_names::kCircleTag.LocalName(),
                          svg_names::kEllipseTag.LocalName(),
                          svg_names::kLineTag.LocalName(),
                          svg_names::kPathTag.LocalName(),
                          svg_names::kPolygonTag.LocalName(),
                          svg_names::kPolylineTag.LocalName(),
                          svg_names::kRectTag.LocalName(),
                          svg_names::kTextTag.LocalName(),
                          svg_names::kTextPathTag.LocalName(),
                          svg_names::kTSpanTag.LocalName(),
                      }));
  return tag_list;
}

bool SVGResources::HasResourceData() const {
  return clipper_filter_masker_data_ || marker_data_ || fill_stroke_data_ ||
         linked_resource_;
}

static inline SVGResources& EnsureResources(
    std::unique_ptr<SVGResources>& resources) {
  if (!resources)
    resources = std::make_unique<SVGResources>();

  return *resources.get();
}

std::unique_ptr<SVGResources> SVGResources::BuildResources(
    const LayoutObject& object,
    const ComputedStyle& computed_style) {
  Node* node = object.GetNode();
  DCHECK(node);
  SECURITY_DCHECK(node->IsSVGElement());

  auto& element = To<SVGElement>(*node);

  const AtomicString& tag_name = element.localName();
  DCHECK(!tag_name.IsNull());

  const SVGComputedStyle& style = computed_style.SvgStyle();

  std::unique_ptr<SVGResources> resources;
  if (ClipperFilterMaskerTags().Contains(tag_name)) {
    if (computed_style.ClipPath() && !object.IsSVGRoot()) {
      if (LayoutSVGResourceClipper* clipper =
              GetSVGResourceAsType(computed_style.ClipPath())) {
        EnsureResources(resources).SetClipper(clipper);
      }
    }

    if (computed_style.HasFilter() && !object.IsSVGRoot()) {
      if (LayoutSVGResourceFilter* filter =
              GetFilterResourceForSVG(computed_style)) {
        EnsureResources(resources).SetFilter(filter);
      }
    }

    if (auto* masker = GetSVGResourceAsType<LayoutSVGResourceMasker>(
            style.MaskerResource())) {
      EnsureResources(resources).SetMasker(masker);
    }
  }

  if (style.HasMarkers() && SupportsMarkers(element)) {
    if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
            style.MarkerStartResource())) {
      EnsureResources(resources).SetMarkerStart(marker);
    }
    if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
            style.MarkerMidResource())) {
      EnsureResources(resources).SetMarkerMid(marker);
    }
    if (auto* marker = GetSVGResourceAsType<LayoutSVGResourceMarker>(
            style.MarkerEndResource())) {
      EnsureResources(resources).SetMarkerEnd(marker);
    }
  }

  if (FillAndStrokeTags().Contains(tag_name)) {
    if (auto* paint_resource =
            GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
                style.FillPaint().Resource())) {
      EnsureResources(resources).SetFill(paint_resource);
    }

    if (auto* paint_resource =
            GetSVGResourceAsType<LayoutSVGResourcePaintServer>(
                style.StrokePaint().Resource())) {
      EnsureResources(resources).SetStroke(paint_resource);
    }
  }

  if (auto* pattern = DynamicTo<SVGPatternElement>(element)) {
    const SVGPatternElement* directly_referenced_pattern =
        pattern->ReferencedElement();
    if (directly_referenced_pattern) {
      EnsureResources(resources).SetLinkedResource(
          DynamicTo<LayoutSVGResourceContainer>(
              directly_referenced_pattern->GetLayoutObject()));
    }
  }

  return (!resources || !resources->HasResourceData()) ? nullptr
                                                       : std::move(resources);
}

void SVGResources::ResourceDestroyed(LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (!HasResourceData())
    return;

  if (linked_resource_ == resource) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    linked_resource_->RemoveAllClientsFromCache();
    linked_resource_ = nullptr;
    return;
  }

  switch (resource->ResourceType()) {
    case kMaskerResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->masker == resource)
        clipper_filter_masker_data_->masker = nullptr;
      break;
    case kMarkerResourceType:
      if (!marker_data_)
        break;
      if (marker_data_->marker_start == resource)
        marker_data_->marker_start = nullptr;
      if (marker_data_->marker_mid == resource)
        marker_data_->marker_mid = nullptr;
      if (marker_data_->marker_end == resource)
        marker_data_->marker_end = nullptr;
      break;
    case kPatternResourceType:
    case kLinearGradientResourceType:
    case kRadialGradientResourceType:
      if (!fill_stroke_data_)
        break;
      if (fill_stroke_data_->fill == resource)
        fill_stroke_data_->fill = nullptr;
      if (fill_stroke_data_->stroke == resource)
        fill_stroke_data_->stroke = nullptr;
      break;
    case kFilterResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->filter == resource)
        clipper_filter_masker_data_->filter = nullptr;
      break;
    case kClipperResourceType:
      if (!clipper_filter_masker_data_)
        break;
      if (clipper_filter_masker_data_->clipper == resource)
        clipper_filter_masker_data_->clipper = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void SVGResources::ClearReferencesTo(LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (linked_resource_ == resource) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    linked_resource_ = nullptr;
    return;
  }

  switch (resource->ResourceType()) {
    case kMaskerResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->masker, resource);
      clipper_filter_masker_data_->masker = nullptr;
      break;
    case kMarkerResourceType:
      DCHECK(marker_data_);
      DCHECK(resource == MarkerStart() || resource == MarkerMid() ||
             resource == MarkerEnd());
      if (marker_data_->marker_start == resource)
        marker_data_->marker_start = nullptr;
      if (marker_data_->marker_mid == resource)
        marker_data_->marker_mid = nullptr;
      if (marker_data_->marker_end == resource)
        marker_data_->marker_end = nullptr;
      break;
    case kPatternResourceType:
    case kLinearGradientResourceType:
    case kRadialGradientResourceType:
      DCHECK(fill_stroke_data_);
      DCHECK(resource == Fill() || resource == Stroke());
      if (fill_stroke_data_->fill == resource)
        fill_stroke_data_->fill = nullptr;
      if (fill_stroke_data_->stroke == resource)
        fill_stroke_data_->stroke = nullptr;
      break;
    case kFilterResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->filter, resource);
      clipper_filter_masker_data_->filter = nullptr;
      break;
    case kClipperResourceType:
      DCHECK(clipper_filter_masker_data_);
      DCHECK_EQ(clipper_filter_masker_data_->clipper, resource);
      clipper_filter_masker_data_->clipper = nullptr;
      break;
    default:
      NOTREACHED();
  }
}

void SVGResources::BuildSetOfResources(
    HashSet<LayoutSVGResourceContainer*>& set) {
  if (!HasResourceData())
    return;

  if (linked_resource_) {
    DCHECK(!clipper_filter_masker_data_);
    DCHECK(!marker_data_);
    DCHECK(!fill_stroke_data_);
    set.insert(linked_resource_);
    return;
  }

  if (clipper_filter_masker_data_) {
    if (clipper_filter_masker_data_->clipper)
      set.insert(clipper_filter_masker_data_->clipper);
    if (clipper_filter_masker_data_->filter)
      set.insert(clipper_filter_masker_data_->filter);
    if (clipper_filter_masker_data_->masker)
      set.insert(clipper_filter_masker_data_->masker);
  }

  if (marker_data_) {
    if (marker_data_->marker_start)
      set.insert(marker_data_->marker_start);
    if (marker_data_->marker_mid)
      set.insert(marker_data_->marker_mid);
    if (marker_data_->marker_end)
      set.insert(marker_data_->marker_end);
  }

  if (fill_stroke_data_) {
    if (fill_stroke_data_->fill)
      set.insert(fill_stroke_data_->fill);
    if (fill_stroke_data_->stroke)
      set.insert(fill_stroke_data_->stroke);
  }
}

void SVGResources::SetClipper(LayoutSVGResourceClipper* clipper) {
  DCHECK(clipper);
  DCHECK_EQ(clipper->ResourceType(), kClipperResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->clipper = clipper;
}

void SVGResources::SetFilter(LayoutSVGResourceFilter* filter) {
  DCHECK(filter);
  DCHECK_EQ(filter->ResourceType(), kFilterResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->filter = filter;
}

void SVGResources::SetMarkerStart(LayoutSVGResourceMarker* marker_start) {
  DCHECK(marker_start);
  DCHECK_EQ(marker_start->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_start = marker_start;
}

void SVGResources::SetMarkerMid(LayoutSVGResourceMarker* marker_mid) {
  DCHECK(marker_mid);
  DCHECK_EQ(marker_mid->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_mid = marker_mid;
}

void SVGResources::SetMarkerEnd(LayoutSVGResourceMarker* marker_end) {
  DCHECK(marker_end);
  DCHECK_EQ(marker_end->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_end = marker_end;
}

void SVGResources::SetMasker(LayoutSVGResourceMasker* masker) {
  DCHECK(masker);
  DCHECK_EQ(masker->ResourceType(), kMaskerResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->masker = masker;
}

void SVGResources::SetFill(LayoutSVGResourcePaintServer* fill) {
  DCHECK(fill);

  if (!fill_stroke_data_)
    fill_stroke_data_ = std::make_unique<FillStrokeData>();

  fill_stroke_data_->fill = fill;
}

void SVGResources::SetStroke(LayoutSVGResourcePaintServer* stroke) {
  DCHECK(stroke);

  if (!fill_stroke_data_)
    fill_stroke_data_ = std::make_unique<FillStrokeData>();

  fill_stroke_data_->stroke = stroke;
}

void SVGResources::SetLinkedResource(
    LayoutSVGResourceContainer* linked_resource) {
  if (!linked_resource)
    return;

  linked_resource_ = linked_resource;
}

#if DCHECK_IS_ON()
void SVGResources::Dump(const LayoutObject* object) {
  DCHECK(object);
  DCHECK(object->GetNode());

  fprintf(stderr, "-> this=%p, SVGResources(layoutObject=%p, node=%p)\n", this,
          object, object->GetNode());
  fprintf(stderr, " | DOM Tree:\n");
  fprintf(stderr, "%s",
          object->GetNode()->ToTreeStringForThis().Utf8().c_str());

  fprintf(stderr, "\n | List of resources:\n");
  if (clipper_filter_masker_data_) {
    if (LayoutSVGResourceClipper* clipper =
            clipper_filter_masker_data_->clipper)
      fprintf(stderr, " |-> Clipper    : %p (node=%p)\n", clipper,
              clipper->GetElement());
    if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter)
      fprintf(stderr, " |-> Filter     : %p (node=%p)\n", filter,
              filter->GetElement());
    if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
      fprintf(stderr, " |-> Masker     : %p (node=%p)\n", masker,
              masker->GetElement());
  }

  if (marker_data_) {
    if (LayoutSVGResourceMarker* marker_start = marker_data_->marker_start)
      fprintf(stderr, " |-> MarkerStart: %p (node=%p)\n", marker_start,
              marker_start->GetElement());
    if (LayoutSVGResourceMarker* marker_mid = marker_data_->marker_mid)
      fprintf(stderr, " |-> MarkerMid  : %p (node=%p)\n", marker_mid,
              marker_mid->GetElement());
    if (LayoutSVGResourceMarker* marker_end = marker_data_->marker_end)
      fprintf(stderr, " |-> MarkerEnd  : %p (node=%p)\n", marker_end,
              marker_end->GetElement());
  }

  if (fill_stroke_data_) {
    if (LayoutSVGResourcePaintServer* fill = fill_stroke_data_->fill)
      fprintf(stderr, " |-> Fill       : %p (node=%p)\n", fill,
              fill->GetElement());
    if (LayoutSVGResourcePaintServer* stroke = fill_stroke_data_->stroke)
      fprintf(stderr, " |-> Stroke     : %p (node=%p)\n", stroke,
              stroke->GetElement());
  }

  if (linked_resource_)
    fprintf(stderr, " |-> xlink:href : %p (node=%p)\n", linked_resource_,
            linked_resource_->GetElement());
}
#endif

void SVGResources::UpdateClipPathFilterMask(SVGElement& element,
                                            const ComputedStyle* old_style,
                                            const ComputedStyle& style) {
  const bool had_client = element.GetSVGResourceClient();
  if (auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style.ClipPath()))
    reference_clip->AddClient(element.EnsureSVGResourceClient());
  if (style.HasFilter()) {
    SVGElementResourceClient& client = element.EnsureSVGResourceClient();
    style.Filter().AddClient(client);
    LayoutObject* layout_object = element.GetLayoutObject();
    // This is called from StyleDidChange so we should have a LayoutObject.
    DCHECK(layout_object);
    // TODO(fs): Reorganise the code so that we don't need to invalidate this
    // again in SVGResourcesCache::ClientStyleChanged (and potentially avoid
    // redundant invalidations).
    layout_object->SetNeedsPaintPropertyUpdate();
    client.MarkFilterDataDirty();
  }
  if (StyleSVGResource* masker_resource = style.SvgStyle().MaskerResource())
    masker_resource->AddClient(element.EnsureSVGResourceClient());
  if (had_client)
    ClearClipPathFilterMask(element, old_style);
}

void SVGResources::ClearClipPathFilterMask(SVGElement& element,
                                           const ComputedStyle* style) {
  if (!style)
    return;
  SVGElementResourceClient* client = element.GetSVGResourceClient();
  if (!client)
    return;
  if (auto* old_reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style->ClipPath()))
    old_reference_clip->RemoveClient(*client);
  if (style->HasFilter()) {
    style->Filter().RemoveClient(*client);
    client->InvalidateFilterData();
  }
  if (StyleSVGResource* masker_resource = style->SvgStyle().MaskerResource())
    masker_resource->RemoveClient(*client);
}

void SVGResources::UpdatePaints(SVGElement& element,
                                const ComputedStyle* old_style,
                                const ComputedStyle& style) {
  const bool had_client = element.GetSVGResourceClient();
  const SVGComputedStyle& svg_style = style.SvgStyle();
  if (StyleSVGResource* paint_resource = svg_style.FillPaint().Resource())
    paint_resource->AddClient(element.EnsureSVGResourceClient());
  if (StyleSVGResource* paint_resource = svg_style.StrokePaint().Resource())
    paint_resource->AddClient(element.EnsureSVGResourceClient());
  if (had_client)
    ClearPaints(element, old_style);
}

void SVGResources::ClearPaints(SVGElement& element,
                               const ComputedStyle* style) {
  if (!style)
    return;
  SVGResourceClient* client = element.GetSVGResourceClient();
  if (!client)
    return;
  const SVGComputedStyle& old_svg_style = style->SvgStyle();
  if (StyleSVGResource* paint_resource = old_svg_style.FillPaint().Resource())
    paint_resource->RemoveClient(*client);
  if (StyleSVGResource* paint_resource = old_svg_style.StrokePaint().Resource())
    paint_resource->RemoveClient(*client);
}

void SVGResources::UpdateMarkers(SVGElement& element,
                                 const ComputedStyle* old_style,
                                 const ComputedStyle& style) {
  const bool had_client = element.GetSVGResourceClient();
  const SVGComputedStyle& svg_style = style.SvgStyle();
  if (StyleSVGResource* marker_resource = svg_style.MarkerStartResource())
    marker_resource->AddClient(element.EnsureSVGResourceClient());
  if (StyleSVGResource* marker_resource = svg_style.MarkerMidResource())
    marker_resource->AddClient(element.EnsureSVGResourceClient());
  if (StyleSVGResource* marker_resource = svg_style.MarkerEndResource())
    marker_resource->AddClient(element.EnsureSVGResourceClient());
  if (had_client)
    ClearMarkers(element, old_style);
}

void SVGResources::ClearMarkers(SVGElement& element,
                                const ComputedStyle* style) {
  if (!style)
    return;
  SVGResourceClient* client = element.GetSVGResourceClient();
  if (!client)
    return;
  const SVGComputedStyle& old_svg_style = style->SvgStyle();
  if (StyleSVGResource* marker_resource = old_svg_style.MarkerStartResource())
    marker_resource->RemoveClient(*client);
  if (StyleSVGResource* marker_resource = old_svg_style.MarkerMidResource())
    marker_resource->RemoveClient(*client);
  if (StyleSVGResource* marker_resource = old_svg_style.MarkerEndResource())
    marker_resource->RemoveClient(*client);
}

sk_sp<PaintFilter> FilterData::BuildPaintFilter() {
  return paint_filter_builder::Build(last_effect_, kInterpolationSpaceSRGB);
}

bool FilterData::Invalidate(SVGFilterPrimitiveStandardAttributes& primitive,
                            const QualifiedName& attribute) {
  if (FilterEffect* effect = node_map_->EffectForElement(primitive)) {
    if (!primitive.SetFilterEffectAttribute(effect, attribute))
      return false;  // No change
    node_map_->InvalidateDependentEffects(effect);
  }
  return true;
}

void FilterData::Trace(Visitor* visitor) const {
  visitor->Trace(last_effect_);
  visitor->Trace(node_map_);
}

void FilterData::Dispose() {
  node_map_ = nullptr;
  if (last_effect_)
    last_effect_->DisposeImageFiltersRecursive();
  last_effect_ = nullptr;
}

SVGElementResourceClient::SVGElementResourceClient(SVGElement* element)
    : element_(element), filter_data_dirty_(false) {}

void SVGElementResourceClient::ResourceContentChanged(
    InvalidationModeMask invalidation_mask) {
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;

  if (invalidation_mask & SVGResourceClient::kFilterCacheInvalidation)
    InvalidateFilterData();

  if (layout_object->IsSVGResourceContainer()) {
    To<LayoutSVGResourceContainer>(layout_object)->RemoveAllClientsFromCache();
    return;
  }

  if (invalidation_mask & SVGResourceClient::kPaintInvalidation) {
    // Since LayoutSVGInlineTexts don't have SVGResources (they use their
    // parent's), they will not be notified of changes to paint servers. So
    // if the client is one that could have a LayoutSVGInlineText use a
    // paint invalidation reason that will force paint invalidation of the
    // entire <text>/<tspan>/... subtree.
    layout_object->SetSubtreeShouldDoFullPaintInvalidation(
        PaintInvalidationReason::kSVGResource);
  }

  if (invalidation_mask & SVGResourceClient::kClipCacheInvalidation)
    layout_object->InvalidateClipPathCache();

  // Invalidate paint properties to update effects if any.
  if (invalidation_mask & SVGResourceClient::kPaintPropertiesInvalidation)
    layout_object->SetNeedsPaintPropertyUpdate();

  if (invalidation_mask & SVGResourceClient::kBoundariesInvalidation)
    layout_object->SetNeedsBoundariesUpdate();

  bool needs_layout =
      invalidation_mask & SVGResourceClient::kLayoutInvalidation;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      *layout_object, needs_layout);
}

void SVGElementResourceClient::ResourceElementChanged() {
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;
  // TODO(fs): If the resource element (for a filter) doesn't actually change
  // we don't need to perform the associated invalidations.
  InvalidateFilterData();
  if (layout_object->Parent()) {
    SVGResourcesCache::UpdateResources(*layout_object);
    LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
        *layout_object, true);
  }
}

void SVGElementResourceClient::ResourceDestroyed(
    LayoutSVGResourceContainer* resource) {
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(*layout_object);
  if (resources)
    resources->ResourceDestroyed(resource);
}

void SVGElementResourceClient::FilterPrimitiveChanged(
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  if (filter_data_ && !filter_data_->Invalidate(primitive, attribute))
    return;  // No change
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;
  layout_object->SetNeedsPaintPropertyUpdate();
  MarkFilterDataDirty();
}

static FilterData* CreateFilterDataWithNodeMap(
    FilterEffectBuilder& builder,
    const ReferenceFilterOperation& reference_filter) {
  auto* node_map = MakeGarbageCollected<SVGFilterGraphNodeMap>();
  Filter* filter =
      builder.BuildReferenceFilter(reference_filter, nullptr, node_map);
  if (!filter || !filter->LastEffect())
    return nullptr;
  paint_filter_builder::PopulateSourceGraphicImageFilters(
      filter->GetSourceGraphic(), kInterpolationSpaceSRGB);
  return MakeGarbageCollected<FilterData>(filter->LastEffect(), node_map);
}

void SVGElementResourceClient::UpdateFilterData(
    CompositorFilterOperations& operations) {
  DCHECK(element_->GetLayoutObject());
  const LayoutObject& object = *element_->GetLayoutObject();
  FloatRect reference_box = SVGResources::ReferenceBoxForEffects(object);
  if (!operations.IsEmpty() && !filter_data_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;
  if (!filter_data_ && GetFilterResourceForSVG(object.StyleRef())) {
    FilterEffectBuilder builder(reference_box, 1);
    filter_data_ = CreateFilterDataWithNodeMap(
        builder,
        To<ReferenceFilterOperation>(*object.StyleRef().Filter().at(0)));
  }
  operations.Clear();
  if (filter_data_) {
    operations.AppendReferenceFilter(filter_data_->BuildPaintFilter());
  } else {
    // Filter construction failed. Create a filter chain that yields
    // transparent black.
    operations.AppendOpacityFilter(0);
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
  LayoutObject* layout_object = element_->GetLayoutObject();
  layout_object->SetNeedsPaintPropertyUpdate();
  MarkFilterDataDirty();
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
    : resources_(SVGResourcesCache::CachedResourcesForLayoutObject(object)),
      object_(object) {}

void SVGResourceInvalidator::InvalidateEffects() {
  if (!resources_)
    return;
  if (resources_->Filter())
    SVGResources::GetClient(object_)->InvalidateFilterData();
  if (resources_->Clipper()) {
    object_.SetShouldDoFullPaintInvalidation();
    object_.InvalidateClipPathCache();
  }
  if (resources_->Masker()) {
    object_.SetShouldDoFullPaintInvalidation();
    object_.SetNeedsPaintPropertyUpdate();
  }
}

void SVGResourceInvalidator::InvalidatePaints() {
  if (!resources_)
    return;
  bool needs_invalidation = false;
  SVGElementResourceClient* client = SVGResources::GetClient(object_);
  if (LayoutSVGResourcePaintServer* fill = resources_->Fill()) {
    fill->RemoveClientFromCache(*client);
    needs_invalidation = true;
  }
  if (LayoutSVGResourcePaintServer* stroke = resources_->Stroke()) {
    stroke->RemoveClientFromCache(*client);
    needs_invalidation = true;
  }
  if (!needs_invalidation)
    return;
  object_.SetSubtreeShouldDoFullPaintInvalidation(
      PaintInvalidationReason::kSVGResource);
}

}  // namespace blink
