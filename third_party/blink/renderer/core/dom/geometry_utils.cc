// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/geometry_utils.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_box_quad_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_convert_coordinate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_box_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_document_element_text.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink::geometry_utils {

namespace {

BoxQuadType ToBoxQuadType(V8CSSBoxType::Enum box_type) {
  switch (box_type) {
    case V8CSSBoxType::Enum::kMargin:
      return BoxQuadType::kMargin;
    case V8CSSBoxType::Enum::kBorder:
      return BoxQuadType::kBorder;
    case V8CSSBoxType::Enum::kPadding:
      return BoxQuadType::kPadding;
    case V8CSSBoxType::Enum::kContent:
      return BoxQuadType::kContent;
  }
  NOTREACHED();
}

gfx::Vector2dF BoxTopLeftOffset(LayoutObject* layout_object,
                                V8CSSBoxType::Enum box_type) {
  auto* box_model = DynamicTo<LayoutBoxModelObject>(layout_object);
  if (!box_model) {
    return gfx::Vector2dF();
  }

  Vector<gfx::QuadF> local_quads;
  layout_object->QuadsInAncestor(local_quads, box_model, {},
                                 ToBoxQuadType(box_type));
  if (local_quads.empty()) {
    return gfx::Vector2dF();
  }
  return local_quads.front().p1() - gfx::PointF();
}

gfx::QuadF ScaleQuadToCSSPixels(const gfx::QuadF& quad,
                                const LayoutObject* layout_object) {
  if (!layout_object) {
    return quad;
  }

  float zoom = AdjustForAbsoluteZoom::GetAbsoluteZoom(*layout_object);
  if (zoom == 1) {
    return quad;
  }

  gfx::QuadF scaled_quad = quad;
  scaled_quad.Scale(1 / zoom, 1 / zoom);
  return scaled_quad;
}

gfx::QuadF ScaleQuadToLayoutUnits(const gfx::QuadF& quad,
                                  const LayoutObject* layout_object) {
  if (!layout_object) {
    return quad;
  }

  float zoom = AdjustForAbsoluteZoom::GetAbsoluteZoom(*layout_object);
  if (zoom == 1) {
    return quad;
  }

  gfx::QuadF scaled_quad = quad;
  scaled_quad.Scale(zoom, zoom);
  return scaled_quad;
}

gfx::PointF MapPointBetweenFrames(const gfx::PointF& point,
                                  const LocalFrameView* source_view,
                                  const LocalFrameView* target_view) {
  if (!source_view || !target_view || source_view == target_view) {
    return point;
  }
  return target_view->ConvertFromRootFrame(
      source_view->ConvertToRootFrame(point));
}

gfx::QuadF MapFrameQuadBetweenFrames(const gfx::QuadF& source_frame_quad,
                                     LayoutObject* source_layout,
                                     LayoutObject* target_layout) {
  const LocalFrameView* source_view = source_layout->GetFrameView();
  const LocalFrameView* target_view = target_layout->GetFrameView();
  if (!source_view || !target_view || source_view == target_view) {
    return source_frame_quad;
  }

  gfx::QuadF frame_quad = source_frame_quad;
  frame_quad.set_p1(
      MapPointBetweenFrames(frame_quad.p1(), source_view, target_view));
  frame_quad.set_p2(
      MapPointBetweenFrames(frame_quad.p2(), source_view, target_view));
  frame_quad.set_p3(
      MapPointBetweenFrames(frame_quad.p3(), source_view, target_view));
  frame_quad.set_p4(
      MapPointBetweenFrames(frame_quad.p4(), source_view, target_view));
  return frame_quad;
}

gfx::QuadF MapFrameQuadToGeometryNode(const gfx::QuadF& frame_quad,
                                      LayoutObject* source_layout,
                                      LayoutObject* target_layout,
                                      V8CSSBoxType::Enum to_box) {
  gfx::QuadF target_quad;
  if (!target_layout || IsA<LayoutView>(target_layout)) {
    target_quad = target_layout ? MapFrameQuadBetweenFrames(
                                      frame_quad, source_layout, target_layout)
                                : frame_quad;
  } else {
    target_quad = target_layout->AbsoluteToLocalQuad(
        MapFrameQuadBetweenFrames(frame_quad, source_layout, target_layout));
    target_quad -= BoxTopLeftOffset(target_layout, to_box);
  }

  return ScaleQuadToCSSPixels(target_quad,
                              target_layout ? target_layout : source_layout);
}

bool CanUseGeometryMapper(const LayoutObject& object) {
  LayoutView* layout_view = object.GetDocument().GetLayoutView();
  return layout_view && !layout_view->NeedsPaintPropertyUpdate() &&
         !layout_view->DescendantNeedsPaintPropertyUpdate();
}

std::optional<gfx::QuadF> TryMapLocalQuadWithGeometryMapper(
    const gfx::QuadF& local_quad,
    LayoutObject* source_layout,
    LayoutObject* target_layout,
    V8CSSBoxType::Enum to_box) {
  if (!target_layout || IsA<LayoutView>(target_layout) ||
      source_layout->GetFrameView() != target_layout->GetFrameView()) {
    return std::nullopt;
  }

  if (!CanUseGeometryMapper(*source_layout) || source_layout->IsFragmented() ||
      target_layout->IsFragmented()) {
    return std::nullopt;
  }

  PropertyTreeStateOrAlias source_properties(PropertyTreeState::kUninitialized);
  if (!source_layout->GetPropertyContainer(nullptr, &source_properties)) {
    return std::nullopt;
  }

  const FragmentData& target_fragment = target_layout->FirstFragment();
  if (!target_fragment.HasLocalBorderBoxProperties()) {
    return std::nullopt;
  }

  gfx::Transform projection;
  if (!GeometryMapper::SourceToDestinationProjection(
          source_properties.Transform(),
          target_fragment.ContentsProperties().Transform(), projection)) {
    return std::nullopt;
  }

  gfx::QuadF target_quad = local_quad;
  target_quad += gfx::Vector2dF(source_layout->FirstFragment().PaintOffset());
  target_quad = projection.MapQuad(target_quad);
  target_quad -= gfx::Vector2dF(target_fragment.PaintOffset());
  target_quad -= BoxTopLeftOffset(target_layout, to_box);
  return ScaleQuadToCSSPixels(target_quad, target_layout);
}

gfx::QuadF MapLocalQuadToGeometryNode(const gfx::QuadF& local_quad,
                                      LayoutObject* source_layout,
                                      LayoutObject* target_layout,
                                      V8CSSBoxType::Enum to_box) {
  if (source_layout == target_layout) {
    gfx::QuadF target_quad = local_quad;
    target_quad -= BoxTopLeftOffset(target_layout, to_box);
    return ScaleQuadToCSSPixels(target_quad, target_layout);
  }

  if (std::optional<gfx::QuadF> target_quad = TryMapLocalQuadWithGeometryMapper(
          local_quad, source_layout, target_layout, to_box)) {
    return *target_quad;
  }

  if (target_layout && !IsA<LayoutView>(target_layout) &&
      source_layout->GetFrameView() == target_layout->GetFrameView()) {
    if (auto* target_box_model = DynamicTo<LayoutBoxModelObject>(target_layout);
        target_box_model && source_layout->IsDescendantOf(target_layout)) {
      gfx::QuadF target_quad =
          source_layout->LocalToAncestorQuad(local_quad, target_box_model);
      target_quad -= BoxTopLeftOffset(target_layout, to_box);
      return ScaleQuadToCSSPixels(target_quad, target_layout);
    }
  }

  return MapFrameQuadToGeometryNode(
      source_layout->LocalToAbsoluteQuad(local_quad), source_layout,
      target_layout, to_box);
}

gfx::QuadF ConvertQuadFromNodeInternal(const gfx::QuadF& quad,
                                       LayoutObject* source_layout,
                                       LayoutObject* target_layout,
                                       V8CSSBoxType::Enum from_box,
                                       V8CSSBoxType::Enum to_box) {
  gfx::QuadF source_quad = ScaleQuadToLayoutUnits(quad, source_layout);
  source_quad += BoxTopLeftOffset(source_layout, from_box);
  return MapLocalQuadToGeometryNode(source_quad, source_layout, target_layout,
                                    to_box);
}

LayoutObject* GetLayoutObject(Node* node,
                              const CSSPseudoElement* pseudo_element) {
  if (pseudo_element) {
    return pseudo_element->GetLayoutObject();
  }
  if (auto* document = DynamicTo<Document>(node)) {
    return document->GetLayoutView();
  }
  return node ? node->GetLayoutObject() : nullptr;
}

bool ResolveConversionLayoutObjects(
    Node* target_node,
    const CSSPseudoElement* target_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* source,
    LayoutObject*& source_layout,
    LayoutObject*& target_layout,
    ExceptionState& exception_state) {
  source_layout = GetLayoutObjectFromGeometryNode(source);
  if (!source_layout && source->GetContentType() ==
                            V8UnionCSSPseudoElementOrDocumentOrElementOrText::
                                ContentType::kCSSPseudoElement) {
    // A non-generated pseudo-element has no box of its own, but its local
    // transform still starts at its originating element.
    source_layout =
        source->GetAsCSSPseudoElement()->element()->GetLayoutObject();
  }
  target_layout = GetLayoutObject(target_node, target_pseudo);
  if (source_layout && target_layout) {
    return true;
  }
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotFoundError,
      "Cannot convert coordinates because the source or target node has no "
      "associated box.");
  return false;
}

