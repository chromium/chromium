// Copyright 2025 Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_box_quad_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_convert_coordinate_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_document_element_text.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_element.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/geometry_utils.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool CSSPseudoElement::IsSupportedTypeForCSSPseudoElement(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdMarker:
    case kPseudoIdScrollMarker:
      return true;
    default:
      return false;
  }
}

CSSPseudoElement::CSSPseudoElement(Element& originating_element,
                                   PseudoId pseudo_id)
    : pseudo_id_(pseudo_id),
      element_(originating_element),
      parent_(MakeGarbageCollected<V8UnionCSSPseudoElementOrElement>(
          &originating_element)) {}

CSSPseudoElement::CSSPseudoElement(CSSPseudoElement& originating_pseudo_element,
                                   PseudoId pseudo_id)
    : pseudo_id_(pseudo_id),
      element_(originating_pseudo_element.element_),
      parent_(MakeGarbageCollected<V8UnionCSSPseudoElementOrElement>(
          &originating_pseudo_element)) {}

String CSSPseudoElement::type() const {
  return PseudoElementTagName(pseudo_id_).ToString();
}

PseudoId CSSPseudoElement::ConvertTypeToSupportedPseudoId(
    const AtomicString& type) {
  HeapVector<CSSSelector> arena;
  CSSParserTokenStream stream(type);
  base::span<CSSSelector> vector = CSSSelectorParser::ParseSelector(
      stream,
      MakeGarbageCollected<CSSParserContext>(
          kHTMLStandardMode, SecureContextMode::kInsecureContext),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
      /*semicolon_aborts_nested_selector=*/false, nullptr, arena);
  if (vector.size() != 1) {
    return kPseudoIdInvalid;
  }
  const CSSSelector& selector = vector.front();
  return CSSSelector::GetPseudoId(selector.GetPseudoType());
}

// static
CSSPseudoElement* CSSPseudoElement::From(PseudoElement* pseudo_element) {
  // Return nullptr for null or disconnected pseudo-elements. A disconnected
  // pseudo cannot navigate to its originating element.
  if (!pseudo_element || !pseudo_element->isConnected()) {
    return nullptr;
  }
  // Build the pseudo-id chain from innermost to outermost by walking
  // parentElement(). e.g. for ::after::marker: [kPseudoIdMarker,
  // kPseudoIdAfter]
  HeapVector<PseudoId> chain;
  for (auto* p = pseudo_element; p;
       p = DynamicTo<PseudoElement>(p->parentElement())) {
    if (!p->isConnected() ||
        !IsSupportedTypeForCSSPseudoElement(p->GetPseudoId())) {
      return nullptr;
    }
    chain.push_back(p->GetPseudoId());
  }
  // Start from the outermost pseudo on the originating element.
  CSSPseudoElement* css_pseudo =
      pseudo_element->UltimateOriginatingElement().EnsureCSSPseudoElement(
          chain.back());
  // Walk inward through each nested level using PseudoId directly.
  if (chain.size() > 1) {
    for (wtf_size_t i = chain.size() - 1; i; --i) {
      css_pseudo = css_pseudo->pseudo(chain[i - 1]);
      if (!css_pseudo) {
        return nullptr;
      }
    }
  }
  return css_pseudo;
}

CSSPseudoElement* CSSPseudoElement::pseudo(PseudoId pseudo_id) {
  if (!IsSupportedTypeForCSSPseudoElement(pseudo_id)) {
    return nullptr;
  }
  if (!css_pseudo_elements_data_) {
    css_pseudo_elements_data_ =
        MakeGarbageCollected<CSSPseudoElementsCacheData>();
  }
  if (CSSPseudoElement* existing =
          css_pseudo_elements_data_->GetCSSPseudoElement(pseudo_id)) {
    return existing;
  }
  auto* css_pseudo_element =
      MakeGarbageCollected<CSSPseudoElement>(*this, pseudo_id);
  css_pseudo_elements_data_->CacheCSSPseudoElement(pseudo_id,
                                                   *css_pseudo_element);
  return css_pseudo_element;
}

CSSPseudoElement* CSSPseudoElement::pseudo(const AtomicString& type) {
  return pseudo(ConvertTypeToSupportedPseudoId(type));
}

