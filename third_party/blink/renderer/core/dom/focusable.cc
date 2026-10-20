// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusable.h"

#include <utility>

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focusable_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_csspseudoelement_element.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

namespace {

// Returns all shadow hosts that contain `element` (or the originating element
// of a pseudo-element) in a shadow tree that is not exposed to `caller_scope`,
// i.e. `element` is inside the shadow host's shadow tree, or slotted into it,
// ordered from outermost to innermost.
HeapVector<Member<Element>> FullShadowHostChain(Element& element,
                                                const TreeScope& caller_scope) {
  DCHECK_EQ(&element.GetDocument(), &caller_scope.GetDocument());
  Element* current = &element;
  if (auto* pseudo_element = DynamicTo<PseudoElement>(current)) {
    current = &pseudo_element->UltimateOriginatingElement();
  }
  // Walk up the flat tree, so that this finds the shadow trees that `element`
  // is slotted into as well (via the slots it's assigned to). Whenever a
  // ShadowRoot boundary is reached (or when `current` is not in the flat tree,
  // e.g. if it isn't assigned to a slot, so `FlatTreeTraversal::ParentElement`
  // returns nullptr while `current` is still inside a shadow tree), record its
  // shadow host and continue from the host.
  HeapVector<Member<Element>> shadow_host_chain;
  while (current) {
    if (!IsA<ShadowRoot>(current->parentNode())) {
      if (Element* parent = FlatTreeTraversal::ParentElement(*current)) {
        current = parent;
        continue;
      }
    }
    ShadowRoot* shadow_root = current->ContainingShadowRoot();
    if (!shadow_root) {
      break;
    }
    // User-Agent shadow trees (e.g. of <details>) shouldn't hide anything
    // slotted into them. Built-in controls with focusable elements in their
    // User-Agent shadow tree are exposed as the control itself instead (see
    // `CreateFromElement()`).
    if (!shadow_root->IsUserAgent() &&
        !shadow_root->IsInclusiveAncestorOf(caller_scope)) {
      shadow_host_chain.push_back(&shadow_root->host());
    }
    current = &shadow_root->host();
  }
  shadow_host_chain.Reverse();
  return shadow_host_chain;
}

// Returns the outermost shadow host that contains `element` in a shadow tree
// that is not exposed to `caller_scope` and is not one of
// `exposed_shadow_roots`, or nullptr if all containing shadow trees are
// exposed.
Element* ContainingOpaqueShadowHost(
    Element& element,
    const TreeScope& caller_scope,
    const Focusable::ShadowRootSet& exposed_shadow_roots) {
  for (Element* host : FullShadowHostChain(element, caller_scope)) {
    if (!exposed_shadow_roots.Contains(host->GetShadowRoot())) {
      return host;
    }
  }
  return nullptr;
}

Focusable::ShadowRootSet ToShadowRootSet(
    const HeapVector<Member<ShadowRoot>>& shadow_roots) {
  Focusable::ShadowRootSet result;
  for (const auto& shadow_root : shadow_roots) {
    result.insert(shadow_root);
  }
  return result;
}

}  // namespace

// static
Focusable* Focusable::Create(const V8UnionCSSPseudoElementOrElement* target,
                             const FocusableOptions* options) {
  CHECK(target);
  auto* focusable =
      target->IsCSSPseudoElement()
          ? MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                            *target->GetAsCSSPseudoElement())
          : MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                            *target->GetAsElement());
  // `target` itself is exposed, so `exposedShadowRoots` only matters for
  // `nextFocusable()` and `previousFocusable()`.
  focusable->exposed_shadow_roots_ =
      ToShadowRootSet(options->exposedShadowRoots());
  return focusable;
}

// static
Focusable* Focusable::Create(Focusable* other,
                             const FocusableOptions* options) {
  CHECK(other);
  auto* focusable =
      MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(), *other);
  focusable->exposed_shadow_roots_ =
      ToShadowRootSet(options->exposedShadowRoots());
  focusable->UpdateShadowHostChain();
  return focusable;
}

// static
Focusable* Focusable::CreateFromElement(Element& element,
                                        const TreeScope& caller_scope) {
  return CreateFromElement(element, caller_scope, ShadowRootSet());
}

// static
Focusable* Focusable::CreateFromElement(
    Element& element,
    const TreeScope& caller_scope,
    const ShadowRootSet& exposed_shadow_roots) {
  auto* pseudo_element = DynamicTo<PseudoElement>(&element);
  Element* originating_element =
      pseudo_element ? &pseudo_element->UltimateOriginatingElement() : &element;
  CHECK(!originating_element->IsPseudoElement());

  // Built-in controls like <input type="date"> or <video controls> use an
  // internal User-Agent shadow root with focusable inner sub-elements (such as
  // DateTimeFieldElement). Unwrap any UA shadow roots up to their host element
  // so that built-in controls are reported as regular `target` elements rather
  // than acting like author shadow hosts with a non-empty `shadowHostChain`
  // and `target` null.
  while (ShadowRoot* shadow_root =
             originating_element->ContainingShadowRoot()) {
    if (!shadow_root->IsUserAgent()) {
      break;
    }
    originating_element = originating_element->OwnerShadowHost();
    pseudo_element = nullptr;
  }

  Focusable* focusable = nullptr;

  // Track the pseudo-element through its CSSPseudoElement if possible (the
  // same cached one that `Event.pseudoTarget` exposes for focus events on it).
  // `CSSPseudoElement::From()` returns nullptr for pseudo-elements that the
  // CSSPseudoElement interface does not support (e.g.
  // `::column::scroll-marker`), which are tracked directly instead.
  if (pseudo_element) {
    if (CSSPseudoElement* css_pseudo_element =
            CSSPseudoElement::From(pseudo_element)) {
      focusable = MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                                  *css_pseudo_element);
    }
  }
  if (!focusable) {
    // TODO(crbug.com/565786176): Replace this with `CHECK(!pseudo_element)`
    // once all focusable pseudo-elements can be represented as
    // CSSPseudoElement.
    Element& focusable_element =
        pseudo_element ? *pseudo_element : *originating_element;
    focusable = MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                                focusable_element);
  }
  focusable->full_shadow_host_chain_ =
      FullShadowHostChain(*originating_element, caller_scope);
  focusable->exposed_shadow_roots_ = exposed_shadow_roots;
  focusable->UpdateShadowHostChain();
  return focusable;
}

