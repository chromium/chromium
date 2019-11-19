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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_pattern_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/core/svg_names.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

SVGResources::SVGResources() : linked_resource_(nullptr) {}

SVGResourceClient* SVGResources::GetClient(const LayoutObject& object) {
  return To<SVGElement>(object.GetNode())->GetSVGResourceClient();
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

namespace {

template <typename ContainerType>
bool IsResourceOfType(LayoutSVGResourceContainer* container) {
  return container->ResourceType() == ContainerType::kResourceType;
}

template <>
bool IsResourceOfType<LayoutSVGResourcePaintServer>(
    LayoutSVGResourceContainer* container) {
  return container->IsSVGPaintServer();
}

template <typename ContainerType>
ContainerType* CastResource(SVGResource* resource) {
  if (!resource)
    return nullptr;
  if (LayoutSVGResourceContainer* container = resource->ResourceContainer()) {
    if (IsResourceOfType<ContainerType>(container))
      return static_cast<ContainerType*>(container);
  }
  return nullptr;
}

template <typename ContainerType>
ContainerType* CastResource(StyleSVGResource& style_resource) {
  return CastResource<ContainerType>(style_resource.Resource());
}

}  // namespace

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
      ClipPathOperation* clip_path_operation = computed_style.ClipPath();
      if (clip_path_operation->GetType() == ClipPathOperation::REFERENCE) {
        const ReferenceClipPathOperation& clip_path_reference =
            To<ReferenceClipPathOperation>(*clip_path_operation);
        EnsureResources(resources).SetClipper(
            CastResource<LayoutSVGResourceClipper>(
                clip_path_reference.Resource()));
      }
    }

    if (computed_style.HasFilter() && !object.IsSVGRoot()) {
      const FilterOperations& filter_operations = computed_style.Filter();
      if (filter_operations.size() == 1) {
        const FilterOperation& filter_operation = *filter_operations.at(0);
        if (const auto* reference_filter_operation =
                DynamicTo<ReferenceFilterOperation>(filter_operation)) {
          EnsureResources(resources).SetFilter(
              CastResource<LayoutSVGResourceFilter>(
                  reference_filter_operation->Resource()));
        }
      }
    }

    if (StyleSVGResource* masker_resource = style.MaskerResource()) {
      EnsureResources(resources).SetMasker(
          CastResource<LayoutSVGResourceMasker>(*masker_resource));
    }
  }

  if (style.HasMarkers() && SupportsMarkers(element)) {
    if (StyleSVGResource* marker_start_resource = style.MarkerStartResource()) {
      EnsureResources(resources).SetMarkerStart(
          CastResource<LayoutSVGResourceMarker>(*marker_start_resource));
    }
    if (StyleSVGResource* marker_mid_resource = style.MarkerMidResource()) {
      EnsureResources(resources).SetMarkerMid(
          CastResource<LayoutSVGResourceMarker>(*marker_mid_resource));
    }
    if (StyleSVGResource* marker_end_resource = style.MarkerEndResource()) {
      EnsureResources(resources).SetMarkerEnd(
          CastResource<LayoutSVGResourceMarker>(*marker_end_resource));
    }
  }

  if (FillAndStrokeTags().Contains(tag_name)) {
    if (StyleSVGResource* fill_resource = style.FillPaint().Resource()) {
      EnsureResources(resources).SetFill(
          CastResource<LayoutSVGResourcePaintServer>(*fill_resource));
    }

    if (StyleSVGResource* stroke_resource = style.StrokePaint().Resource()) {
      EnsureResources(resources).SetStroke(
          CastResource<LayoutSVGResourcePaintServer>(*stroke_resource));
    }
  }

  if (auto* pattern = ToSVGPatternElementOrNull(element)) {
    const SVGPatternElement* directly_referenced_pattern =
        pattern->ReferencedElement();
    if (directly_referenced_pattern) {
      EnsureResources(resources).SetLinkedResource(
          ToLayoutSVGResourceContainerOrNull(
              directly_referenced_pattern->GetLayoutObject()));
    }
  }

  return (!resources || !resources->HasResourceData()) ? nullptr
                                                       : std::move(resources);
}

void SVGResources::LayoutIfNeeded() {
  if (clipper_filter_masker_data_) {
    if (LayoutSVGResourceClipper* clipper =
            clipper_filter_masker_data_->clipper)
      clipper->LayoutIfNeeded();
    if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
      masker->LayoutIfNeeded();
    if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter)
      filter->LayoutIfNeeded();
  }

  if (marker_data_) {
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_start)
      marker->LayoutIfNeeded();
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_mid)
      marker->LayoutIfNeeded();
    if (LayoutSVGResourceMarker* marker = marker_data_->marker_end)
      marker->LayoutIfNeeded();
  }

  if (fill_stroke_data_) {
    if (LayoutSVGResourcePaintServer* fill = fill_stroke_data_->fill)
      fill->LayoutIfNeeded();
    if (LayoutSVGResourcePaintServer* stroke = fill_stroke_data_->stroke)
      stroke->LayoutIfNeeded();
  }

  if (linked_resource_)
    linked_resource_->LayoutIfNeeded();
}

