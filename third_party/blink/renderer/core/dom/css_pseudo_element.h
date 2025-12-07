// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class CSSPseudoElementsCacheData;
class V8UnionCSSPseudoElementOrElement;

// Implementation of CSSPseudoElement IDL interface.
// This class serves as a proxy object to work with a pseudo-element
// (blink::PseudoElement) from JS.
// The entry point to this class is Element::pseudo(type) which provides
// an identity-consistent (cached) CSSPseudoElement object which is cached
// in Element's ElementRareData. The identity consistency is needed
// for better developer experience, as most uses cases rely on object staying
// the same between two `.pseudo` calls (e.g. to add/remove event listeners).
// Spec: https://www.w3.org/TR/css-pseudo-4/#CSSPseudoElement-interface
class CSSPseudoElement final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPseudoElement(Element& originating_element, PseudoId pseudo_id);

  CSSPseudoElement(CSSPseudoElement& originating_pseudo_element,
                   PseudoId pseudo_id);

  // Parses the `type` to determine the PseudoId and if it's a currently
  // supported pseudo-element.
  static PseudoId ConvertTypeToSupportedPseudoId(const AtomicString& type);
  // Returns true if the `pseudo_id` is a supported pseudo-element type
  // for CSSPseudoElement interface.
  static bool IsSupportedTypeForCSSPseudoElement(PseudoId pseudo_id);

  // IDL interface.
  // The type attribute is a string representing the type of the pseudo-element.
  // For example, "::before" or "::after".
  String type() const;
  // The element attribute is the ultimate originating element of the
  // pseudo-element.
  Element* element() const { return element_; }
  // The parent attribute is the originating element of the pseudo-element. For
  // most pseudo-elements parent and element will return the same Element; for
  // sub-pseudo-elements, parent will return a CSSPseudoElement while element
  // returns an Element.
  V8UnionCSSPseudoElementOrElement* parent() const { return parent_; }
  // The pseudo(type) method returns the CSSPseudoElement interface representing
  // the sub-pseudo-element referenced in its argument, if such a
  // sub-pseudo-element could exist and would be valid, and null otherwise.
  CSSPseudoElement* pseudo(const AtomicString& type);

  PseudoId GetPseudoId() const { return pseudo_id_; }

  void Trace(Visitor* v) const final;

 private:
  PseudoId pseudo_id_;
  Member<Element> element_;
  Member<V8UnionCSSPseudoElementOrElement> parent_;
  Member<CSSPseudoElementsCacheData> css_pseudo_elements_data_;
};

// The cache of CSSPseudoElement objects for a Element(lives on
// ElementRareData)/CSSPseudoElement.
class CSSPseudoElementsCacheData
    : public GarbageCollected<CSSPseudoElementsCacheData>,
      public ElementRareDataField {
 public:
  void CacheCSSPseudoElement(PseudoId, CSSPseudoElement&);

  CSSPseudoElement* GetCSSPseudoElement(PseudoId);

  void Trace(Visitor* v) const final;

 private:
  HeapHashMap<PseudoId, WeakMember<CSSPseudoElement>> pseudo_elements_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_