V8CSSBoxType::Enum GetBoxType(const BoxQuadOptions* options) {
  return options && options->hasBox() ? options->box().AsEnum()
                                      : V8CSSBoxType::Enum::kBorder;
}

V8CSSBoxType::Enum GetFromBoxType(const ConvertCoordinateOptions* options) {
  return options && options->hasFromBox() ? options->fromBox().AsEnum()
                                          : V8CSSBoxType::Enum::kBorder;
}

V8CSSBoxType::Enum GetToBoxType(const ConvertCoordinateOptions* options) {
  return options && options->hasToBox() ? options->toBox().AsEnum()
                                        : V8CSSBoxType::Enum::kBorder;
}

Node* GetNodeFromGeometryNode(
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* node) {
  if (!node) {
    return nullptr;
  }
  switch (node->GetContentType()) {
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kElement:
      return node->GetAsElement();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::kText:
      return node->GetAsText();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kDocument:
      return node->GetAsDocument();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kCSSPseudoElement:
      return node->GetAsCSSPseudoElement()->element();
  }
  NOTREACHED();
}

bool CheckSameOriginPath(const Node* source,
                         const Node* target,
                         ExceptionState& exception_state) {
  if (!source || !target || &source->GetDocument() == &target->GetDocument()) {
    return true;
  }
  Frame* source_frame = source->GetDocument().GetFrame();
  if (source_frame && source_frame->IsFrameTreePathSameOrigin(
                          target->GetDocument().GetFrame())) {
    return true;
  }
  exception_state.ThrowSecurityError(
      "Geometry conversion is not allowed across a cross-origin document "
      "boundary.");
  return false;
}

