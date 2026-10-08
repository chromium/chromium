// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/focusable.h"

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
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

// Retargets `element` against `caller_scope`, like `TreeScope::Retarget()`,
// except that shadow trees in `shadow_roots` are treated as exposed instead of
// retargeting to their shadow host. `element` must be in the same document as
// `caller_scope`.
Element& RetargetWithShadowRoots(Element& element,
                                 const TreeScope& caller_scope,
                                 const Focusable::ShadowRootSet* shadow_roots) {
  DCHECK_EQ(&element.GetDocument(), &caller_scope.GetDocument());
  Element* retargeted = &element;
  for (const TreeScope* scope = &element.GetTreeScope();
       !scope->IsInclusiveAncestorOf(caller_scope);
       scope = scope->ParentTreeScope()) {
    auto& shadow_root = To<ShadowRoot>(scope->RootNode());
    if (!shadow_roots || !shadow_roots->Contains(&shadow_root)) {
      retargeted = &shadow_root.host();
    }
  }
  return *retargeted;
}

// Returns the outermost shadow host that contains `element` (or the originating
// element of a pseudo-element) in a shadow tree that is not exposed to
// `caller_scope`, i.e. `element` is inside the shadow host's shadow tree, or
// slotted into it, unless that shadow tree is one of `shadow_roots`. Returns
// nullptr if `element` is exposed to `caller_scope`.
Element* ContainingOpaqueShadowHost(
    Element& element,
    const TreeScope& caller_scope,
    const Focusable::ShadowRootSet* shadow_roots) {
  Element* current = &element;
  if (auto* pseudo_element = DynamicTo<PseudoElement>(current)) {
    current = &pseudo_element->UltimateOriginatingElement();
  }
  // Walk up the flat tree, so that this finds the shadow trees that `element`
  // is slotted into as well (via the slots it's assigned to), and retarget the
  // ancestors against `caller_scope`. The outermost ancestor that is not
  // exposed to `caller_scope` is retargeted to the outermost shadow host.
  Element* shadow_host = nullptr;
  for (; current; current = FlatTreeTraversal::ParentElement(*current)) {
    // User-Agent shadow trees (e.g. of <details>) shouldn't hide anything
    // slotted into them. Built-in controls with focusable elements in their
    // User-Agent shadow tree are exposed as the control itself instead (see
    // `CreateFromElement()`).
    if (current->IsInUserAgentShadowRoot()) {
      continue;
    }
    Element* retargeted =
        &RetargetWithShadowRoots(*current, caller_scope, shadow_roots);
    if (retargeted != current) {
      shadow_host = retargeted;
      // The flat tree ancestors of `current` up to `shadow_host` are all inside
      // its shadow tree too, so they would be retargeted to it as well. Skip
      // straight to it. This is also needed if `current` isn't in the flat tree
      // (e.g. if it isn't assigned to a slot), since walking up from it would
      // stop right away, missing any shadow tree `shadow_host` is slotted into.
      current = shadow_host;
    }
  }
  // Since `caller_scope` is never inside a User-Agent shadow tree, nothing
  // inside one is ever exposed as the shadow host.
  DCHECK(!shadow_host || !shadow_host->IsInUserAgentShadowRoot());
  return shadow_host;
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
                                            /*shadow_host=*/nullptr,
                                            *target->GetAsCSSPseudoElement())
          : MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                            /*shadow_host=*/nullptr,
                                            *target->GetAsElement());
  // `target` itself is exposed, so the shadow roots only matter for
  // `nextFocusable()` and `previousFocusable()`.
  focusable->shadow_roots_ = ToShadowRootSet(options->shadowRoots());
  return focusable;
}

// static
Focusable* Focusable::Create(Focusable* other,
                             const FocusableOptions* options) {
  CHECK(other);
  auto* focusable =
      MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(), *other);
  focusable->shadow_roots_ = ToShadowRootSet(options->shadowRoots());
  focusable->opaque_shadow_host_ = focusable->shadow_host_;
  // Expose the focusable item if `shadow_roots_` covers all the shadow trees
  // that hid it, provided the element is still in the same document and shadow
  // tree as when `other` was created.
  Element* target_element = focusable->TargetElement();
  if (focusable->shadow_host_ && target_element &&
      target_element->isConnected()) {
    const TreeScope& caller_scope = focusable->shadow_host_->GetTreeScope();
    if (&target_element->GetDocument() == &caller_scope.GetDocument() &&
        ContainingOpaqueShadowHost(*target_element, caller_scope,
                                   /*shadow_roots=*/nullptr) ==
            focusable->shadow_host_) {
      focusable->opaque_shadow_host_ = ContainingOpaqueShadowHost(
          *target_element, caller_scope, &focusable->shadow_roots_);
    }
  }
  return focusable;
}

// static
Focusable* Focusable::CreateFromElement(Element& element,
                                        const TreeScope& caller_scope) {
  return CreateFromElement(element, caller_scope, ShadowRootSet());
}

