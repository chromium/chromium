/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2005, 2006 Apple Computer, Inc.
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/data_ref.h"
#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class StyleDifference;

// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class SVGComputedStyle : public RefCounted<SVGComputedStyle> {
  USING_FAST_MALLOC(SVGComputedStyle);

 public:
  static scoped_refptr<SVGComputedStyle> Create() {
    return base::AdoptRef(new SVGComputedStyle);
  }
  scoped_refptr<SVGComputedStyle> Copy() const {
    return base::AdoptRef(new SVGComputedStyle(*this));
  }
  CORE_EXPORT ~SVGComputedStyle();

  bool InheritedEqual(const SVGComputedStyle&) const;
  void InheritFrom(const SVGComputedStyle&);

  CORE_EXPORT StyleDifference Diff(const SVGComputedStyle&) const;

  bool operator==(const SVGComputedStyle&) const;
  bool operator!=(const SVGComputedStyle& o) const { return !(*this == o); }

  // Initial values for all the properties
  static EDominantBaseline InitialDominantBaseline() { return DB_AUTO; }
  static LineCap InitialCapStyle() { return kButtCap; }
  static WindRule InitialClipRule() { return RULE_NONZERO; }
  static EColorInterpolation InitialColorInterpolation() { return CI_SRGB; }
  static EColorInterpolation InitialColorInterpolationFilters() {
    return CI_LINEARRGB;
  }
  static EColorRendering InitialColorRendering() { return CR_AUTO; }
  static WindRule InitialFillRule() { return RULE_NONZERO; }
  static LineJoin InitialJoinStyle() { return kMiterJoin; }
  static EShapeRendering InitialShapeRendering() { return SR_AUTO; }
  static ETextAnchor InitialTextAnchor() { return TA_START; }
  static float InitialFillOpacity() { return 1; }
  static SVGPaint InitialFillPaint() { return SVGPaint(Color::kBlack); }
  static float InitialStrokeOpacity() { return 1; }
  static SVGPaint InitialStrokePaint() { return SVGPaint(); }
  static scoped_refptr<SVGDashArray> InitialStrokeDashArray();
  static Length InitialStrokeDashOffset() { return Length::Fixed(); }
  static float InitialStrokeMiterLimit() { return 4; }
  static UnzoomedLength InitialStrokeWidth() {
    return UnzoomedLength(Length::Fixed(1));
  }
  static StyleSVGResource* InitialMarkerStartResource() { return nullptr; }
  static StyleSVGResource* InitialMarkerMidResource() { return nullptr; }
  static StyleSVGResource* InitialMarkerEndResource() { return nullptr; }
  static EPaintOrder InitialPaintOrder() { return kPaintOrderNormal; }

  // SVG CSS Property setters
  void SetDominantBaseline(EDominantBaseline val) {
    svg_inherited_flags.dominant_baseline = val;
  }
  void SetCapStyle(LineCap val) { svg_inherited_flags.cap_style = val; }
  void SetClipRule(WindRule val) { svg_inherited_flags.clip_rule = val; }
  void SetColorInterpolation(EColorInterpolation val) {
    svg_inherited_flags.color_interpolation = val;
  }
  void SetColorInterpolationFilters(EColorInterpolation val) {
    svg_inherited_flags.color_interpolation_filters = val;
  }
  void SetColorRendering(EColorRendering val) {
    svg_inherited_flags.color_rendering = val;
  }
  void SetFillRule(WindRule val) { svg_inherited_flags.fill_rule = val; }
  void SetJoinStyle(LineJoin val) { svg_inherited_flags.join_style = val; }
  void SetShapeRendering(EShapeRendering val) {
    svg_inherited_flags.shape_rendering = val;
  }
  void SetTextAnchor(ETextAnchor val) { svg_inherited_flags.text_anchor = val; }
  void SetPaintOrder(EPaintOrder val) {
    svg_inherited_flags.paint_order = (int)val;
  }
  void SetFillOpacity(float obj) {
    if (!(fill->opacity == obj))
      fill.Access()->opacity = obj;
  }

  void SetFillPaint(const SVGPaint& paint) {
    if (!(fill->paint == paint))
      fill.Access()->paint = paint;
  }

  void SetInternalVisitedFillPaint(const SVGPaint& paint) {
    if (!(fill->visited_link_paint == paint))
      fill.Access()->visited_link_paint = paint;
  }

  void SetStrokeOpacity(float obj) {
    if (!(stroke->opacity == obj))
      stroke.Access()->opacity = obj;
  }

  void SetStrokePaint(const SVGPaint& paint) {
    if (!(stroke->paint == paint))
      stroke.Access()->paint = paint;
  }

  void SetInternalVisitedStrokePaint(const SVGPaint& paint) {
    if (!(stroke->visited_link_paint == paint))
      stroke.Access()->visited_link_paint = paint;
  }

  void SetStrokeDashArray(scoped_refptr<SVGDashArray> dash_array) {
    if (stroke->dash_array->data != dash_array->data)
      stroke.Access()->dash_array = std::move(dash_array);
  }

  void SetStrokeMiterLimit(float obj) {
    if (!(stroke->miter_limit == obj))
      stroke.Access()->miter_limit = obj;
  }

  void SetStrokeWidth(const UnzoomedLength& stroke_width) {
    if (!(stroke->width == stroke_width))
      stroke.Access()->width = stroke_width;
  }

  void SetStrokeDashOffset(const Length& dash_offset) {
    if (!(stroke->dash_offset == dash_offset))
      stroke.Access()->dash_offset = dash_offset;
  }

  // Setters for inherited resources
  void SetMarkerStartResource(scoped_refptr<StyleSVGResource> resource);

  void SetMarkerMidResource(scoped_refptr<StyleSVGResource> resource);

  void SetMarkerEndResource(scoped_refptr<StyleSVGResource> resource);

  // Read accessors for all the properties
  EDominantBaseline DominantBaseline() const {
    return (EDominantBaseline)svg_inherited_flags.dominant_baseline;
  }
  LineCap CapStyle() const { return (LineCap)svg_inherited_flags.cap_style; }
  WindRule ClipRule() const { return (WindRule)svg_inherited_flags.clip_rule; }
  EColorInterpolation ColorInterpolation() const {
    return (EColorInterpolation)svg_inherited_flags.color_interpolation;
  }
  EColorInterpolation ColorInterpolationFilters() const {
    return (EColorInterpolation)svg_inherited_flags.color_interpolation_filters;
  }
  EColorRendering ColorRendering() const {
    return (EColorRendering)svg_inherited_flags.color_rendering;
  }
  WindRule FillRule() const { return (WindRule)svg_inherited_flags.fill_rule; }
  LineJoin JoinStyle() const {
    return (LineJoin)svg_inherited_flags.join_style;
  }
  EShapeRendering ShapeRendering() const {
    return (EShapeRendering)svg_inherited_flags.shape_rendering;
  }
  ETextAnchor TextAnchor() const {
    return (ETextAnchor)svg_inherited_flags.text_anchor;
  }
  float FillOpacity() const { return fill->opacity; }
  const SVGPaint& FillPaint() const { return fill->paint; }
  float StrokeOpacity() const { return stroke->opacity; }
  const SVGPaint& StrokePaint() const { return stroke->paint; }
  SVGDashArray* StrokeDashArray() const { return stroke->dash_array.get(); }
  float StrokeMiterLimit() const { return stroke->miter_limit; }
  const UnzoomedLength& StrokeWidth() const { return stroke->width; }
  const Length& StrokeDashOffset() const { return stroke->dash_offset; }
  StyleSVGResource* MarkerStartResource() const {
    return inherited_resources->marker_start.get();
  }
  StyleSVGResource* MarkerMidResource() const {
    return inherited_resources->marker_mid.get();
  }
  StyleSVGResource* MarkerEndResource() const {
    return inherited_resources->marker_end.get();
  }
  EPaintOrder PaintOrder() const {
    return (EPaintOrder)svg_inherited_flags.paint_order;
  }

  const SVGPaint& InternalVisitedFillPaint() const {
    return fill->visited_link_paint;
  }
  const SVGPaint& InternalVisitedStrokePaint() const {
    return stroke->visited_link_paint;
  }

 protected:
  // inherit
  struct InheritedFlags {
    bool operator==(const InheritedFlags& other) const {
      return (color_rendering == other.color_rendering) &&
             (shape_rendering == other.shape_rendering) &&
             (clip_rule == other.clip_rule) && (fill_rule == other.fill_rule) &&
             (cap_style == other.cap_style) &&
             (join_style == other.join_style) &&
             (text_anchor == other.text_anchor) &&
             (color_interpolation == other.color_interpolation) &&
             (color_interpolation_filters ==
              other.color_interpolation_filters) &&
             (paint_order == other.paint_order) &&
             (dominant_baseline == other.dominant_baseline);
    }

    bool operator!=(const InheritedFlags& other) const {
      return !(*this == other);
    }

    unsigned color_rendering : 2;              // EColorRendering
    unsigned shape_rendering : 2;              // EShapeRendering
    unsigned clip_rule : 1;                    // WindRule
    unsigned fill_rule : 1;                    // WindRule
    unsigned cap_style : 2;                    // LineCap
    unsigned join_style : 2;                   // LineJoin
    unsigned text_anchor : 2;                  // ETextAnchor
    unsigned color_interpolation : 2;          // EColorInterpolation
    unsigned color_interpolation_filters : 2;  // EColorInterpolation_
    unsigned paint_order : 3;                  // EPaintOrder
    unsigned dominant_baseline : 4;            // EDominantBaseline
  } svg_inherited_flags;

  // inherited attributes
  DataRef<StyleFillData> fill;
  DataRef<StyleStrokeData> stroke;
  DataRef<StyleInheritedResourceData> inherited_resources;

 private:
  enum CreateInitialType { kCreateInitial };

  CORE_EXPORT SVGComputedStyle();
  SVGComputedStyle(const SVGComputedStyle&);
  SVGComputedStyle(
      CreateInitialType);  // Used to create the initial style singleton.

  bool DiffNeedsLayoutAndPaintInvalidation(const SVGComputedStyle& other) const;
  bool DiffNeedsPaintInvalidation(const SVGComputedStyle& other) const;

  void SetBitDefaults() {
    svg_inherited_flags.clip_rule = InitialClipRule();
    svg_inherited_flags.color_rendering = InitialColorRendering();
    svg_inherited_flags.fill_rule = InitialFillRule();
    svg_inherited_flags.shape_rendering = InitialShapeRendering();
    svg_inherited_flags.text_anchor = InitialTextAnchor();
    svg_inherited_flags.cap_style = InitialCapStyle();
    svg_inherited_flags.join_style = InitialJoinStyle();
    svg_inherited_flags.color_interpolation = InitialColorInterpolation();
    svg_inherited_flags.color_interpolation_filters =
        InitialColorInterpolationFilters();
    svg_inherited_flags.paint_order = InitialPaintOrder();
    svg_inherited_flags.dominant_baseline = InitialDominantBaseline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SVG_COMPUTED_STYLE_H_
