// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_OR_SHADOW_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_OR_SHADOW_ROOT_H_

#include "third_party/blink/renderer/core/animation/document_animation.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class V8ObservableArrayCSSStyleSheet;

class DocumentOrShadowRoot {
  STATIC_ONLY(DocumentOrShadowRoot);

 public:
  static Element* activeElement(Document& document) {
    return document.ActiveElement();
  }

  static Element* activeElement(ShadowRoot& shadow_root) {
    return shadow_root.ActiveElement();
  }

  static StyleSheetList* styleSheets(Document& document) {
    return &document.StyleSheets();
  }

  static StyleSheetList* styleSheets(ShadowRoot& shadow_root) {
    return &shadow_root.StyleSheets();
  }

  static V8ObservableArrayCSSStyleSheet* adoptedStyleSheets(
      TreeScope& tree_scope) {
    return tree_scope.AdoptedStyleSheets();
  }

  static DOMSelection* getSelection(TreeScope& tree_scope) {
    return tree_scope.GetSelection();
  }

  static HeapVector<Member<Animation>> getAnimations(Document& document) {
    return document.GetDocumentAnimations().getAnimations(document);
  }

  static HeapVector<Member<Animation>> getAnimations(ShadowRoot& shadow_root) {
    return shadow_root.GetDocument().GetDocumentAnimations().getAnimations(
        shadow_root);
  }

  static Element* elementFromPoint(TreeScope& tree_scope, double x, double y) {
    return tree_scope.ElementFromPoint(x, y);
  }

  static HeapVector<Member<Element>> elementsFromPoint(TreeScope& tree_scope,
                                                       double x,
                                                       double y) {
    return tree_scope.ElementsFromPoint(x, y);
  }

  static Element* pointerLockElement(Document& document) {
    UseCounter::Count(document, WebFeature::kDocumentPointerLockElement);
    const Element* target = document.PointerLockElement();
    if (!target)
      return nullptr;
    return document.AdjustedElement(*target);
  }

  static Element* pointerLockElement(ShadowRoot& shadow_root) {
    UseCounter::Count(shadow_root.GetDocument(),
                      WebFeature::kShadowRootPointerLockElement);
    const Element* target = shadow_root.GetDocument().PointerLockElement();
    if (!target)
      return nullptr;
    return shadow_root.AdjustedElement(*target);
  }

  static Element* fullscreenElement(TreeScope& scope) {
    return Fullscreen::FullscreenElementForBindingFrom(scope);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_OR_SHADOW_ROOT_H_
