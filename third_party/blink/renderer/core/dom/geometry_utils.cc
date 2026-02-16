// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/geometry_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_box_quad_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_convert_coordinate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_box_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_document_element_text.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink::geometry_utils {

namespace {

DOMPointInit* ConvertPointToDOMPointInit(const gfx::PointF& p,
                                         const LayoutObject* source_layout,
                                         const LayoutObject* target_layout) {
  gfx::PointF target_origin =
      target_layout->LocalToAbsolutePoint(gfx::PointF());
  gfx::PointF absolute = source_layout->LocalToAbsolutePoint(p);
  gfx::PointF shifted = absolute - target_origin.OffsetFromOrigin();
  DOMPointInit* dom_point = DOMPointInit::Create();
  dom_point->setX(shifted.x());
  dom_point->setY(shifted.y());
  return dom_point;
}

}  //  namespace

LayoutObject* GetLayoutObjectFromGeometryNode(
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* node) {
  if (!node) {
    return nullptr;
  }
  switch (node->GetContentType()) {
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kElement: {
      Element* element = node->GetAsElement();
      element->GetDocument().EnsurePaintLocationDataValidForNode(
          element, DocumentUpdateReason::kJavaScript);
      return element->GetLayoutObject();
    }
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::kText: {
      Text* text = node->GetAsText();
      text->GetDocument().EnsurePaintLocationDataValidForNode(
          text, DocumentUpdateReason::kJavaScript);
      return text->GetLayoutObject();
    }
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kDocument: {
      Document* doc = node->GetAsDocument();
      Element* element = doc->documentElement();
      if (element) {
        doc->EnsurePaintLocationDataValidForNode(
            element, DocumentUpdateReason::kJavaScript);
        return element->GetLayoutObject();
      }
      return nullptr;
    }
    case V8UnionCSSPseudoElementOrDocumentOrElementOrText::ContentType::
        kCSSPseudoElement: {
      return node->GetAsCSSPseudoElement()->GetLayoutObject();
    }
  }
  NOTREACHED();
}

PhysicalRect GetBoxRect(LayoutBox* layout_box, V8CSSBoxType::Enum box_type) {
  switch (box_type) {
    case V8CSSBoxType::Enum::kMargin: {
      PhysicalRect border_box = layout_box->PhysicalBorderBoxRect();
      PhysicalBoxStrut margins = layout_box->MarginBoxOutsets();
      border_box.Expand(margins);
      return border_box;
    }
    case V8CSSBoxType::Enum::kBorder:
      return layout_box->PhysicalBorderBoxRect();
    case V8CSSBoxType::Enum::kPadding:
      return layout_box->PhysicalPaddingBoxRect();
    case V8CSSBoxType::Enum::kContent:
      return layout_box->PhysicalContentBoxRect();
  }
  NOTREACHED();
}

HeapVector<Member<DOMQuad>> GetBoxQuads(LayoutObject* layout_object,
                                        V8CSSBoxType::Enum box_type,
                                        LayoutObject* relative_to) {
  HeapVector<Member<DOMQuad>> result;

  if (!layout_object) {
    return result;
  }

  auto* layout_box = DynamicTo<LayoutBox>(layout_object);
  if (layout_box) {
    PhysicalRect box_rect = GetBoxRect(layout_box, box_type);
    gfx::QuadF quad = layout_object->LocalRectToAbsoluteQuad(box_rect);
    if (relative_to) {
      gfx::PointF origin = relative_to->LocalToAbsolutePoint(gfx::PointF());
      quad -= origin.OffsetFromOrigin();
    }
    result.push_back(MakeGarbageCollected<DOMQuad>(
        quad.p1().x(), quad.p1().y(), quad.p2().x() - quad.p1().x(),
        quad.p4().y() - quad.p1().y()));
  } else {
    // For non-box layout objects, use absolute quads.
    Vector<gfx::QuadF> quads;
    layout_object->AbsoluteQuads(quads);
    for (const auto& quad : quads) {
      gfx::QuadF adjusted_quad = quad;
      if (relative_to) {
        gfx::PointF origin = relative_to->LocalToAbsolutePoint(gfx::PointF());
        adjusted_quad -= origin.OffsetFromOrigin();
      }
      result.push_back(MakeGarbageCollected<DOMQuad>(
          adjusted_quad.p1().x(), adjusted_quad.p1().y(),
          adjusted_quad.p2().x() - adjusted_quad.p1().x(),
          adjusted_quad.p4().y() - adjusted_quad.p1().y()));
    }
  }

  return result;
}

DOMQuad* ConvertQuadFromNode(DOMQuad* quad,
                             LayoutObject* source_layout,
                             LayoutObject* target_layout) {
  if (!target_layout || !source_layout || !quad) {
    return nullptr;
  }

  gfx::PointF p1(quad->p1()->x(), quad->p1()->y());
  gfx::PointF p2(quad->p2()->x(), quad->p2()->y());
  gfx::PointF p3(quad->p3()->x(), quad->p3()->y());
  gfx::PointF p4(quad->p4()->x(), quad->p4()->y());

  DOMPointInit* dom_p1 =
      ConvertPointToDOMPointInit(p1, source_layout, target_layout);
  DOMPointInit* dom_p2 =
      ConvertPointToDOMPointInit(p2, source_layout, target_layout);
  DOMPointInit* dom_p3 =
      ConvertPointToDOMPointInit(p3, source_layout, target_layout);
  DOMPointInit* dom_p4 =
      ConvertPointToDOMPointInit(p4, source_layout, target_layout);

  return DOMQuad::Create(dom_p1, dom_p2, dom_p3, dom_p4);
}

DOMQuad* ConvertRectFromNode(DOMRectReadOnly* rect,
                             LayoutObject* source_layout,
                             LayoutObject* target_layout) {
  if (!target_layout || !source_layout) {
    return nullptr;
  }

  // Get the rect as 4 corner points.
  double x = rect->x();
  double y = rect->y();
  double width = rect->width();
  double height = rect->height();

  gfx::PointF p1(x, y);
  gfx::PointF p2(x + width, y);
  gfx::PointF p3(x + width, y + height);
  gfx::PointF p4(x, y + height);

  DOMPointInit* dom_p1 =
      ConvertPointToDOMPointInit(p1, source_layout, target_layout);
  DOMPointInit* dom_p2 =
      ConvertPointToDOMPointInit(p2, source_layout, target_layout);
  DOMPointInit* dom_p3 =
      ConvertPointToDOMPointInit(p3, source_layout, target_layout);
  DOMPointInit* dom_p4 =
      ConvertPointToDOMPointInit(p4, source_layout, target_layout);

  return DOMQuad::Create(dom_p1, dom_p2, dom_p3, dom_p4);
}

DOMPoint* ConvertPointFromNode(DOMPoint* point,
                               LayoutObject* source_layout,
                               LayoutObject* target_layout) {
  if (!target_layout || !source_layout || !point) {
    return nullptr;
  }

  double x = point->x();
  double y = point->y();
  double z = point->z();
  double w = point->w();

  // Convert from source local to absolute.
  gfx::PointF abs_point =
      source_layout->LocalToAbsolutePoint(gfx::PointF(x, y));

  // Convert from absolute to target local.
  gfx::PointF target_origin =
      target_layout->LocalToAbsolutePoint(gfx::PointF());
  gfx::PointF target_point = abs_point - target_origin.OffsetFromOrigin();

  return DOMPoint::Create(target_point.x(), target_point.y(), z, w);
}

}  // namespace blink::geometry_utils