// static
Focusable* Focusable::FindAdjacentFocusable(Element& start,
                                            const TreeScope& caller_scope,
                                            mojom::blink::FocusType type) {
  return FindAdjacentFocusable(start, caller_scope, type, ShadowRootSet());
}

// static
Focusable* Focusable::FindAdjacentFocusable(
    Element& start,
    const TreeScope& caller_scope,
    mojom::blink::FocusType type,
    const ShadowRootSet& exposed_shadow_roots) {
  Page* page = start.GetDocument().GetPage();
  if (!page) {
    return nullptr;
  }
  // A shadow host whose shadow tree is not exposed to `caller_scope` (and is
  // not one of `exposed_shadow_roots`) is a single entry in the sequential
  // focus order: everything that is focusable inside its shadow tree, or
  // slotted into it, is exposed as that shadow host once. So if `start` is
  // part of such an entry, the rest of it is skipped, rather than returning
  // the same shadow host again.
  Element* start_shadow_host =
      ContainingOpaqueShadowHost(start, caller_scope, exposed_shadow_roots);
  Element* current = &start;
  while (true) {
    Element* found =
        page->GetFocusController().FindAdjacentFocusableElementFrom(*current,
                                                                    type);
    if (!found) {
      return nullptr;
    }
    Focusable* focusable =
        CreateFromElement(*found, caller_scope, exposed_shadow_roots);
    if (!start_shadow_host ||
        focusable->opaque_shadow_host_ != start_shadow_host) {
      return focusable;
    }
    current = found;
  }
}

Focusable::Focusable(base::PassKey<Focusable>, Element& element)
    : shadow_host_chain_(MakeGarbageCollected<FrozenArray<Element>>()),
      element_(&element) {
  if (auto* pseudo_element = DynamicTo<PseudoElement>(element)) {
    originating_element_ = &pseudo_element->UltimateOriginatingElement();
  }
}

Focusable::Focusable(base::PassKey<Focusable>, CSSPseudoElement& pseudo_element)
    : shadow_host_chain_(MakeGarbageCollected<FrozenArray<Element>>()),
      pseudo_element_(&pseudo_element) {}

Focusable::Focusable(base::PassKey<Focusable>, const Focusable& other)
    : full_shadow_host_chain_(other.full_shadow_host_chain_),
      shadow_host_chain_(other.shadow_host_chain_),
      opaque_shadow_host_(other.opaque_shadow_host_),
      exposed_shadow_roots_(other.exposed_shadow_roots_),
      element_(other.element_),
      pseudo_element_(other.pseudo_element_),
      originating_element_(other.originating_element_) {}

void Focusable::UpdateShadowHostChain() {
  opaque_shadow_host_ = nullptr;
  HeapVector<Member<Element>> shadow_host_chain;
  for (const Member<Element>& host : full_shadow_host_chain_) {
    shadow_host_chain.push_back(host);
    if (!exposed_shadow_roots_.Contains(host->GetShadowRoot())) {
      opaque_shadow_host_ = host.Get();
      break;
    }
  }
  shadow_host_chain_ =
      MakeGarbageCollected<FrozenArray<Element>>(std::move(shadow_host_chain));
}

Element* Focusable::target() const {
  if (opaque_shadow_host_) {
    return nullptr;
  }
  if (pseudo_element_) {
    return pseudo_element_->element();
  }
  if (originating_element_) {
    return originating_element_.Get();
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

const TreeScope* Focusable::CallerTreeScope() const {
  if (!full_shadow_host_chain_.empty()) {
    return &full_shadow_host_chain_.front()->GetTreeScope();
  }
  if (Element* target_element = target()) {
    return &target_element->GetTreeScope();
  }
  return nullptr;
}

void Focusable::focus(const FocusOptions* options) {
  if (Element* element = ResolveFocusTarget()) {
    element->focusForBindings(options);
  }
}

Focusable* Focusable::FindAdjacentFocusable(
    mojom::blink::FocusType type) const {
  Element* current = ResolveFocusTarget();
  const TreeScope* scope = CallerTreeScope();
  // `current` may have been moved to another document since this Focusable was
  // created.
  if (!current || !scope || &current->GetDocument() != &scope->GetDocument()) {
    return nullptr;
  }
  return FindAdjacentFocusable(*current, *scope, type, exposed_shadow_roots_);
}

Focusable* Focusable::nextFocusable() const {
  return FindAdjacentFocusable(mojom::blink::FocusType::kForward);
}

Focusable* Focusable::previousFocusable() const {
  return FindAdjacentFocusable(mojom::blink::FocusType::kBackward);
}

void Focusable::Trace(Visitor* visitor) const {
  visitor->Trace(full_shadow_host_chain_);
  visitor->Trace(shadow_host_chain_);
  visitor->Trace(opaque_shadow_host_);
  visitor->Trace(exposed_shadow_roots_);
  visitor->Trace(element_);
  visitor->Trace(pseudo_element_);
  visitor->Trace(originating_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
