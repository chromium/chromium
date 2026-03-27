// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

class BoxQuadOptions;
class ConvertCoordinateOptions;
class CSSPseudoElementsCacheData;
class DOMPoint;
class DOMPointInit;
class DOMQuad;
class DOMQuadInit;
class DOMRectReadOnly;
class V8UnionCSSPseudoElementOrDocumentOrElementOrText;
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
  CSSPseudoElement(Element& originating_element,
                   PseudoId pseudo_id,
                   const AtomicString& pseudo_argument = g_null_atom);

  CSSPseudoElement(CSSPseudoElement& originating_pseudo_element,
                   PseudoId pseudo_id,
                   const AtomicString& pseudo_argument = g_null_atom);

  // Parses the `type` to determine the PseudoId and if it's a currently
  // supported pseudo-element. Optionally extracts the pseudo_argument.
  static std::pair<PseudoId, AtomicString> ConvertTypeToSupportedPseudoId(
      const AtomicString& type);
  // Returns true if the `pseudo_id` is a supported pseudo-element type
  // for CSSPseudoElement interface.
  static bool IsSupportedTypeForCSSPseudoElement(PseudoId pseudo_id);

  // Returns the parent view transition pseudo-element type and argument for a
  // given view transition pseudo-element type and argument.
  // View transitions follow a specific hierarchy:
  // ::view-transition
  //  └─ ::view-transition-group(name)
  //      └─ ::view-transition-image-pair(name)
  //          ├─ ::view-transition-old(name)
  //          └─ ::view-transition-new(name)
  // Returns {kPseudoIdNone, g_null_atom} if it has no parent pseudo-element.
  // This is used by Element to recursively build the proxy chain.
  static std::pair<PseudoId, AtomicString> GetViewTransitionParent(
      PseudoId pseudo_id,
      const AtomicString& pseudo_argument);

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
  // PseudoId-based overload: avoids string parsing, for internal use.
  CSSPseudoElement* pseudo(PseudoId pseudo_id,
                           const AtomicString& pseudo_argument = g_null_atom);

  // Returns the CSSPseudoElement proxy chain for the given PseudoElement,
  // creating it if necessary. Handles nested pseudos (e.g. ::after::marker)
  // by walking the parentElement() chain from innermost to outermost.
  static CSSPseudoElement* From(PseudoElement* pseudo_element);

  // GeometryUtils methods
  // https://drafts.csswg.org/cssom-view/#the-geometryutils-interface
  HeapVector<Member<DOMQuad>> getBoxQuads(const BoxQuadOptions* options) const;
  DOMQuad* convertQuadFromNode(
      DOMQuadInit* quad,
      const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
      const ConvertCoordinateOptions* options) const;
  DOMQuad* convertRectFromNode(
      DOMRectReadOnly* rect,
      const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
      const ConvertCoordinateOptions* options) const;
  DOMPoint* convertPointFromNode(
      DOMPointInit* point,
      const V8UnionCSSPseudoElementOrDocumentOrElementOrText* from,
      const ConvertCoordinateOptions* options) const;

  PseudoId GetPseudoId() const { return pseudo_id_; }
  const AtomicString& GetPseudoArgument() const { return pseudo_argument_; }

  LayoutObject* GetLayoutObject() const;

  void Trace(Visitor* v) const final;

 private:
  PseudoId pseudo_id_;
  AtomicString pseudo_argument_;
  Member<Element> element_;
  Member<V8UnionCSSPseudoElementOrElement> parent_;
  Member<CSSPseudoElementsCacheData> css_pseudo_elements_data_;
};

// PseudoElementCacheKey is used to uniquely identify a CSSPseudoElement proxy
// for a given originating element. It combines the pseudo-type and the
// optional argument (used for view transitions).
struct PseudoElementCacheKey {
  PseudoId pseudo_id;
  AtomicString pseudo_argument;

  PseudoElementCacheKey() : pseudo_id(kPseudoIdNone) {}
  PseudoElementCacheKey(PseudoId id, const AtomicString& arg)
      : pseudo_id(id), pseudo_argument(arg) {}

  bool operator==(const PseudoElementCacheKey& other) const {
    return pseudo_id == other.pseudo_id &&
           pseudo_argument == other.pseudo_argument;
  }
};

template <>
struct HashTraits<PseudoElementCacheKey>
    : SimpleClassHashTraits<PseudoElementCacheKey> {
  static unsigned GetHash(const PseudoElementCacheKey& key) {
    unsigned arg_hash =
        key.pseudo_argument.IsNull() ? 0 : key.pseudo_argument.Hash();
    return HashInts(key.pseudo_id, arg_hash);
  }
  static const bool kEmptyValueIsZero = false;
  static bool IsEmptyValue(const PseudoElementCacheKey& key) {
    return key.pseudo_id == kPseudoIdNone;
  }
  static void ConstructDeletedValue(PseudoElementCacheKey& slot) {
    HashTraits<AtomicString>::ConstructDeletedValue(slot.pseudo_argument);
  }
  static bool IsDeletedValue(const PseudoElementCacheKey& key) {
    return HashTraits<AtomicString>::IsDeletedValue(key.pseudo_argument);
  }
};

// The cache of CSSPseudoElement objects for a Element(lives on
// ElementRareData)/CSSPseudoElement.
class CSSPseudoElementsCacheData
    : public GarbageCollected<CSSPseudoElementsCacheData>,
      public ElementRareDataField {
 public:
  void CacheCSSPseudoElement(PseudoId, const AtomicString&, CSSPseudoElement&);

  CSSPseudoElement* GetCSSPseudoElement(PseudoId, const AtomicString&);

  void Trace(Visitor* v) const final;

 private:
  HeapHashMap<PseudoElementCacheKey, WeakMember<CSSPseudoElement>>
      pseudo_elements_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_PSEUDO_ELEMENT_H_
