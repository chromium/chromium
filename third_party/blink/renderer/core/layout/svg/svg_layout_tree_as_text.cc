/*
 * Copyright (C) 2004, 2005, 2007, 2009 Apple Inc. All rights reserved.
 *           (C) 2005 Rob Buis <buis@kde.org>
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/svg/svg_layout_tree_as_text.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_linear_gradient.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_pattern.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_radial_gradient.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/linear_gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/pattern_attributes.h"
#include "third_party/blink/renderer/core/svg/radial_gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_animated_angle.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_point_list.h"
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/core/svg/svg_line_element.h"
#include "third_party/blink/renderer/core/svg/svg_linear_gradient_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_pattern_element.h"
#include "third_party/blink/renderer/core/svg/svg_point_list.h"
#include "third_party/blink/renderer/core/svg/svg_poly_element.h"
#include "third_party/blink/renderer/core/svg/svg_radial_gradient_element.h"
#include "third_party/blink/renderer/core/svg/svg_rect_element.h"
#include "third_party/blink/renderer/platform/graphics/dash_array.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

/** class + iomanip to help streaming list separators, i.e. ", " in string "a,
 * b, c, d"
 * Can be used in cases where you don't know which item in the list is the first
 * one to be printed, but still want to avoid strings like ", b, c".
 */
class TextStreamSeparator {
 public:
  TextStreamSeparator(const String& s)
      : separator_(s), need_to_separate_(false) {}

 private:
  friend WTF::TextStream& operator<<(WTF::TextStream&, TextStreamSeparator&);

  String separator_;
  bool need_to_separate_;
};

WTF::TextStream& operator<<(WTF::TextStream& ts, TextStreamSeparator& sep) {
  if (sep.need_to_separate_)
    ts << sep.separator_;
  else
    sep.need_to_separate_ = true;
  return ts;
}

template <typename ValueType>
static void WriteNameValuePair(WTF::TextStream& ts,
                               const char* name,
                               const ValueType& value) {
  ts << " [" << name << "=" << value << "]";
}

static void WriteSVGResourceIfNotNull(WTF::TextStream& ts,
                                      const char* name,
                                      const StyleSVGResource* value,
                                      TreeScope& tree_scope) {
  if (!value)
    return;
  AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
      value->Url(), tree_scope);
  WriteNameValuePair(ts, name, id);
}

template <typename ValueType>
static void WriteNameAndQuotedValue(WTF::TextStream& ts,
                                    const char* name,
                                    ValueType value) {
  ts << " [" << name << "=\"" << value << "\"]";
}

template <typename ValueType>
static void WriteIfNotDefault(WTF::TextStream& ts,
                              const char* name,
                              ValueType value,
                              ValueType default_value) {
  if (value != default_value)
    WriteNameValuePair(ts, name, value);
}

WTF::TextStream& operator<<(WTF::TextStream& ts,
                            const AffineTransform& transform) {
  if (transform.IsIdentity()) {
    ts << "identity";
  } else {
    ts << "{m=((" << transform.A() << "," << transform.B() << ")("
       << transform.C() << "," << transform.D() << ")) t=(" << transform.E()
       << "," << transform.F() << ")}";
  }

  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts, const WindRule rule) {
  switch (rule) {
    case RULE_NONZERO:
      ts << "NON-ZERO";
      break;
    case RULE_EVENODD:
      ts << "EVEN-ODD";
      break;
  }

  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const SVGUnitTypes::SVGUnitType& unit_type) {
  ts << GetEnumerationMap<SVGUnitTypes::SVGUnitType>().NameFromValue(unit_type);
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const SVGMarkerUnitsType& marker_unit) {
  ts << GetEnumerationMap<SVGMarkerUnitsType>().NameFromValue(marker_unit);
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const SVGMarkerOrientType& orient_type) {
  ts << GetEnumerationMap<SVGMarkerOrientType>().NameFromValue(orient_type);
  return ts;
}