namespace {

// Helper to get the PseudoElement from the originating element hierarchy.
PseudoElement* GetPseudoElementForCSSPseudoElement(
    const V8UnionCSSPseudoElementOrElement* parent,
    PseudoId pseudo_id) {
  CHECK(parent);

  // Walk up the chain from the current parent to the ultimate originating
  // Element, collecting pseudo-ids along the way (immediate parent first).
  Vector<PseudoId> pseudo_chain;
  const V8UnionCSSPseudoElementOrElement* current_parent = parent;
  while (current_parent && current_parent->IsCSSPseudoElement()) {
    CSSPseudoElement* parent_css_pseudo =
        current_parent->GetAsCSSPseudoElement();
    pseudo_chain.push_back(parent_css_pseudo->GetPseudoId());
    current_parent = parent_css_pseudo->parent();
  }

  // current_parent should now be the originating Element.
  if (!current_parent || !current_parent->IsElement()) {
    return nullptr;
  }
  Element* base_element = current_parent->GetAsElement();

  // CSSPseudoElement is a proxy representation of PseudoElement. To resolve a
  // nested PseudoElement from a CSSPseudoElement received from JS, we first
  // walk up the chain of CSSPseudoElements to the ultimate originating Element,
  // collecting the pseudo-ids along the way. Then we walk back down from the
  // Element, using those pseudo-ids to build the actual PseudoElement chain.
  // This is necessary because PseudoElements can only be accessed via
  // (Element/PseudoElement).GetPseudoElement(pseudo_id), starting from a real
  // Element.
  // Walk down the pseudo-elements chains, starting from the ultimate
  // originating element, using the collected pseudo-ids (in reverse order) to
  // resolve nested pseudo-elements.
  // E.g. for ::before::marker, first get ::before from the element, then (after
  // cycle) get ::marker from that ::before pseudo-element.
  Element* current_owner = base_element;
  for (size_t i = pseudo_chain.size(); i > 0; --i) {
    PseudoId id = pseudo_chain[i - 1];
    PseudoElement* next_pseudo = current_owner->GetPseudoElement(id);
    if (!next_pseudo) {
      return nullptr;
    }
    current_owner = next_pseudo;
  }

  return current_owner->GetPseudoElement(pseudo_id);
}

}  // namespace

LayoutObject* CSSPseudoElement::GetLayoutObject() const {
  CHECK(element_);
  // Ensure layout is up to date.
  element_->GetDocument().EnsurePaintLocationDataValidForNode(
      element_, DocumentUpdateReason::kJavaScript);

  PseudoElement* pseudo_element =
      GetPseudoElementForCSSPseudoElement(parent_, pseudo_id_);
  if (!pseudo_element) {
    return nullptr;
  }
  return pseudo_element->GetLayoutObject();
}

HeapVector<Member<DOMQuad>> CSSPseudoElement::getBoxQuads(
    const BoxQuadOptions* options) const {
  CHECK(RuntimeEnabledFeatures::GeometryUtilsForCSSPseudoElementEnabled());
  V8CSSBoxType::Enum box_type = V8CSSBoxType::Enum::kBorder;
  if (options && options->hasBox()) {
    box_type = options->box().AsEnum();
  }
  LayoutObject* relative_to = nullptr;
  if (options && options->hasRelativeTo()) {
    relative_to =
        geometry_utils::GetLayoutObjectFromGeometryNode(options->relativeTo());
  }
  return geometry_utils::GetBoxQuads(GetLayoutObject(), box_type, relative_to);
}

DOMQuad* CSSPseudoElement::convertQuadFromNode(
    DOMQuadInit* quad,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options) const {
  CHECK(RuntimeEnabledFeatures::GeometryUtilsForCSSPseudoElementEnabled());
  return geometry_utils::ConvertQuadFromNode(
      DOMQuad::fromQuad(quad),
      geometry_utils::GetLayoutObjectFromGeometryNode(from), GetLayoutObject());
}

DOMQuad* CSSPseudoElement::convertRectFromNode(
    DOMRectReadOnly* rect,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options) const {
  CHECK(RuntimeEnabledFeatures::GeometryUtilsForCSSPseudoElementEnabled());
  return geometry_utils::ConvertRectFromNode(
      rect, geometry_utils::GetLayoutObjectFromGeometryNode(from),
      GetLayoutObject());
}

DOMPoint* CSSPseudoElement::convertPointFromNode(
    DOMPointInit* point,
    const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
    const ConvertCoordinateOptions* options) const {
  CHECK(RuntimeEnabledFeatures::GeometryUtilsForCSSPseudoElementEnabled());
  return geometry_utils::ConvertPointFromNode(
      DOMPoint::fromPoint(point),
      geometry_utils::GetLayoutObjectFromGeometryNode(from), GetLayoutObject());
}

void CSSPseudoElement::Trace(Visitor* v) const {
  v->Trace(element_);
  v->Trace(parent_);
  v->Trace(css_pseudo_elements_data_);
  ScriptWrappable::Trace(v);
}

void CSSPseudoElementsCacheData::CacheCSSPseudoElement(
    PseudoId pseudo_id,
    CSSPseudoElement& pseudo_element) {
  auto it = pseudo_elements_map_.find(pseudo_id);
  if (it != pseudo_elements_map_.end()) {
    return;
  }
  pseudo_elements_map_.insert(pseudo_id, pseudo_element);
}

CSSPseudoElement* CSSPseudoElementsCacheData::GetCSSPseudoElement(
    PseudoId pseudo_id) {
  auto it = pseudo_elements_map_.find(pseudo_id);
  if (it == pseudo_elements_map_.end()) {
    return nullptr;
  }
  return it->value;
}

void CSSPseudoElementsCacheData::Trace(Visitor* v) const {
  v->Trace(pseudo_elements_map_);
  ElementRareDataField::Trace(v);
}

}  // namespace blink