// static
Focusable* Focusable::CreateFromElement(Element& element,
                                        const TreeScope& caller_scope,
                                        const ShadowRootSet& shadow_roots) {
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

  // If the focusable item is inside a shadow tree whose inner nodes are not
  // exposed to `caller_scope`, the outermost such shadow host in
  // `caller_scope` is exposed as `shadowHost` (just like
  // `DocumentOrShadowRoot.activeElement`). Unlike `activeElement`, the same
  // goes for a light DOM element that is slotted into such a shadow tree (see
  // `shadow_host_`).
  Element* shadow_host = ContainingOpaqueShadowHost(
      *originating_element, caller_scope, /*shadow_roots=*/nullptr);
  // `target` and `pseudoElement` are exposed alongside `shadowHost` if all
  // the shadow trees that hide the focusable item are in `shadow_roots`.
  Element* opaque_shadow_host =
      shadow_host && !shadow_roots.empty()
          ? ContainingOpaqueShadowHost(*originating_element, caller_scope,
                                       &shadow_roots)
          : shadow_host;

  Focusable* focusable = nullptr;

  // Track the pseudo-element through its CSSPseudoElement if possible (the
  // same cached one that `Event.pseudoTarget` exposes for focus events on it).
  // `CSSPseudoElement::From()` returns nullptr for pseudo-elements that the
  // CSSPseudoElement interface does not support (e.g.
  // `::column::scroll-marker`), which are tracked directly instead.
  if (pseudo_element) {
    if (CSSPseudoElement* css_pseudo_element =
            CSSPseudoElement::From(pseudo_element)) {
      focusable = MakeGarbageCollected<Focusable>(
          base::PassKey<Focusable>(), shadow_host, *css_pseudo_element);
    }
  }
  if (!focusable) {
    // TODO(crbug.com/565786176): Replace this with `CHECK(!pseudo_element)`
    // once all focusable pseudo-elements can be represented as
    // CSSPseudoElement.
    Element& focusable_element =
        pseudo_element ? *pseudo_element : *originating_element;
    focusable = MakeGarbageCollected<Focusable>(base::PassKey<Focusable>(),
                                                shadow_host, focusable_element);
  }
  focusable->opaque_shadow_host_ = opaque_shadow_host;
  focusable->shadow_roots_ = shadow_roots;
  return focusable;
}

// static
Focusable* Focusable::FindAdjacentFocusable(Element& start,
                                            const TreeScope& caller_scope,
                                            mojom::blink::FocusType type) {
  return FindAdjacentFocusable(start, caller_scope, type, ShadowRootSet());
}

// static
Focusable* Focusable::FindAdjacentFocusable(Element& start,
                                            const TreeScope& caller_scope,
                                            mojom::blink::FocusType type,
                                            const ShadowRootSet& shadow_roots) {
  Page* page = start.GetDocument().GetPage();
  if (!page) {
    return nullptr;
  }
  // A shadow host whose shadow tree is not exposed to `caller_scope` (and is
  // not one of `shadow_roots`) is a single entry in the sequential focus order:
  // everything that is focusable inside its shadow tree, or slotted into it, is
  // exposed as that shadow host once. So if `start` is part of such an entry,
  // the rest of it is skipped, rather than returning the same shadow host
  // again.
  Element* start_shadow_host =
      ContainingOpaqueShadowHost(start, caller_scope, &shadow_roots);
  Element* current = &start;
  while (true) {
    Element* found =
        page->GetFocusController().FindAdjacentFocusableElementFrom(*current,
                                                                    type);
    if (!found) {
      return nullptr;
    }
    Focusable* focusable =
        CreateFromElement(*found, caller_scope, shadow_roots);
    if (!start_shadow_host ||
        focusable->opaque_shadow_host_ != start_shadow_host) {
      return focusable;
    }
    current = found;
  }
}

Focusable::Focusable(base::PassKey<Focusable>,
                     Element* shadow_host,
                     Element& element)
    : shadow_host_(shadow_host),
      opaque_shadow_host_(shadow_host),
      element_(&element) {
  if (auto* pseudo_element = DynamicTo<PseudoElement>(element)) {
    originating_element_ = &pseudo_element->UltimateOriginatingElement();
  }
}

Focusable::Focusable(base::PassKey<Focusable>,
                     Element* shadow_host,
                     CSSPseudoElement& pseudo_element)
    : shadow_host_(shadow_host),
      opaque_shadow_host_(shadow_host),
      pseudo_element_(&pseudo_element) {}

Focusable::Focusable(base::PassKey<Focusable>, const Focusable& other)
    : shadow_host_(other.shadow_host_),
      opaque_shadow_host_(other.opaque_shadow_host_),
      shadow_roots_(other.shadow_roots_),
      element_(other.element_),
      pseudo_element_(other.pseudo_element_),
      originating_element_(other.originating_element_) {}

Element* Focusable::target() const {
  return opaque_shadow_host_ ? nullptr : TargetElement();
}

Element* Focusable::TargetElement() const {
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
  if (shadow_host_) {
    return &shadow_host_->GetTreeScope();
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
  return FindAdjacentFocusable(*current, *scope, type, shadow_roots_);
}

Focusable* Focusable::nextFocusable() const {
  return FindAdjacentFocusable(mojom::blink::FocusType::kForward);
}

Focusable* Focusable::previousFocusable() const {
  return FindAdjacentFocusable(mojom::blink::FocusType::kBackward);
}

void Focusable::Trace(Visitor* visitor) const {
  visitor->Trace(shadow_host_);
  visitor->Trace(opaque_shadow_host_);
  visitor->Trace(shadow_roots_);
  visitor->Trace(element_);
  visitor->Trace(pseudo_element_);
  visitor->Trace(originating_element_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
