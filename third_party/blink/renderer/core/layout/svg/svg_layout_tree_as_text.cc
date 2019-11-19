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

#include "third_party/blink/renderer/core/layout/api/line_layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_linear_gradient.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_marker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_pattern.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_radial_gradient.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_root_inline_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_svg_resource.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/linear_gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/pattern_attributes.h"
#include "third_party/blink/renderer/core/svg/radial_gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_circle_element.h"
#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
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
#include "third_party/blink/renderer/platform/heap/heap.h"

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
                               ValueType value) {
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

static void WriteQuotedSVGResource(WTF::TextStream& ts,
                                   const char* name,
                                   const StyleSVGResource* value,
                                   TreeScope& tree_scope) {
  DCHECK(value);
  AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
      value->Url(), tree_scope);
  WriteNameAndQuotedValue(ts, name, id);
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

// FIXME: Maybe this should be in GraphicsTypes.cpp
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

// FIXME: Maybe this should be in GraphicsTypes.cpp
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

static void WriteSVGPaintingResource(
    WTF::TextStream& ts,
    const SVGPaintDescription& paint_description) {
  DCHECK(paint_description.is_valid);
  if (!paint_description.resource) {
    ts << "[type=SOLID] [color=" << paint_description.color << "]";
    return;
  }

  LayoutSVGResourcePaintServer* paint_server_container =
      paint_description.resource;
  SVGElement* element = paint_server_container->GetElement();
  DCHECK(element);

  if (paint_server_container->ResourceType() == kPatternResourceType)
    ts << "[type=PATTERN]";
  else if (paint_server_container->ResourceType() ==
           kLinearGradientResourceType)
    ts << "[type=LINEAR-GRADIENT]";
  else if (paint_server_container->ResourceType() ==
           kRadialGradientResourceType)
    ts << "[type=RADIAL-GRADIENT]";

  ts << " [id=\"" << element->GetIdAttribute() << "\"]";
}

static void WriteStyle(WTF::TextStream& ts, const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();

  if (!object.LocalSVGTransform().IsIdentity())
    WriteNameValuePair(ts, "transform", object.LocalSVGTransform());
  WriteIfNotDefault(
      ts, "image rendering", static_cast<int>(style.ImageRendering()),
      static_cast<int>(ComputedStyleInitialValues::InitialImageRendering()));
  WriteIfNotDefault(ts, "opacity", style.Opacity(),
                    ComputedStyleInitialValues::InitialOpacity());
  if (object.IsSVGShape()) {
    const LayoutSVGShape& shape = static_cast<const LayoutSVGShape&>(object);
    DCHECK(shape.GetElement());

    SVGPaintDescription stroke_paint_description =
        LayoutSVGResourcePaintServer::RequestPaintDescription(
            shape, shape.StyleRef(), kApplyToStrokeMode);
    if (stroke_paint_description.is_valid) {
      TextStreamSeparator s(" ");
      ts << " [stroke={" << s;
      WriteSVGPaintingResource(ts, stroke_paint_description);

      SVGLengthContext length_context(shape.GetElement());
      double dash_offset =
          length_context.ValueForLength(svg_style.StrokeDashOffset(), style);
      double stroke_width =
          length_context.ValueForLength(svg_style.StrokeWidth());
      DashArray dash_array = SVGLayoutSupport::ResolveSVGDashArray(
          *svg_style.StrokeDashArray(), style, length_context);

      WriteIfNotDefault(ts, "opacity", svg_style.StrokeOpacity(), 1.0f);
      WriteIfNotDefault(ts, "stroke width", stroke_width, 1.0);
      WriteIfNotDefault(ts, "miter limit", svg_style.StrokeMiterLimit(), 4.0f);
      WriteIfNotDefault(ts, "line cap", svg_style.CapStyle(), kButtCap);
      WriteIfNotDefault(ts, "line join", svg_style.JoinStyle(), kMiterJoin);
      WriteIfNotDefault(ts, "dash offset", dash_offset, 0.0);
      if (!dash_array.IsEmpty())
        WriteNameValuePair(ts, "dash array", dash_array);

      ts << "}]";
    }

    SVGPaintDescription fill_paint_description =
        LayoutSVGResourcePaintServer::RequestPaintDescription(
            shape, shape.StyleRef(), kApplyToFillMode);
    if (fill_paint_description.is_valid) {
      TextStreamSeparator s(" ");
      ts << " [fill={" << s;
      WriteSVGPaintingResource(ts, fill_paint_description);

      WriteIfNotDefault(ts, "opacity", svg_style.FillOpacity(), 1.0f);
      WriteIfNotDefault(ts, "fill rule", svg_style.FillRule(), RULE_NONZERO);
      ts << "}]";
    }
    WriteIfNotDefault(ts, "clip rule", svg_style.ClipRule(), RULE_NONZERO);
  }

  TreeScope& tree_scope = object.GetDocument();
  WriteSVGResourceIfNotNull(ts, "start marker", svg_style.MarkerStartResource(),
                            tree_scope);
  WriteSVGResourceIfNotNull(ts, "middle marker", svg_style.MarkerMidResource(),
                            tree_scope);
  WriteSVGResourceIfNotNull(ts, "end marker", svg_style.MarkerEndResource(),
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
  SVGLengthContext length_context(svg_element);
  const ComputedStyle& style = shape.StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();

  if (IsSVGRectElement(*svg_element)) {
    WriteNameValuePair(ts, "x",
                       length_context.ValueForLength(svg_style.X(), style,
                                                     SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "y",
                       length_context.ValueForLength(svg_style.Y(), style,
                                                     SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "width",
                       length_context.ValueForLength(style.Width(), style,
                                                     SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "height",
                       length_context.ValueForLength(style.Height(), style,
                                                     SVGLengthMode::kHeight));
  } else if (IsSVGLineElement(*svg_element)) {
    SVGLineElement& element = ToSVGLineElement(*svg_element);
    WriteNameValuePair(ts, "x1",
                       element.x1()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "y1",
                       element.y1()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "x2",
                       element.x2()->CurrentValue()->Value(length_context));
    WriteNameValuePair(ts, "y2",
                       element.y2()->CurrentValue()->Value(length_context));
  } else if (IsSVGEllipseElement(*svg_element)) {
    WriteNameValuePair(ts, "cx",
                       length_context.ValueForLength(svg_style.Cx(), style,
                                                     SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "cy",
                       length_context.ValueForLength(svg_style.Cy(), style,
                                                     SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "rx",
                       length_context.ValueForLength(svg_style.Rx(), style,
                                                     SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "ry",
                       length_context.ValueForLength(svg_style.Ry(), style,
                                                     SVGLengthMode::kHeight));
  } else if (IsSVGCircleElement(*svg_element)) {
    WriteNameValuePair(ts, "cx",
                       length_context.ValueForLength(svg_style.Cx(), style,
                                                     SVGLengthMode::kWidth));
    WriteNameValuePair(ts, "cy",
                       length_context.ValueForLength(svg_style.Cy(), style,
                                                     SVGLengthMode::kHeight));
    WriteNameValuePair(ts, "r",
                       length_context.ValueForLength(svg_style.R(), style,
                                                     SVGLengthMode::kOther));
  } else if (IsSVGPolyElement(*svg_element)) {
    WriteNameAndQuotedValue(ts, "points",
                            ToSVGPolyElement(*svg_element)
                                .Points()
                                ->CurrentValue()
                                ->ValueAsString());
  } else if (IsSVGPathElement(*svg_element)) {
    const StylePath& path =
        svg_style.D() ? *svg_style.D() : *StylePath::EmptyPath();
    WriteNameAndQuotedValue(
        ts, "data",
        BuildStringFromByteStream(path.ByteStream(), kNoTransformation));
  } else {
    NOTREACHED();
  }
  return ts;
}

static WTF::TextStream& operator<<(WTF::TextStream& ts,
                                   const LayoutSVGRoot& root) {
  ts << " " << root.FrameRect();
  WriteStyle(ts, root);
  return ts;
}

static void WriteLayoutSVGTextBox(WTF::TextStream& ts,
                                  const LayoutSVGText& text) {
  SVGRootInlineBox* box = ToSVGRootInlineBox(text.FirstRootBox());
  if (!box)
    return;

  // FIXME: Remove this hack, once the new text layout engine is completly
  // landed. We want to preserve the old web test results for now.
  ts << " contains 1 chunk(s)";

  if (text.Parent() && (text.Parent()->ResolveColor(GetCSSPropertyColor()) !=
                        text.ResolveColor(GetCSSPropertyColor()))) {
    WriteNameValuePair(
        ts, "color",
        text.ResolveColor(GetCSSPropertyColor()).NameForLayoutTreeAsText());
  }
}

static inline void WriteSVGInlineTextBox(WTF::TextStream& ts,
                                         SVGInlineTextBox* text_box,
                                         int indent) {
  Vector<SVGTextFragment>& fragments = text_box->TextFragments();
  if (fragments.IsEmpty())
    return;

  LineLayoutSVGInlineText text_line_layout =
      LineLayoutSVGInlineText(text_box->GetLineLayoutItem());

  const SVGComputedStyle& svg_style = text_line_layout.StyleRef().SvgStyle();
  String text = text_box->GetLineLayoutItem().GetText();

  unsigned fragments_size = fragments.size();
  for (unsigned i = 0; i < fragments_size; ++i) {
    SVGTextFragment& fragment = fragments.at(i);
    WriteIndent(ts, indent + 1);

    unsigned start_offset = fragment.character_offset;
    unsigned end_offset = fragment.character_offset + fragment.length;

    // FIXME: Remove this hack, once the new text layout engine is completly
    // landed. We want to preserve the old web test results for now.
    ts << "chunk 1 ";
    ETextAnchor anchor = svg_style.TextAnchor();
    bool is_vertical_text =
        !text_line_layout.StyleRef().IsHorizontalWritingMode();
    if (anchor == TA_MIDDLE) {
      ts << "(middle anchor";
      if (is_vertical_text)
        ts << ", vertical";
      ts << ") ";
    } else if (anchor == TA_END) {
      ts << "(end anchor";
      if (is_vertical_text)
        ts << ", vertical";
      ts << ") ";
    } else if (is_vertical_text) {
      ts << "(vertical) ";
    }
    start_offset -= text_box->Start();
    end_offset -= text_box->Start();
    // </hack>

    ts << "text run " << i + 1 << " at (" << fragment.x << "," << fragment.y
       << ")";
    ts << " startOffset " << start_offset << " endOffset " << end_offset;
    if (is_vertical_text)
      ts << " height " << fragment.height;
    else
      ts << " width " << fragment.width;

    if (!text_box->IsLeftToRightDirection() || text_box->DirOverride()) {
      ts << (text_box->IsLeftToRightDirection() ? " LTR" : " RTL");
      if (text_box->DirOverride())
        ts << " override";
    }

    ts << ": "
       << QuoteAndEscapeNonPrintables(
              text.Substring(fragment.character_offset, fragment.length))
       << "\n";
  }
}

static inline void WriteSVGInlineTextBoxes(WTF::TextStream& ts,
                                           const LayoutText& text,
                                           int indent) {
  for (InlineTextBox* box : text.TextBoxes()) {
    if (!box->IsSVGInlineTextBox())
      continue;

    WriteSVGInlineTextBox(ts, ToSVGInlineTextBox(box), indent);
  }
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

  LayoutSVGResourceContainer* resource =
      ToLayoutSVGResourceContainer(const_cast<LayoutObject*>(&object));
  DCHECK(resource);

  if (resource->ResourceType() == kMaskerResourceType) {
    LayoutSVGResourceMasker* masker = ToLayoutSVGResourceMasker(resource);
    WriteNameValuePair(ts, "maskUnits", masker->MaskUnits());
    WriteNameValuePair(ts, "maskContentUnits", masker->MaskContentUnits());
    ts << "\n";
  } else if (resource->ResourceType() == kFilterResourceType) {
    LayoutSVGResourceFilter* filter = ToLayoutSVGResourceFilter(resource);
    WriteNameValuePair(ts, "filterUnits", filter->FilterUnits());
    WriteNameValuePair(ts, "primitiveUnits", filter->PrimitiveUnits());
    ts << "\n";
    // Creating a placeholder filter which is passed to the builder.
    FloatRect dummy_rect;
    auto* dummy_filter = MakeGarbageCollected<Filter>(dummy_rect, dummy_rect, 1,
                                                      Filter::kBoundingBox);
    SVGFilterBuilder builder(dummy_filter->GetSourceGraphic());
    builder.BuildGraph(dummy_filter, ToSVGFilterElement(*filter->GetElement()),
                       dummy_rect);
    if (FilterEffect* last_effect = builder.LastEffect())
      last_effect->ExternalRepresentation(ts, indent + 1);
  } else if (resource->ResourceType() == kClipperResourceType) {
    WriteNameValuePair(ts, "clipPathUnits",
                       ToLayoutSVGResourceClipper(resource)->ClipPathUnits());
    ts << "\n";
  } else if (resource->ResourceType() == kMarkerResourceType) {
    LayoutSVGResourceMarker* marker = ToLayoutSVGResourceMarker(resource);
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
    PatternAttributes attributes;
    ToSVGPatternElement(pattern->GetElement())
        ->CollectPatternAttributes(attributes);

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
    LinearGradientAttributes attributes;
    ToSVGLinearGradientElement(gradient->GetElement())
        ->CollectGradientAttributes(attributes);
    WriteCommonGradientProperties(ts, attributes);

    ts << " [start=" << gradient->StartPoint(attributes)
       << "] [end=" << gradient->EndPoint(attributes) << "]\n";
  } else if (resource->ResourceType() == kRadialGradientResourceType) {
    LayoutSVGResourceRadialGradient* gradient =
        ToLayoutSVGResourceRadialGradient(resource);

    // Dump final results that are used for layout. No use in asking
    // SVGGradientElement for its gradientUnits(), as it may link to other
    // gradients using xlink:href, we need to build the full inheritance chain,
    // aka. collectGradientProperties()
    RadialGradientAttributes attributes;
    ToSVGRadialGradientElement(gradient->GetElement())
        ->CollectGradientAttributes(attributes);
    WriteCommonGradientProperties(ts, attributes);

    FloatPoint focal_point = gradient->FocalPoint(attributes);
    FloatPoint center_point = gradient->CenterPoint(attributes);
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
  // Currently LayoutSVGResourceFilterPrimitive has no meaningful output.
  if (container.IsSVGResourceFilterPrimitive())
    return;
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

void WriteSVGText(WTF::TextStream& ts, const LayoutSVGText& text, int indent) {
  WriteStandardPrefix(ts, text, indent);
  WritePositionAndStyle(ts, text);
  WriteLayoutSVGTextBox(ts, text);
  ts << "\n";
  WriteResources(ts, text, indent);
  WriteChildren(ts, text, indent);
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
  WriteResources(ts, text, indent);
  WriteSVGInlineTextBoxes(ts, text, indent);
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

void WriteResources(WTF::TextStream& ts,
                    const LayoutObject& object,
                    int indent) {
  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  if (!resources)
    return;
  const FloatRect reference_box = object.ObjectBoundingBox();
  const ComputedStyle& style = object.StyleRef();
  TreeScope& tree_scope = object.GetDocument();
  if (LayoutSVGResourceMasker* masker = resources->Masker()) {
    WriteIndent(ts, indent);
    ts << " ";
    WriteQuotedSVGResource(ts, "masker", style.SvgStyle().MaskerResource(),
                           tree_scope);
    ts << " ";
    WriteStandardPrefix(ts, *masker, 0);
    ts << " " << masker->ResourceBoundingBox(reference_box) << "\n";
  }
  if (LayoutSVGResourceClipper* clipper = resources->Clipper()) {
    DCHECK(style.ClipPath());
    DCHECK_EQ(style.ClipPath()->GetType(), ClipPathOperation::REFERENCE);
    const ReferenceClipPathOperation& clip_path_reference =
        To<ReferenceClipPathOperation>(*style.ClipPath());
    AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
        clip_path_reference.Url(), tree_scope);
    WriteIndent(ts, indent);
    ts << " ";
    WriteNameAndQuotedValue(ts, "clipPath", id);
    ts << " ";
    WriteStandardPrefix(ts, *clipper, 0);
    ts << " " << clipper->ResourceBoundingBox(reference_box) << "\n";
  }
  if (LayoutSVGResourceFilter* filter = resources->Filter()) {
    DCHECK(style.HasFilter());
    DCHECK_EQ(style.Filter().size(), 1u);
    const FilterOperation& filter_operation = *style.Filter().at(0);
    DCHECK_EQ(filter_operation.GetType(), FilterOperation::REFERENCE);
    const auto& reference_filter_operation =
        To<ReferenceFilterOperation>(filter_operation);
    AtomicString id = SVGURIReference::FragmentIdentifierFromIRIString(
        reference_filter_operation.Url(), tree_scope);
    WriteIndent(ts, indent);
    ts << " ";
    WriteNameAndQuotedValue(ts, "filter", id);
    ts << " ";
    WriteStandardPrefix(ts, *filter, 0);
    ts << " " << filter->ResourceBoundingBox(reference_box) << "\n";
  }
}

}  // namespace blink
