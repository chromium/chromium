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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_RESOURCES_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
class FilterEffect;
class LayoutObject;
class LayoutSVGResourceClipper;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMarker;
class LayoutSVGResourceMasker;
class LayoutSVGResourcePaintServer;
class SVGElement;
class SVGElementResourceClient;
class SVGFilterGraphNodeMap;

// Holds a set of resources associated with a LayoutObject
class SVGResources {
  USING_FAST_MALLOC(SVGResources);

 public:
  SVGResources();
  SVGResources(const SVGResources&) = delete;
  SVGResources& operator=(const SVGResources&) = delete;

  static SVGElementResourceClient* GetClient(const LayoutObject&);
  static FloatRect ReferenceBoxForEffects(const LayoutObject&);

  static std::unique_ptr<SVGResources> BuildResources(const LayoutObject&,
                                                      const ComputedStyle&);

  static void UpdateClipPathFilterMask(SVGElement&,
                                       const ComputedStyle* old_style,
                                       const ComputedStyle&);
  static void ClearClipPathFilterMask(SVGElement&, const ComputedStyle*);
  static void UpdatePaints(SVGElement&,
                           const ComputedStyle* old_style,
                           const ComputedStyle&);
  static void ClearPaints(SVGElement&, const ComputedStyle*);
  static void UpdateMarkers(SVGElement&,
                            const ComputedStyle* old_style,
                            const ComputedStyle&);
  static void ClearMarkers(SVGElement&, const ComputedStyle*);

  static bool SupportsMarkers(const SVGElement&);

  // Ordinary resources
  LayoutSVGResourceClipper* Clipper() const {
    return clipper_filter_masker_data_ ? clipper_filter_masker_data_->clipper
                                       : nullptr;
  }
  LayoutSVGResourceMarker* MarkerStart() const {
    return marker_data_ ? marker_data_->marker_start : nullptr;
  }
  LayoutSVGResourceMarker* MarkerMid() const {
    return marker_data_ ? marker_data_->marker_mid : nullptr;
  }
  LayoutSVGResourceMarker* MarkerEnd() const {
    return marker_data_ ? marker_data_->marker_end : nullptr;
  }
  LayoutSVGResourceMasker* Masker() const {
    return clipper_filter_masker_data_ ? clipper_filter_masker_data_->masker
                                       : nullptr;
  }

  LayoutSVGResourceFilter* Filter() const {
    if (clipper_filter_masker_data_)
      return clipper_filter_masker_data_->filter;
    return nullptr;
  }

  bool HasClipOrMaskOrFilter() const { return !!clipper_filter_masker_data_; }

  // Paint servers
  LayoutSVGResourcePaintServer* Fill() const {
    return fill_stroke_data_ ? fill_stroke_data_->fill : nullptr;
  }
  LayoutSVGResourcePaintServer* Stroke() const {
    return fill_stroke_data_ ? fill_stroke_data_->stroke : nullptr;
  }

  // Chainable resources - linked through xlink:href
  LayoutSVGResourceContainer* LinkedResource() const {
    return linked_resource_;
  }

  void BuildSetOfResources(HashSet<LayoutSVGResourceContainer*>&);

  // Methods operating on all cached resources
  void ResourceDestroyed(LayoutSVGResourceContainer*);
  void ClearReferencesTo(LayoutSVGResourceContainer*);

#if DCHECK_IS_ON()
  void Dump(const LayoutObject*);
#endif

 private:
  bool HasResourceData() const;

  void SetClipper(LayoutSVGResourceClipper*);
  void SetFilter(LayoutSVGResourceFilter*);
  void SetMarkerStart(LayoutSVGResourceMarker*);
  void SetMarkerMid(LayoutSVGResourceMarker*);
  void SetMarkerEnd(LayoutSVGResourceMarker*);
  void SetMasker(LayoutSVGResourceMasker*);
  void SetFill(LayoutSVGResourcePaintServer*);
  void SetStroke(LayoutSVGResourcePaintServer*);
  void SetLinkedResource(LayoutSVGResourceContainer*);

  // From SVG 1.1 2nd Edition
  // clipper: 'container elements' and 'graphics elements'
  // filter:  'container elements' and 'graphics elements'
  // masker:  'container elements' and 'graphics elements'
  // -> a, circle, defs, ellipse, glyph, g, image, line, marker, mask,
  // missing-glyph, path, pattern, polygon, polyline, rect, svg, switch, symbol,
  // text, use
  struct ClipperFilterMaskerData {
    USING_FAST_MALLOC(ClipperFilterMaskerData);

   public:
    ClipperFilterMaskerData()
        : clipper(nullptr), filter(nullptr), masker(nullptr) {}

    LayoutSVGResourceClipper* clipper;
    LayoutSVGResourceFilter* filter;
    LayoutSVGResourceMasker* masker;
  };

  // From SVG 1.1 2nd Edition
  // marker: line, path, polygon, polyline
  struct MarkerData {
    USING_FAST_MALLOC(MarkerData);

   public:
    MarkerData()
        : marker_start(nullptr), marker_mid(nullptr), marker_end(nullptr) {}

    LayoutSVGResourceMarker* marker_start;
    LayoutSVGResourceMarker* marker_mid;
    LayoutSVGResourceMarker* marker_end;
  };

  // From SVG 1.1 2nd Edition
  // fill:       'shapes' and 'text content elements'
  // stroke:     'shapes' and 'text content elements'
  // -> circle, ellipse, line, path, polygon, polyline, rect, text, textPath,
  // tspan
  struct FillStrokeData {
    USING_FAST_MALLOC(FillStrokeData);

   public:
    FillStrokeData() : fill(nullptr), stroke(nullptr) {}

    LayoutSVGResourcePaintServer* fill;
    LayoutSVGResourcePaintServer* stroke;
  };

  std::unique_ptr<ClipperFilterMaskerData> clipper_filter_masker_data_;
  std::unique_ptr<MarkerData> marker_data_;
  std::unique_ptr<FillStrokeData> fill_stroke_data_;
  LayoutSVGResourceContainer* linked_resource_;
};

class FilterData final : public GarbageCollected<FilterData> {
 public:
  FilterData(FilterEffect* last_effect, SVGFilterGraphNodeMap* node_map)
      : last_effect_(last_effect), node_map_(node_map) {}

  sk_sp<PaintFilter> BuildPaintFilter();
  // Perform a finegrained invalidation of the filter chain for the
  // specified filter primitive and attribute. Returns false if no
  // further invalidation is required, otherwise true.
  bool Invalidate(SVGFilterPrimitiveStandardAttributes& primitive,
                  const QualifiedName& attribute);

  void Dispose();

  void Trace(Visitor*) const;

 private:
  Member<FilterEffect> last_effect_;
  Member<SVGFilterGraphNodeMap> node_map_;
};

class SVGElementResourceClient final
    : public GarbageCollected<SVGElementResourceClient>,
      public SVGResourceClient {
 public:
  explicit SVGElementResourceClient(SVGElement*);

  void ResourceContentChanged(InvalidationModeMask) override;
  void ResourceElementChanged() override;
  void ResourceDestroyed(LayoutSVGResourceContainer*) override;

  void FilterPrimitiveChanged(SVGFilterPrimitiveStandardAttributes& primitive,
                              const QualifiedName& attribute) override;

  void UpdateFilterData(CompositorFilterOperations&);
  void InvalidateFilterData();
  bool ClearFilterData();
  void MarkFilterDataDirty();

  void Trace(Visitor*) const override;

 private:
  Member<SVGElement> element_;
  Member<FilterData> filter_data_;
  bool filter_data_dirty_;
};

// Helper class for handling invalidation of resources (generally after the
// reference box of a LayoutObject may have changed).
class SVGResourceInvalidator {
  STACK_ALLOCATED();

 public:
  explicit SVGResourceInvalidator(LayoutObject& object);

  // Invalidate any associated clip-path/mask/filter.
  void InvalidateEffects();

  // Invalidate any associated paints (fill/stroke).
  void InvalidatePaints();

 private:
  const SVGResources* resources_;
  LayoutObject& object_;
};

}  // namespace blink

#endif
