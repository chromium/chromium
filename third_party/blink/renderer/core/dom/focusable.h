// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CSSPseudoElement;
class Element;
class FocusableOptions;
class FocusOptions;
template <typename IDLType>
class FrozenArray;
class ShadowRoot;
class TreeScope;
class V8UnionCSSPseudoElementOrElement;

class CORE_EXPORT Focusable final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using ShadowRootSet = HeapHashSet<Member<ShadowRoot>>;

  // Called by the two constructors in focusable.idl. These are separate
  // overloads because the `Focusable` constructor is behind its own
  // [RuntimeEnabled] flag, which IDL cannot apply to a single member of a
  // union.
  static Focusable* Create(const V8UnionCSSPseudoElementOrElement* target,
                           const FocusableOptions* options);
  static Focusable* Create(Focusable* focusable,
                           const FocusableOptions* options);
  static Focusable* CreateFromElement(Element& element,
                                      const TreeScope& caller_scope);
  static Focusable* FindAdjacentFocusable(Element& start,
                                          const TreeScope& caller_scope,
                                          mojom::blink::FocusType type);

  // See the member comments below.
  Focusable(base::PassKey<Focusable>, Element& element);
  Focusable(base::PassKey<Focusable>, CSSPseudoElement& pseudo_element);
  Focusable(base::PassKey<Focusable>, const Focusable& other);

  const FrozenArray<Element>& shadowHostChain() const {
    return *shadow_host_chain_;
  }
  Element* target() const;
  CSSPseudoElement* pseudoElement() const {
    // `pseudo_element_` (like `element_`) is still set when it's hidden, since
    // `focus()` needs it (see the member comments below), so this has to check
    // `opaque_shadow_host_` to avoid exposing it.
    return opaque_shadow_host_ ? nullptr : pseudo_element_.Get();
  }

  void focus(const FocusOptions* options);
  Focusable* nextFocusable() const;
  Focusable* previousFocusable() const;

  void Trace(Visitor* visitor) const override;

 private:
  // Static so that both the public static helpers above (which have no
  // `Focusable` instance and pass an empty `ShadowRootSet`) and the member
  // `FindAdjacentFocusable()` (which passes `exposed_shadow_roots_`) can share
  // the implementation.
  static Focusable* CreateFromElement(
      Element& element,
      const TreeScope& caller_scope,
      const ShadowRootSet& exposed_shadow_roots);
  static Focusable* FindAdjacentFocusable(
      Element& start,
      const TreeScope& caller_scope,
      mojom::blink::FocusType type,
      const ShadowRootSet& exposed_shadow_roots);

  Focusable* FindAdjacentFocusable(mojom::blink::FocusType type) const;
  void UpdateShadowHostChain();
  Element* ResolveFocusTarget() const;
  const TreeScope* CallerTreeScope() const;

  // All shadow hosts that contain the focusable item in a shadow tree whose
  // inner nodes are not exposed to the caller's TreeScope (i.e. the
  // DocumentOrShadowRoot whose `activeFocusable` created this Focusable), or
  // slotted into one, ordered from outermost to innermost. This doesn't depend
  // on `exposed_shadow_roots_`.
  //
  // Note that `DocumentOrShadowRoot.activeElement` does return a focused light
  // DOM element that is slotted into such a shadow tree, since it's in the
  // caller's TreeScope. It's still not exposed here, though (unless the shadow
  // root is in `exposed_shadow_roots_`), since the shadow tree determines where
  // it is in the sequential focus order.
  //
  // `element_` or `pseudo_element_` is still set in that case, since `focus()`
  // focuses the focusable item itself, not the shadow host. Focusing the shadow
  // host instead would focus the host itself, nothing at all, or (with
  // `delegatesFocus`) whatever element in the shadow tree the host delegates
  // focus to, none of which is necessarily the focusable item.
  //
  // This is computed once, when the Focusable is created, and doesn't change
  // afterwards (just like `Event.target`), even if the focusable item moves or
  // is removed. Computing it again later would leak shadow tree internals:
  // e.g. an element removed from a closed shadow tree is adopted into the
  // document's TreeScope, so retargeting it against the document would expose
  // the element itself.
  HeapVector<Member<Element>> full_shadow_host_chain_;

  // The prefix of `full_shadow_host_chain_` exposed as `shadowHostChain`:
  // outermost first, ending with either the last shadow host (when all of their
  // shadow roots are in `exposed_shadow_roots_`) or `opaque_shadow_host_` (the
  // first shadow host whose shadow root is not in `exposed_shadow_roots_`).
  // Never null: this is an empty array when `full_shadow_host_chain_` is empty.
  Member<FrozenArray<Element>> shadow_host_chain_;

  // The outermost shadow host in `full_shadow_host_chain_` whose shadow root is
  // NOT in `exposed_shadow_roots_`, and therefore still hides the focusable
  // item from the caller.
  // - When null, all shadow trees containing the focusable item are exposed, so
  //   `target` / `pseudoElement` returns the focusable item.
  // - When non-null, `target` and `pseudoElement` return null, and
  //   `nextFocusable()` / `previousFocusable()` treat everything inside this
  //   host as a single focus stop.
  // Without `exposed_shadow_roots_`, this is `full_shadow_host_chain_.front()`
  // (or null if `full_shadow_host_chain_` is empty).
  //
  // For example, given:
  //
  //   <div id="host">
  //     <template shadowrootmode="open">  <!-- root1 -->
  //       <button id="a"></button>
  //       <div id="nested-host">
  //         <template shadowrootmode="open">  <!-- root2 -->
  //           <button id="b"></button>
  //         </template>
  //       </div>
  //     </template>
  //   </div>
  //
  // From the document, `full_shadow_host_chain_` is [#host] for #a and
  // [#host, #nested-host] for #b, while `shadow_host_chain_` and
  // `opaque_shadow_host_` depend on `exposed_shadow_roots_`:
  // - If `exposed_shadow_roots_` is empty (or has only root2), then
  //   `shadow_host_chain_` is [#host] and `opaque_shadow_host_` is #host for
  //   both #a and #b (`target` is null).
  // - If `exposed_shadow_roots_` has only root1, then for #a
  //   `shadow_host_chain_` is [#host] and `opaque_shadow_host_` is null
  //   (`target` is #a), while for #b `shadow_host_chain_` is
  //   [#host, #nested-host] and `opaque_shadow_host_` is #nested-host (`target`
  //   is null, and `nextFocusable()` from #b skips anything else inside
  //   #nested-host).
  // - If `exposed_shadow_roots_` has both root1 and root2, then
  //   `opaque_shadow_host_` is null for both #a (`shadow_host_chain_` is
  //   [#host], `target` is #a) and #b (`shadow_host_chain_` is
  //   [#host, #nested-host], `target` is #b).
  Member<Element> opaque_shadow_host_;

  // The shadow roots whose shadow trees the caller opted into exposing via
  // `new Focusable(..., {exposedShadowRoots})`. Focusables returned by
  // `nextFocusable()` and `previousFocusable()` keep this set.
  ShadowRootSet exposed_shadow_roots_;

  // The focusable item, i.e. what `focus()` focuses, which `target` and
  // `pseudoElement` are derived from. Exactly one of these is set:
  // - `element_`, for elements, and for pseudo-elements that the
  //   CSSPseudoElement interface doesn't support (e.g.
  //   `::column::scroll-marker`).
  //   Just like `Event.pseudoTarget`, such a pseudo-element is only exposed
  //   through its ultimate originating element as `target` (see
  //   `originating_element_`).
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
  // only be focused while it exists.
  // TODO(crbug.com/565786176): Track all pseudo-elements through
  // CSSPseudoElement, once it supports them.
  Member<Element> element_;
  Member<CSSPseudoElement> pseudo_element_;

  // Set if `element_` is a PseudoElement. It's the ultimate originating element
  // of the pseudo-element (e.g. the multicol container for
  // `::column::scroll-marker`, not the `::column`), which is exposed as
  // `target`. It's stored when the Focusable is created, since a destroyed
  // PseudoElement no longer knows its originating element (see
  // `PseudoElement::Dispose()`), and `target` should still be the ultimate
  // originating element then (just like `CSSPseudoElement.element`).
  Member<Element> originating_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUSABLE_H_
