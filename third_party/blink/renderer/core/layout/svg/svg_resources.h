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

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class LayoutSVGResourceClipper;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMarker;
class LayoutSVGResourceMasker;
class LayoutSVGResourcePaintServer;
class SVGElement;

// Holds a set of resources associated with a LayoutObject
class SVGResources {
  USING_FAST_MALLOC(SVGResources);

 public:
  SVGResources();

  static SVGResourceClient* GetClient(const LayoutObject&);

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

  void LayoutIfNeeded();

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
  InvalidationModeMask RemoveClientFromCache(SVGResourceClient&) const;
  InvalidationModeMask RemoveClientFromCacheAffectingObjectBounds(
      SVGResourceClient&) const;
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
  DISALLOW_COPY_AND_ASSIGN(SVGResources);
};

class SVGElementResourceClient final
    : public GarbageCollected<SVGElementResourceClient>,
      public SVGResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(SVGElementResourceClient);

 public:
  explicit SVGElementResourceClient(SVGElement*);

  void ResourceContentChanged(InvalidationModeMask) override;
  void ResourceElementChanged() override;
  void ResourceDestroyed(LayoutSVGResourceContainer*) override;

  void Trace(Visitor*) override;

 private:
  Member<SVGElement> element_;
};

}  // namespace blink

#endif