void UpdateLayoutForGeometryNodes(const Node* source, const Node* target) {
  if (source) {
    source->GetDocument().UpdateStyleAndLayoutForNode(
        source, DocumentUpdateReason::kJavaScript);
  }
  if (target && (!source || &target->GetDocument() != &source->GetDocument())) {
    target->GetDocument().UpdateStyleAndLayoutForNode(
        target, DocumentUpdateReason::kJavaScript);
  }
}

}  // namespace

LayoutObject* GetLayoutObjectFromGeometryNode(
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* node) {
  if (!node) {
    return nullptr;
  }
  switch (node->GetContentType()) {
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kElement:
      return node->GetAsElement()->GetLayoutObject();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::kText:
      return node->GetAsText()->GetLayoutObject();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kDocument:
      return node->GetAsDocument()->GetLayoutView();
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kCSSPseudoElement: {
      return node->GetAsCSSPseudoElement()->GetLayoutObject();
    }
  }
  NOTREACHED();
}

HeapVector<Member<DOMQuad>> GetBoxQuads(Node* source_node,
                                        const CSSPseudoElement* source_pseudo,
                                        const BoxQuadOptions* options,
                                        ExceptionState& exception_state) {
  Node* relative_to_node = options && options->hasRelativeTo()
                               ? GetNodeFromGeometryNode(options->relativeTo())
                               : nullptr;
  if (!CheckSameOriginPath(source_node, relative_to_node, exception_state)) {
    return {};
  }
  UpdateLayoutForGeometryNodes(source_node, relative_to_node);

  LayoutObject* relative_to =
      options && options->hasRelativeTo()
          ? GetLayoutObjectFromGeometryNode(options->relativeTo())
          : source_node->GetDocument().GetLayoutView();
  if (options && options->hasRelativeTo() && !relative_to) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Cannot get box quads relative to a node with no associated box.");
    return {};
  }
  return GetBoxQuads(GetLayoutObject(source_node, source_pseudo),
                     GetBoxType(options), relative_to);
}

