// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusable.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_element.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"

namespace blink {

// static
Focusable* Focusable::Create(const V8UnionCSSPseudoElementOrElement* target) {
  CHECK(target);
  if (target->IsCSSPseudoElement()) {
    return MakeGarbageCollected<Focusable>(
        base::PassKey<Focusable>(), /*shadow_host=*/nullptr,
        /*element=*/nullptr, target->GetAsCSSPseudoElement());
  }
  return MakeGarbageCollected<Focusable>(
      base::PassKey<Focusable>(), /*shadow_host=*/nullptr,
      target->GetAsElement(), /*pseudo_element=*/nullptr);
}

// static
Focusable* Focusable::CreateFromElement(Element& element,
                                        const TreeScope& caller_scope) {
  auto* pseudo_element = DynamicTo<PseudoElement>(&element);
  Element* originating_element =
      pseudo_element ? &pseudo_element->UltimateOriginatingElement() : &element;
  CHECK(!originating_element->IsPseudoElement());

  // Built-in controls like <input type="date"> or <video controls> use an
  // internal User-Agent shadow root with focusable inner sub-elements (such as
  // DateTimeFieldElement). Unwrap any UA shadow roots up to their host element
  // so that built-in controls are reported as regular `target` elements rather
  // than acting like author shadow hosts with `shadowHost` set and `target`
  // null.
  while (ShadowRoot* shadow_root =
             originating_element->ContainingShadowRoot()) {
    if (!shadow_root->IsUserAgent()) {
      break;
    }
    originating_element = originating_element->OwnerShadowHost();
    pseudo_element = nullptr;
  }

  // Retarget `originating_element` against `caller_scope` (walking up the
  // shadow host chain to find the host in `caller_scope`, just like
  // `DocumentOrShadowRoot.activeElement` does). If this yields another element,
  // the focusable item lives inside a shadow tree whose inner nodes are not
  // exposed to `caller_scope`, and the retargeted element is the outermost
  // shadow host in `caller_scope`, which is exposed as `shadowHost` instead of
  // `target` and `pseudoElement`.
  Element& retargeted = caller_scope.Retarget(*originating_element);
  Element* shadow_host =
      &retargeted != originating_element ? &retargeted : nullptr;

  // Track the pseudo-element through its CSSPseudoElement if possible (the
  // same cached one that `Event.pseudoTarget` exposes for focus events on it).
  // `CSSPseudoElement::From()` returns nullptr for pseudo-elements that the
  // CSSPseudoElement interface does not support (e.g. `::scroll-button()` or
  // `::column::scroll-marker`), which are tracked directly instead.
  CSSPseudoElement* css_pseudo_element =
      pseudo_element ? CSSPseudoElement::From(pseudo_element) : nullptr;
  Element* focusable_element = nullptr;
  if (!css_pseudo_element) {
    focusable_element = pseudo_element ? pseudo_element : originating_element;
  }
  return MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                         shadow_host, focusable_element,
                                         css_pseudo_element);
}

Focusable::Focusable(base::PassKey<Focusable>,
                     Element* shadow_host,
                     Element* element,
                     CSSPseudoElement* pseudo_element)
    : shadow_host_(shadow_host),
      element_(element),
      pseudo_element_(pseudo_element) {
  // See the member comments in the header.
  DCHECK_NE(!!element_, !!pseudo_element_);
}

Element* Focusable::target() const {
  if (shadow_host_) {
    return nullptr;
  }
  if (pseudo_element_) {
    return pseudo_element_->element();
  }
  if (auto* pseudo_element = DynamicTo<PseudoElement>(element_.Get())) {
    // A destroyed PseudoElement is disconnected, and no longer knows its
    // originating element (see `PseudoElement::Dispose()`).
    return pseudo_element->isConnected()
               ? &pseudo_element->UltimateOriginatingElement()
               : nullptr;
  }
  return element_.Get();
}

Element* Focusable::ResolveFocusTarget() const {
  if (pseudo_element_) {
    // Update style and layout first so that the underlying PseudoElement and
    // its LayoutObject (e.g., for ::scroll-marker) are created and have
    // up-to-date focusability state before lookup. This may be nullptr if the
    // pseudo-element does not currently exist, in which case there is nothing
    // to focus (just like `CSSPseudoElement.focus()`).
    pseudo_element_->element()->GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kFocus);
    return pseudo_element_->GetPseudoElement();
  }
  if (element_->IsPseudoElement()) {
    // Same as above, which also destroys the PseudoElement if it's no longer
    // needed, so that `isConnected()` below is up to date.
    element_->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kFocus);
  }
  // If the element was removed, or the pseudo-element was destroyed, there is
  // nothing to focus.
  return element_->isConnected() ? element_.Get() : nullptr;
}

void Focusable::focus(const FocusOptions* options) {
  if (Element* element = ResolveFocusTarget()) {
    element->focusForBindings(options);
  }
}

void Focusable::Trace(Visitor* visitor) const {
  visitor->Trace(shadow_host_);
  visitor->Trace(element_);
  visitor->Trace(pseudo_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
