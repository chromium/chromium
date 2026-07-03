/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_path_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_svg_path_data_settings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_animated_path.h"
#include "third_party/blink/renderer/core/svg/svg_mpath_element.h"
#include "third_party/blink/renderer/core/svg/svg_path.h"
#include "third_party/blink/renderer/core/svg/svg_path_query.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_zoom_migration.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

SVGPathElement::SVGPathElement(Document& document)
    : SVGGeometryElement(svg_names::kPathTag, document),
      path_(MakeGarbageCollected<SVGAnimatedPath>(this,
                                                  svg_names::kDAttr,
                                                  CSSPropertyID::kD)) {}

void SVGPathElement::Trace(Visitor* visitor) const {
  visitor->Trace(path_);
  SVGGeometryElement::Trace(visitor);
}

const StylePath* SVGPathElement::GetStylePath() const {
  if (const ComputedStyle* style = GetComputedStyle()) {
    if (const StylePath* style_path = style->D())
      return style_path;
    return StylePath::EmptyPath();
  }
  return path_->CurrentValue()->GetStylePath();
}

float SVGPathElement::ComputePathLength() const {
  return GetStylePath()->length();
}

const SVGPathByteStream& SVGPathElement::PathByteStream() const {
  return GetStylePath()->ByteStream();
}

Path SVGPathElement::AsPath() const {
  const Path& unzoomed_path = GetStylePath()->GetUnzoomedPath();
  if (!RuntimeEnabledFeatures::SvgNewZoomEnabled()) {
    return unzoomed_path;
  }

  const ComputedStyle* style = GetComputedStyle();
  if (!style || style->EffectiveZoom() == 1) {
    return unzoomed_path;
  }
  return PathBuilder(unzoomed_path)
      .Transform(AffineTransform::MakeScale(style->EffectiveZoom()))
      .Finalize();
}

PathBuilder SVGPathElement::AsMutablePath() const {
  return PathBuilder(AsPath());
}

const Path& SVGPathElement::GetUnzoomedAsPath() const {
  return GetStylePath()->GetUnzoomedPath();
}

float SVGPathElement::getTotalLength(ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  EnsureComputedStyle();
  return SVGPathQuery(PathByteStream()).GetTotalLength();
}

SVGPointTearOff* SVGPathElement::getPointAtLength(
    float length,
    ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  EnsureComputedStyle();
  const SVGPathByteStream& byte_stream = PathByteStream();
  if (byte_stream.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The element's path is empty.");
    return nullptr;
  }

  SVGPathQuery path_query(byte_stream);
  if (length < 0) {
    length = 0;
  } else {
    float computed_length = path_query.GetTotalLength();
    if (length > computed_length)
      length = computed_length;
  }
  gfx::PointF point = path_query.GetPointAtLength(length);
  return SVGPointTearOff::CreateDetached(point);
}

HeapVector<Member<SVGPathSegment>> SVGPathElement::getPathData(
    const SVGPathDataSettings* settings) {
  const bool normalize = settings->normalize();
  if (normalize) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kSVGPathElementGetPathDataNormalized);
  }
  // Read the attribute base value, unaffected by SMIL or CSS.
  return BuildPathSegmentsFromByteStream(path_->BaseValue()->ByteStream(),
                                         normalize);
}

void SVGPathElement::setPathData(
    const HeapVector<Member<SVGPathSegment>>& path_data) {
  // Build the valid-prefix byte stream and route it through setAttribute("d")
  // so the full attribute mutation pipeline fires (invalidation, <use>
  // instances, MutationObserver).
  // TODO(crbug.com/40441025): Write the byte stream directly into SVGPath
  // once lazy attribute sync fires Will/DidModifyAttribute.
  SVGPathByteStream byte_stream = BuildByteStreamFromSegments(path_data);
  // An empty result removes the attribute, matching other list-valued
  // attributes.
  if (byte_stream.IsEmpty()) {
    removeAttribute(svg_names::kDAttr);
    return;
  }
  setAttribute(svg_names::kDAttr, AtomicString(BuildStringFromByteStream(
                                      byte_stream, kNoTransformation)));
}

void SVGPathElement::DidRecalcStyle(const StyleRecalcChange change) {
  SVGGeometryElement::DidRecalcStyle(change);
  InvalidateMPathDependencies();
}

void SVGPathElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kDAttr) {
    InvalidateMPathDependencies();
    GeometryPresentationAttributeChanged(params.property);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

void SVGPathElement::InvalidateMPathDependencies() {
  // <mpath> can only reference <path> but this dependency is not handled in
  // markForLayoutAndParentResourceInvalidation so we update any mpath
  // dependencies manually.
  if (SVGElementSet* dependencies = SetOfIncomingReferences()) {
    for (SVGElement* element : *dependencies) {
      if (auto* mpath = DynamicTo<SVGMPathElement>(*element))
        mpath->TargetPathChanged();
    }
  }
}

Node::InsertionNotificationRequest SVGPathElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGGeometryElement::InsertedInto(root_parent);
  InvalidateMPathDependencies();
  return kInsertionDone;
}

void SVGPathElement::RemovedFrom(ContainerNode& root_parent) {
  SVGGeometryElement::RemovedFrom(root_parent);
  InvalidateMPathDependencies();
}

gfx::RectF SVGPathElement::GetBBox() {
  // We want the exact bounds.
  return GetStylePath()->GetUnzoomedPath().TightBoundingRect();
}

SVGAnimatedPropertyBase* SVGPathElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kDAttr) {
    return path_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGPathElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{path_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

void SVGPathElement::CollectExtraStyleForPresentationAttribute(
    HeapVector<CSSPropertyValue, 8>& style) {
  AddAnimatedPropertyToPresentationAttributeStyle(*path_, style);
  SVGGeometryElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink
