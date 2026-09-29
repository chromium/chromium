// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPseudoElement;
class Element;
class FocusOptions;
class TreeScope;
class V8UnionCSSPseudoElementOrElement;

class CORE_EXPORT Focusable final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Focusable* Create(const V8UnionCSSPseudoElementOrElement* target);
  static Focusable* CreateFromElement(Element& element,
                                      const TreeScope& caller_scope);

  // Exactly one of `element` and `pseudo_element` must be set. See the member
  // comments below.
  Focusable(base::PassKey<Focusable>,
            Element* shadow_host,
            Element* element,
            CSSPseudoElement* pseudo_element);

  Element* shadowHost() const { return shadow_host_.Get(); }
  Element* target() const;
  CSSPseudoElement* pseudoElement() const {
    return shadow_host_ ? nullptr : pseudo_element_.Get();
  }

  void focus(const FocusOptions* options);

  void Trace(Visitor* visitor) const override;

 private:
  Element* ResolveFocusTarget() const;

  // Set if the focusable item is inside a shadow tree whose inner nodes are not
  // exposed to the caller's TreeScope (i.e. the DocumentOrShadowRoot whose
  // `activeFocusable` created this Focusable). It is the outermost shadow host
  // that is exposed, i.e. what `DocumentOrShadowRoot.activeElement` returns in
  // that case. `target` and `pseudoElement` are null whenever this is set, so
  // that nothing inside the shadow tree is exposed.
  //
  // This is computed once, when the Focusable is created, and doesn't change
  // afterwards (just like `Event.target`), even if the focusable item moves or
  // is removed. Computing it again later would leak shadow tree internals:
  // e.g. an element removed from a closed shadow tree is adopted into the
  // document's TreeScope, so retargeting it against the document would expose
  // the element itself.
  Member<Element> shadow_host_;

  // The focusable item, i.e. what `focus()` focuses, which `target` and
  // `pseudoElement` are derived from. Exactly one of these is set:
  // - `element_`, for elements, and for pseudo-elements that the
  //   CSSPseudoElement interface doesn't support (e.g. `::scroll-button()`).
  //   Just like `Event.pseudoTarget`, such a pseudo-element is only exposed
  //   through its ultimate originating element as `target`.
  // - `pseudo_element_`, for pseudo-elements that the CSSPseudoElement
  //   interface supports. It is exposed as `pseudoElement`, and its ultimate
  //   originating element as `target`.
  //
  // Style and DOM changes can destroy the PseudoElement of a pseudo-element and
  // later create a new one in its place (e.g. when the pseudo-element is
  // `display: none` for a while, or when its originating element is removed
  // and re-inserted). `new Focusable(element.pseudo("::after"))` can also refer
  // to a pseudo-element that doesn't exist yet. `pseudo_element_` handles all
  // of these, since CSSPseudoElement looks up the current PseudoElement
  // whenever it's needed. A PseudoElement in `element_`, on the other hand, can
  // only be focused while it exists, and its `target` becomes null once it's
  // destroyed, since a destroyed PseudoElement no longer knows its originating
  // element.
  // TODO(crbug.com/565786176): Track all pseudo-elements through
  // CSSPseudoElement, once it supports them.
  Member<Element> element_;
  Member<CSSPseudoElement> pseudo_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_