DOMQuad* ConvertQuadFromNode(
    const DOMQuadInit* quad,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state) {
  Node* from_node = GetNodeFromGeometryNode(from);
  if (!CheckSameOriginPath(from_node, source_node, exception_state)) {
    return nullptr;
  }
  UpdateLayoutForGeometryNodes(source_node, from_node);

  LayoutObject* from_layout = nullptr;
  LayoutObject* target_layout = nullptr;
  if (!ResolveConversionLayoutObjects(source_node, source_pseudo, from,
                                      from_layout, target_layout,
                                      exception_state)) {
    return nullptr;
  }
  return ConvertQuadFromNode(DOMQuad::fromQuad(quad), from_layout,
                             target_layout, GetFromBoxType(options),
                             GetToBoxType(options));
}

DOMQuad* ConvertRectFromNode(
    DOMRectReadOnly* rect,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state) {
  Node* from_node = GetNodeFromGeometryNode(from);
  if (!CheckSameOriginPath(from_node, source_node, exception_state)) {
    return nullptr;
  }
  UpdateLayoutForGeometryNodes(source_node, from_node);

  LayoutObject* from_layout = nullptr;
  LayoutObject* target_layout = nullptr;
  if (!ResolveConversionLayoutObjects(source_node, source_pseudo, from,
                                      from_layout, target_layout,
                                      exception_state)) {
    return nullptr;
  }
  return ConvertRectFromNode(rect, from_layout, target_layout,
                             GetFromBoxType(options), GetToBoxType(options));
}

DOMPoint* ConvertPointFromNode(
    const DOMPointInit* point,
    Node* source_node,
    const CSSPseudoElement* source_pseudo,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options,
    ExceptionState& exception_state) {
  Node* from_node = GetNodeFromGeometryNode(from);
  if (!CheckSameOriginPath(from_node, source_node, exception_state)) {
    return nullptr;
  }
  UpdateLayoutForGeometryNodes(source_node, from_node);

  LayoutObject* from_layout = nullptr;
  LayoutObject* target_layout = nullptr;
  if (!ResolveConversionLayoutObjects(source_node, source_pseudo, from,
                                      from_layout, target_layout,
                                      exception_state)) {
    return nullptr;
  }
  return ConvertPointFromNode(DOMPoint::fromPoint(point), from_layout,
                              target_layout, GetFromBoxType(options),
                              GetToBoxType(options));
}