InvalidationModeMask SVGResources::RemoveClientFromCacheAffectingObjectBounds(
    SVGResourceClient& client) const {
  if (!clipper_filter_masker_data_)
    return 0;
  InvalidationModeMask invalidation_flags = 0;
  if (LayoutSVGResourceClipper* clipper = clipper_filter_masker_data_->clipper)
    clipper->RemoveClientFromCache(client);
  if (LayoutSVGResourceFilter* filter = clipper_filter_masker_data_->filter) {
    if (filter->RemoveClientFromCache(client))
      invalidation_flags |= SVGResourceClient::kPaintInvalidation;
  }
  if (LayoutSVGResourceMasker* masker = clipper_filter_masker_data_->masker)
    masker->RemoveClientFromCache(client);
  return invalidation_flags | SVGResourceClient::kBoundariesInvalidation;
}

InvalidationModeMask SVGResources::RemoveClientFromCache(
    SVGResourceClient& client) const {
  if (!HasResourceData())
    return 0;

  // We never call this method for the elements where this would be non-null.
  DCHECK(!linked_resource_);

  InvalidationModeMask invalidation_flags =
      RemoveClientFromCacheAffectingObjectBounds(client);

  if (fill_stroke_data_) {
    if (LayoutSVGResourcePaintServer* fill = fill_stroke_data_->fill)
      fill->RemoveClientFromCache(client);
    if (LayoutSVGResourcePaintServer* stroke = fill_stroke_data_->stroke)
      stroke->RemoveClientFromCache(client);
    invalidation_flags |= SVGResourceClient::kPaintInvalidation;
  }

  return invalidation_flags;
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
  if (!clipper)
    return;

  DCHECK_EQ(clipper->ResourceType(), kClipperResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->clipper = clipper;
}

void SVGResources::SetFilter(LayoutSVGResourceFilter* filter) {
  if (!filter)
    return;

  DCHECK_EQ(filter->ResourceType(), kFilterResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->filter = filter;
}

void SVGResources::SetMarkerStart(LayoutSVGResourceMarker* marker_start) {
  if (!marker_start)
    return;

  DCHECK_EQ(marker_start->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_start = marker_start;
}

void SVGResources::SetMarkerMid(LayoutSVGResourceMarker* marker_mid) {
  if (!marker_mid)
    return;

  DCHECK_EQ(marker_mid->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_mid = marker_mid;
}

void SVGResources::SetMarkerEnd(LayoutSVGResourceMarker* marker_end) {
  if (!marker_end)
    return;

  DCHECK_EQ(marker_end->ResourceType(), kMarkerResourceType);

  if (!marker_data_)
    marker_data_ = std::make_unique<MarkerData>();

  marker_data_->marker_end = marker_end;
}

void SVGResources::SetMasker(LayoutSVGResourceMasker* masker) {
  if (!masker)
    return;

  DCHECK_EQ(masker->ResourceType(), kMaskerResourceType);

  if (!clipper_filter_masker_data_)
    clipper_filter_masker_data_ = std::make_unique<ClipperFilterMaskerData>();

  clipper_filter_masker_data_->masker = masker;
}

void SVGResources::SetFill(LayoutSVGResourcePaintServer* fill) {
  if (!fill)
    return;

  if (!fill_stroke_data_)
    fill_stroke_data_ = std::make_unique<FillStrokeData>();

  fill_stroke_data_->fill = fill;
}

void SVGResources::SetStroke(LayoutSVGResourcePaintServer* stroke) {
  if (!stroke)
    return;

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
  if (style.HasFilter())
    style.Filter().AddClient(element.EnsureSVGResourceClient());
  if (StyleSVGResource* masker_resource = style.SvgStyle().MaskerResource())
    masker_resource->AddClient(element.EnsureSVGResourceClient());
  if (had_client)
    ClearClipPathFilterMask(element, old_style);
}

void SVGResources::ClearClipPathFilterMask(SVGElement& element,
                                           const ComputedStyle* style) {
  if (!style)
    return;
  SVGResourceClient* client = element.GetSVGResourceClient();
  if (!client)
    return;
  if (auto* old_reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style->ClipPath()))
    old_reference_clip->RemoveClient(*client);
  if (style->HasFilter())
    style->Filter().RemoveClient(*client);
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

SVGElementResourceClient::SVGElementResourceClient(SVGElement* element)
    : element_(element) {}

void SVGElementResourceClient::ResourceContentChanged(
    InvalidationModeMask invalidation_mask) {
  LayoutObject* layout_object = element_->GetLayoutObject();
  if (!layout_object)
    return;
  bool mark_for_invalidation =
      invalidation_mask & ~SVGResourceClient::kParentOnlyInvalidation;
  if (layout_object->IsSVGResourceContainer()) {
    ToLayoutSVGResourceContainer(layout_object)
        ->RemoveAllClientsFromCache(mark_for_invalidation);
    return;
  }

  if (mark_for_invalidation) {
    LayoutSVGResourceContainer::MarkClientForInvalidation(*layout_object,
                                                          invalidation_mask);
  }

  // Special case for filter invalidation.
  if (invalidation_mask & SVGResourceClient::kSkipAncestorInvalidation)
    return;

  bool needs_layout =
      invalidation_mask & SVGResourceClient::kLayoutInvalidation;
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      *layout_object, needs_layout);
}

void SVGElementResourceClient::ResourceElementChanged() {
  if (LayoutObject* layout_object = element_->GetLayoutObject())
    SVGResourcesCache::ResourceReferenceChanged(*layout_object);
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

void SVGElementResourceClient::Trace(Visitor* visitor) {
  visitor->Trace(element_);
  SVGResourceClient::Trace(visitor);
}

}  // namespace blink