// FIXME: Maybe this should be in platform/graphics/graphics_types.cc
static WTF::TextStream& operator<<(WTF::TextStream& ts, LineCap style) {
  switch (style) {
    case kButtCap:
      ts << "BUTT";
      break;
    case kRoundCap:
      ts << "ROUND";
      break;
    case kSquareCap:
      ts << "SQUARE";
      break;
  }
  return ts;
}

// FIXME: Maybe this should be in platform/graphics/graphics_types.cc
static WTF::TextStream& operator<<(WTF::TextStream& ts, LineJoin style) {
  switch (style) {
    case kMiterJoin:
      ts << "MITER";
      break;
    case kRoundJoin:
      ts << "ROUND";
      break;
    case kBevelJoin:
      ts << "BEVEL";
      break;
  }
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const SVGSpreadMethodType& type) {
  auto* name = GetEnumerationMap<SVGSpreadMethodType>().NameFromValue(type);
  ts << String(name).UpperASCII();
  return ts;
}

static void WriteSVGPaintingResource(WTF::TextStream& ts,
                                     const SVGResource& resource) {
  const LayoutSVGResourceContainer* container =
      resource.ResourceContainerNoCycleCheck();
  DCHECK(container);
  switch (container->ResourceType()) {
    case kPatternResourceType:
      ts << "[type=PATTERN]";
      break;
    case kLinearGradientResourceType:
      ts << "[type=LINEAR-GRADIENT]";
      break;
    case kRadialGradientResourceType:
      ts << "[type=RADIAL-GRADIENT]";
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  ts << " [id=\"" << resource.Target()->GetIdAttribute() << "\"]";
}

static bool WriteSVGPaint(WTF::TextStream& ts,
                          const LayoutObject& object,
                          const SVGPaint& paint,
                          const Longhand& property,
                          const char* paint_name) {
  TextStreamSeparator s(" ");
  const ComputedStyle& style = object.StyleRef();
  if (const StyleSVGResource* resource = paint.Resource()) {
    const SVGResource* paint_resource = resource->Resource();
    SVGResourceClient* client = SVGResources::GetClient(object);
    if (GetSVGResourceAsType<LayoutSVGResourcePaintServer>(*client,
                                                           paint_resource)) {
      ts << " [" << paint_name << "={" << s;
      WriteSVGPaintingResource(ts, *paint_resource);
      return true;
    }
  }
  if (paint.HasColor()) {
    Color color = style.VisitedDependentColor(property);
    ts << " [" << paint_name << "={" << s;
    ts << "[type=SOLID] [color=" << color << "]";
    return true;
  }
  if (paint.type == SVGPaintType::kContextFill) {
    ts << " [" << paint_name << "={" << s;
    ts << "[type=CONTEXT-FILL]";
    return true;
  }
  if (paint.type == SVGPaintType::kContextStroke) {
    ts << " [" << paint_name << "={" << s;
    ts << "[type=CONTEXT-STROKE]";
    return true;
  }
  return false;
}

static void WriteStyle(WTF::TextStream& ts, const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();

  if (!object.LocalSVGTransform().IsIdentity())
    WriteNameValuePair(ts, "transform", object.LocalSVGTransform());
  WriteIfNotDefault(
      ts, "image rendering", static_cast<int>(style.ImageRendering()),
      static_cast<int>(ComputedStyleInitialValues::InitialImageRendering()));
  WriteIfNotDefault(ts, "opacity", style.Opacity(),
                    ComputedStyleInitialValues::InitialOpacity());
  if (object.IsSVGShape()) {
    if (WriteSVGPaint(ts, object, style.StrokePaint(), GetCSSPropertyStroke(),
                      "stroke")) {
      const SVGViewportResolver viewport_resolver(object);
      double dash_offset =
          ValueForLength(style.StrokeDashOffset(), viewport_resolver, style);
      double stroke_width =
          ValueForLength(style.StrokeWidth(), viewport_resolver);
      DashArray dash_array = SVGLayoutSupport::ResolveSVGDashArray(
          *style.StrokeDashArray(), style, viewport_resolver);

      WriteIfNotDefault(ts, "opacity", style.StrokeOpacity(), 1.0f);
      WriteIfNotDefault(ts, "stroke width", stroke_width, 1.0);
      WriteIfNotDefault(ts, "miter limit", style.StrokeMiterLimit(), 4.0f);
      WriteIfNotDefault(ts, "line cap", style.CapStyle(), kButtCap);
      WriteIfNotDefault(ts, "line join", style.JoinStyle(), kMiterJoin);
      WriteIfNotDefault(ts, "dash offset", dash_offset, 0.0);
      if (!dash_array.empty())
        WriteNameValuePair(ts, "dash array", dash_array);

      ts << "}]";
    }

    if (WriteSVGPaint(ts, object, style.FillPaint(), GetCSSPropertyFill(),
                      "fill")) {
      WriteIfNotDefault(ts, "opacity", style.FillOpacity(), 1.0f);
      WriteIfNotDefault(ts, "fill rule", style.FillRule(), RULE_NONZERO);
      ts << "}]";
    }
    WriteIfNotDefault(ts, "clip rule", style.ClipRule(), RULE_NONZERO);
  }

  TreeScope& tree_scope = object.GetDocument();
  WriteSVGResourceIfNotNull(ts, "start marker", style.MarkerStartResource(),
                            tree_scope);
  WriteSVGResourceIfNotNull(ts, "middle marker", style.MarkerMidResource(),
                            tree_scope);
  WriteSVGResourceIfNotNull(ts, "end marker", style.MarkerEndResource(),
                            tree_scope);
}

static WTF::TextStream& WritePositionAndStyle(WTF::TextStream& ts,
                                              const LayoutObject& object) {
  ts << " " << object.ObjectBoundingBox();
  WriteStyle(ts, object);
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const LayoutSVGShape& shape) {
  WritePositionAndStyle(ts, shape);

  SVGElement* svg_element = shape.GetElement();
  DCHECK(svg_element);
  const SVGViewportResolver viewport_resolver(shape);
  const ComputedStyle& style = shape.StyleRef();

  if (IsA<SVGRectElement>(*svg_element)) {
    WriteNameValuePair(ts, "x",
                       ValueForLength(style.X(), viewport_resolver, style,
                                      SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "y",
                       ValueForLength(style.Y(), viewport_resolver, style,
                                      SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "width",
                       ValueForLength(style.Width(), viewport_resolver, style,
                                      SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "height",
                       ValueForLength(style.Height(), viewport_resolver, style,
                                      SVGLengthMode::kHeight));
  } else if (auto* element = DynamicTo<SVGLineElement>(*svg_element)) {
    const SVGLengthContext length_context(svg_element);
    WriteNameValuePair(ts, "x1",
                       element->x1()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "y1",
                       element->y1()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "x2",
                       element->x2()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "y2",
                       element->y2()->CurrentValue()->Value(length_context));
  } else if (IsA<SVGEllipseElement>(*svg_element)) {
    WriteNameValuePair(ts, "cx",
                       ValueForLength(style.Cx(), viewport_resolver, style,
                                      SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "cy",
                       ValueForLength(style.Cy(), viewport_resolver, style,
                                      SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "rx",
                       ValueForLength(style.Rx(), viewport_resolver, style,
                                      SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "ry",
                       ValueForLength(style.Ry(), viewport_resolver, style,
                                      SVGLengthMode::kHeight));
  } else if (IsA<SVGCircleElement>(*svg_element)) {
    WriteNameValuePair(ts, "cx",
                       ValueForLength(style.Cx(), viewport_resolver, style,
                                      SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "cy",
                       ValueForLength(style.Cy(), viewport_resolver, style,
                                      SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "r",
                       ValueForLength(style.R(), viewport_resolver, style,
                                      SVGLengthMode::kOther));
  } else if (auto* svg_poly_element = DynamicTo<SVGPolyElement>(svg_element)) {
    WriteNameAndQuotedValue(
        ts, "points",
        svg_poly_element->Points()->CurrentValue()->ValueAsString());
  } else if (IsA<SVGPathElement>(*svg_element)) {
    const StylePath& path = style.D() ? *style.D() : *StylePath::EmptyPath();
    WriteNameAndQuotedValue(
        ts, "data",
        BuildStringFromByteStream(path.ByteStream(), kNoTransformation));
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const LayoutSVGRoot& root) {
  ts << " " << PhysicalRect(root.PhysicalLocation(), root.Size());
  WriteStyle(ts, root);
  return ts;
}

static void WriteStandardPrefix(WTF::TextStream& ts,
                                const LayoutObject& object,
                                int indent) {
  WriteIndent(ts, indent);
  ts << object.DecoratedName();

  if (object.GetNode())
    ts << " {" << object.GetNode()->nodeName() << "}";
}

static void WriteChildren(WTF::TextStream& ts,
                          const LayoutObject& object,
                          int indent) {
  for (LayoutObject* child = object.SlowFirstChild(); child;
       child = child->NextSibling())
    Write(ts, *child, indent + 1);
}

static inline void WriteCommonGradientProperties(
    WTF::TextStream& ts,
    const GradientAttributes& attrs) {
  WriteNameValuePair(ts, "gradientUnits", attrs.GradientUnits());

  if (attrs.SpreadMethod() != kSVGSpreadMethodPad)
    ts << " [spreadMethod=" << attrs.SpreadMethod() << "]";

  if (!attrs.GradientTransform().IsIdentity())
    ts << " [gradientTransform=" << attrs.GradientTransform() << "]";

  if (attrs.HasStops()) {
    ts << " [stops=( ";
    for (const auto& stop : attrs.Stops())
      ts << stop.color << "@" << stop.stop << " ";
    ts << ")]";
  }
}

void WriteSVGResourceContainer(WTF::TextStream& ts,
                               const LayoutObject& object,
                               int indent) {
  WriteStandardPrefix(ts, object, indent);

  auto* element = To<Element>(object.GetNode());
  const AtomicString& id = element->GetIdAttribute();
  WriteNameAndQuotedValue(ts, "id", id);

  auto* resource =
      To<LayoutSVGResourceContainer>(const_cast<LayoutObject*>(&object));
  DCHECK(resource);

  if (resource->ResourceType() == kMaskerResourceType) {
    auto* masker = To<LayoutSVGResourceMasker>(resource);
    WriteNameValuePair(ts, "maskUnits", masker->MaskUnits());
    WriteNameValuePair(ts, "maskContentUnits", masker->MaskContentUnits());
    ts << "\n";
  } else if (resource->ResourceType() == kFilterResourceType) {
    auto* filter = To<LayoutSVGResourceFilter>(resource);
    WriteNameValuePair(ts, "filterUnits", filter->FilterUnits());
    WriteNameValuePair(ts, "primitiveUnits", filter->PrimitiveUnits());
    ts << "\n";
    // Creating a placeholder filter which is passed to the builder.
    gfx::RectF dummy_rect;
    auto* dummy_filter = MakeGarbageCollected<Filter>(dummy_rect, dummy_rect, 1,
                                                      Filter::kBoundingBox);
    SVGFilterBuilder builder(dummy_filter->GetSourceGraphic());
    builder.BuildGraph(dummy_filter,
                       To<SVGFilterElement>(*filter->GetElement()), dummy_rect);
    if (FilterEffect* last_effect = builder.LastEffect())
      last_effect->ExternalRepresentation(ts, indent + 1);
  } else if (resource->ResourceType() == kClipperResourceType) {
    WriteNameValuePair(ts, "clipPathUnits",
                       To<LayoutSVGResourceClipper>(resource)->ClipPathUnits());
    ts << "\n";
  } else if (resource->ResourceType() == kMarkerResourceType) {
    auto* marker = To<LayoutSVGResourceMarker>(resource);
    WriteNameValuePair(ts, "markerUnits", marker->MarkerUnits());
    ts << " [ref at " << marker->ReferencePoint() << "]";
    ts << " [angle=";
    if (marker->OrientType() != kSVGMarkerOrientAngle)
      ts << marker->OrientType() << "]\n";
    else
      ts << marker->Angle() << "]\n";
  } else if (resource->ResourceType() == kPatternResourceType) {
    LayoutSVGResourcePattern* pattern =
        static_cast<LayoutSVGResourcePattern*>(resource);

    // Dump final results that are used for layout. No use in asking
    // SVGPatternElement for its patternUnits(), as it may link to other
    // patterns using xlink:href, we need to build the full inheritance chain,
    // aka. collectPatternProperties()
    PatternAttributes attributes = To<SVGPatternElement>(*pattern->GetElement())
                                       .CollectPatternAttributes();

    WriteNameValuePair(ts, "patternUnits", attributes.PatternUnits());
    WriteNameValuePair(ts, "patternContentUnits",
                       attributes.PatternContentUnits());

    AffineTransform transform = attributes.PatternTransform();
    if (!transform.IsIdentity())
      ts << " [patternTransform=" << transform << "]";
    ts << "\n";
  } else if (resource->ResourceType() == kLinearGradientResourceType) {
    LayoutSVGResourceLinearGradient* gradient =
        static_cast<LayoutSVGResourceLinearGradient*>(resource);

    // Dump final results that are used for layout. No use in asking
    // SVGGradientElement for its gradientUnits(), as it may link to other
    // gradients using xlink:href, we need to build the full inheritance chain,
    // aka. collectGradientProperties()
    LinearGradientAttributes attributes =
        To<SVGLinearGradientElement>(*gradient->GetElement())
            .CollectGradientAttributes();
    WriteCommonGradientProperties(ts, attributes);

    ts << " [start=" << gradient->StartPoint(attributes)
       << "] [end=" << gradient->EndPoint(attributes) << "]\n";
  } else if (resource->ResourceType() == kRadialGradientResourceType) {
    auto* gradient = To<LayoutSVGResourceRadialGradient>(resource);

    // Dump final results that are used for layout. No use in asking
    // SVGGradientElement for its gradientUnits(), as it may link to other
    // gradients using xlink:href, we need to build the full inheritance chain,
    // aka. collectGradientProperties()
    RadialGradientAttributes attributes =
        To<SVGRadialGradientElement>(*gradient->GetElement())
            .CollectGradientAttributes();
    WriteCommonGradientProperties(ts, attributes);

    gfx::PointF focal_point = gradient->FocalPoint(attributes);
    gfx::PointF center_point = gradient->CenterPoint(attributes);
    float radius = gradient->Radius(attributes);
    float focal_radius = gradient->FocalRadius(attributes);

    ts << " [center=" << center_point << "] [focal=" << focal_point
       << "] [radius=" << radius << "] [focalRadius=" << focal_radius << "]\n";
  } else {
    ts << "\n";
  }
  WriteChildren(ts, object, indent);
}

void WriteSVGContainer(WTF::TextStream& ts,
                       const LayoutObject& container,
                       int indent) {
  WriteStandardPrefix(ts, container, indent);
  WritePositionAndStyle(ts, container);
  ts << "\n";
  WriteResources(ts, container, indent);
  WriteChildren(ts, container, indent);
}

void Write(WTF::TextStream& ts, const LayoutSVGRoot& root, int indent) {
  WriteStandardPrefix(ts, root, indent);
  ts << root << "\n";
  WriteChildren(ts, root, indent);
}

void WriteSVGInline(WTF::TextStream& ts,
                    const LayoutSVGInline& text,
                    int indent) {
  WriteStandardPrefix(ts, text, indent);
  WritePositionAndStyle(ts, text);
  ts << "\n";
  WriteResources(ts, text, indent);
  WriteChildren(ts, text, indent);
}

void WriteSVGInlineText(WTF::TextStream& ts,
                        const LayoutSVGInlineText& text,
                        int indent) {
  WriteStandardPrefix(ts, text, indent);
  WritePositionAndStyle(ts, text);
  ts << "\n";
}

void WriteSVGImage(WTF::TextStream& ts,
                   const LayoutSVGImage& image,
                   int indent) {
  WriteStandardPrefix(ts, image, indent);
  WritePositionAndStyle(ts, image);
  ts << "\n";
  WriteResources(ts, image, indent);
}

void Write(WTF::TextStream& ts, const LayoutSVGShape& shape, int indent) {
  WriteStandardPrefix(ts, shape, indent);
  ts << shape << "\n";
  WriteResources(ts, shape, indent);
}

// Get the LayoutSVGResourceFilter from the 'filter' property iff the 'filter'
// is a single url(...) reference.
static LayoutSVGResourceFilter* GetFilterResourceForSVG(
    SVGResourceClient& client,
    const ComputedStyle& style) {
  if (!style.HasFilter())
    return nullptr;
  const FilterOperations& operations = style.Filter();
  if (operations.size() != 1)
    return nullptr;
  const auto* reference_filter =
      DynamicTo<ReferenceFilterOperation>(*operations.at(0));
  if (!reference_filter)
    return nullptr;
  return GetSVGResourceAsType<LayoutSVGResourceFilter>(
      client, reference_filter->Resource());
}

static void WriteSVGResourceReferencePrefix(
    WTF::TextStream& ts,
    const char* resource_name,
    const LayoutSVGResourceContainer* resource_object,
    const AtomicString& url,
    const TreeScope& tree_scope,
    int indent) {
  AtomicString id =
      SVGURIReference::FragmentIdentifierFromIRIString(url, tree_scope);
  WriteIndent(ts, indent);
  ts << " ";
  WriteNameAndQuotedValue(ts, resource_name, id);
  ts << " ";
  WriteStandardPrefix(ts, *resource_object, 0);
}

void WriteResources(WTF::TextStream& ts,
                    const LayoutObject& object,
                    int indent) {
  const gfx::RectF reference_box = object.ObjectBoundingBox();
  const ComputedStyle& style = object.StyleRef();
  TreeScope& tree_scope = object.GetDocument();
  SVGResourceClient* client = SVGResources::GetClient(object);
  if (!client)
    return;
  if (const ClipPathOperation* clip_path = style.ClipPath()) {
    if (LayoutSVGResourceClipper* clipper =
            GetSVGResourceAsType(*client, clip_path)) {
      DCHECK_EQ(clip_path->GetType(), ClipPathOperation::kReference);
      const auto& clip_path_reference =
          To<ReferenceClipPathOperation>(*clip_path);
      WriteSVGResourceReferencePrefix(ts, "clipPath", clipper,
                                      clip_path_reference.Url(), tree_scope,
                                      indent);
      ts << " " << clipper->ResourceBoundingBox(reference_box) << "\n";
    }
  }
  // TODO(fs): Only handles the single url(...) case. Do we care?
  if (LayoutSVGResourceFilter* filter =
          GetFilterResourceForSVG(*client, style)) {
    DCHECK(style.HasFilter());
    DCHECK_EQ(style.Filter().size(), 1u);
    const FilterOperation& filter_operation = *style.Filter().at(0);
    DCHECK_EQ(filter_operation.GetType(),
              FilterOperation::OperationType::kReference);
    const auto& reference_filter_operation =
        To<ReferenceFilterOperation>(filter_operation);
    WriteSVGResourceReferencePrefix(ts, "filter", filter,
                                    reference_filter_operation.Url(),
                                    tree_scope, indent);
    ts << " " << filter->ResourceBoundingBox(reference_box) << "\n";
  }
}

}  // namespace blink