HeapVector<Member<DOMQuad>> GetBoxQuads(LayoutObject* layout_object,
                                        V8CSSBoxType::Enum box_type,
                                        LayoutObject* relative_to) {
  HeapVector<Member<DOMQuad>> result;
  if (!layout_object) {
    return result;
  }
  if (relative_to && !layout_object->GetFrame()->IsFrameTreePathSameOrigin(
                         relative_to->GetFrame())) {
    return result;
  }

  Vector<gfx::QuadF> quads;
  auto* local_ancestor = DynamicTo<LayoutBoxModelObject>(layout_object);
  if (auto* layout_view = DynamicTo<LayoutView>(layout_object)) {
    const gfx::Size size = layout_view->GetLayoutSize(kExcludeScrollbars);
    quads.push_back(gfx::QuadF(gfx::PointF(), gfx::PointF(size.width(), 0),
                               gfx::PointF(size.width(), size.height()),
                               gfx::PointF(0, size.height())));
  } else {
    layout_object->QuadsInAncestor(quads, local_ancestor, {},
                                   ToBoxQuadType(box_type));
  }
  for (const gfx::QuadF& quad : quads) {
    if (local_ancestor) {
      result.push_back(DOMQuad::FromQuadF(MapLocalQuadToGeometryNode(
          quad, layout_object, relative_to, V8CSSBoxType::Enum::kBorder)));
    } else {
      result.push_back(DOMQuad::FromQuadF(MapFrameQuadToGeometryNode(
          quad, layout_object, relative_to, V8CSSBoxType::Enum::kBorder)));
    }
  }

  return result;
}

DOMQuad* ConvertQuadFromNode(DOMQuad* quad,
                             LayoutObject* source_layout,
                             LayoutObject* target_layout,
                             V8CSSBoxType::Enum from_box,
                             V8CSSBoxType::Enum to_box) {
  if (!target_layout || !source_layout || !quad) {
    return nullptr;
  }
  if (!source_layout->GetFrame()->IsFrameTreePathSameOrigin(
          target_layout->GetFrame())) {
    return nullptr;
  }

  return DOMQuad::FromQuadF(ConvertQuadFromNodeInternal(
      gfx::QuadF(gfx::PointF(quad->p1()->x(), quad->p1()->y()),
                 gfx::PointF(quad->p2()->x(), quad->p2()->y()),
                 gfx::PointF(quad->p3()->x(), quad->p3()->y()),
                 gfx::PointF(quad->p4()->x(), quad->p4()->y())),
      source_layout, target_layout, from_box, to_box));
}

DOMQuad* ConvertRectFromNode(DOMRectReadOnly* rect,
                             LayoutObject* source_layout,
                             LayoutObject* target_layout,
                             V8CSSBoxType::Enum from_box,
                             V8CSSBoxType::Enum to_box) {
  if (!target_layout || !source_layout) {
    return nullptr;
  }
  if (!source_layout->GetFrame()->IsFrameTreePathSameOrigin(
          target_layout->GetFrame())) {
    return nullptr;
  }

  // Get the rect as 4 corner points.
  double x = rect->x();
  double y = rect->y();
  double width = rect->width();
  double height = rect->height();

  return DOMQuad::FromQuadF(ConvertQuadFromNodeInternal(
      gfx::QuadF(gfx::PointF(x, y), gfx::PointF(x + width, y),
                 gfx::PointF(x + width, y + height),
                 gfx::PointF(x, y + height)),
      source_layout, target_layout, from_box, to_box));
}

DOMPoint* ConvertPointFromNode(DOMPoint* point,
                               LayoutObject* source_layout,
                               LayoutObject* target_layout,
                               V8CSSBoxType::Enum from_box,
                               V8CSSBoxType::Enum to_box) {
  if (!target_layout || !source_layout || !point) {
    return nullptr;
  }
  if (!source_layout->GetFrame()->IsFrameTreePathSameOrigin(
          target_layout->GetFrame())) {
    return nullptr;
  }

  double x = point->x();
  double y = point->y();

  gfx::QuadF target_quad = ConvertQuadFromNodeInternal(
      gfx::QuadF(gfx::PointF(x, y), gfx::PointF(x, y), gfx::PointF(x, y),
                 gfx::PointF(x, y)),
      source_layout, target_layout, from_box, to_box);

  return DOMPoint::Create(target_quad.p1().x(), target_quad.p1().y());
}

}  // namespace blink::geometry_utils
