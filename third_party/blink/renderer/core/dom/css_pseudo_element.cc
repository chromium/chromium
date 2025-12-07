// Copyright 2025 Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_element.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

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
  PseudoId pseudo_id = CSSSelector::GetPseudoId(selector.GetPseudoType());
  if (IsSupportedTypeForCSSPseudoElement(pseudo_id)) {
    return pseudo_id;
  }
  return kPseudoIdInvalid;
}

CSSPseudoElement* CSSPseudoElement::pseudo(const AtomicString& type) {
  PseudoId pseudo_id = ConvertTypeToSupportedPseudoId(type);
  if (pseudo_id == kPseudoIdInvalid) {
    return nullptr;
  }
  if (!css_pseudo_elements_data_) {
    css_pseudo_elements_data_ =
        MakeGarbageCollected<CSSPseudoElementsCacheData>();
  }
  if (CSSPseudoElement* css_pseudo_element =
          css_pseudo_elements_data_->GetCSSPseudoElement(pseudo_id)) {
    return css_pseudo_element;
  }
  auto* css_pseudo_element =
      MakeGarbageCollected<CSSPseudoElement>(*this, pseudo_id);
  css_pseudo_elements_data_->CacheCSSPseudoElement(pseudo_id,
                                                   *css_pseudo_element);
  return css_pseudo_element;
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
