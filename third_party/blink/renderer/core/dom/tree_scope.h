/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TREE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TREE_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/tree_ordered_map.h"
#include "third_party/blink/renderer/core/html/forms/radio_button_group_scope.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ContainerNode;
class CSSStyleSheet;
class DOMSelection;
class Document;
class Element;
class HTMLMapElement;
class HitTestResult;
class IdTargetObserverRegistry;
class SVGTreeScopeResources;
class ScopedStyleResolver;

// The root node of a document tree (in which case this is a Document) or of a
// shadow tree (in which case this is a ShadowRoot). Various things, like
// element IDs, are scoped to the TreeScope in which they are rooted, if any.
//
// A class which inherits both Node and TreeScope must call clearRareData() in
// its destructor so that the Node destructor no longer does problematic
// NodeList cache manipulation in the destructor.
class CORE_EXPORT TreeScope : public GarbageCollectedMixin {
 public:
  enum HitTestPointType {
    kInternal = 1 << 1,
    kWebExposed = 1 << 2,
  };

  TreeScope* ParentTreeScope() const { return parent_tree_scope_; }

  bool IsInclusiveAncestorTreeScopeOf(const TreeScope&) const;

  Element* AdjustedFocusedElement() const;
  // Finds a retargeted element to the given argument, when the retargeted
  // element is in this TreeScope. Returns null otherwise.
  // TODO(kochi): once this algorithm is named in the spec, rename the method
  // name.
  Element* AdjustedElement(const Element&) const;
  Element* getElementById(const AtomicString&) const;
  const HeapVector<Member<Element>>& GetAllElementsById(
      const AtomicString&) const;
  bool HasElementWithId(const AtomicString& id) const;
  bool ContainsMultipleElementsWithId(const AtomicString& id) const;
  void AddElementById(const AtomicString& element_id, Element&);
  void RemoveElementById(const AtomicString& element_id, Element&);

  Document& GetDocument() const {
    DCHECK(document_);
    return *document_;
  }

  Node* AncestorInThisScope(Node*) const;

  void AddImageMap(HTMLMapElement&);
  void RemoveImageMap(HTMLMapElement&);
  HTMLMapElement* GetImageMap(const String& url) const;

  Element* ElementFromPoint(double x, double y) const;
  Element* HitTestPoint(double x, double y, const HitTestRequest&) const;
  HeapVector<Member<Element>> ElementsFromPoint(double x, double y) const;
  HeapVector<Member<Element>> ElementsFromHitTestResult(HitTestResult&) const;

  DOMSelection* GetSelection() const;

  Element& Retarget(const Element& target) const;

  Element* AdjustedFocusedElementInternal(const Element& target) const;

  // Find first anchor which matches the given URL fragment.
  // First searches for an element with the given ID, but if that fails, then
  // looks for an anchor with the given name. ID matching is always case
  // sensitive, but Anchor name matching is case sensitive in strict mode and
  // not case sensitive in quirks mode for historical compatibility reasons.
  // First searches for the raw fragment if not an SVG document, then searches
  // with the URL decoded fragment.
  Node* FindAnchor(const String& fragment);

  // Used by the basic DOM mutation methods (e.g., appendChild()).
  void AdoptIfNeeded(Node&);

  ContainerNode& RootNode() const { return *root_node_; }

  IdTargetObserverRegistry& GetIdTargetObserverRegistry() const {
    return *id_target_observer_registry_.Get();
  }

  RadioButtonGroupScope& GetRadioButtonGroupScope() {
    return radio_button_group_scope_;
  }

  bool IsInclusiveAncestorOf(const TreeScope&) const;
  uint16_t ComparePosition(const TreeScope&) const;

  const TreeScope* CommonAncestorTreeScope(const TreeScope& other) const;
  TreeScope* CommonAncestorTreeScope(TreeScope& other);

  Element* GetElementByAccessKey(const String& key) const;

  void Trace(Visitor*) const override;

  ScopedStyleResolver* GetScopedStyleResolver() const {
    return scoped_style_resolver_.Get();
  }
  ScopedStyleResolver& EnsureScopedStyleResolver();
  void ClearScopedStyleResolver();

  SVGTreeScopeResources& EnsureSVGTreeScopedResources();

  bool HasAdoptedStyleSheets() const;
  const HeapVector<Member<CSSStyleSheet>>& AdoptedStyleSheets();
  void SetAdoptedStyleSheets(HeapVector<Member<CSSStyleSheet>>&);
  void SetAdoptedStyleSheets(HeapVector<Member<CSSStyleSheet>>&,
                             ExceptionState&);

 protected:
  TreeScope(ContainerNode&, Document&);
  TreeScope(Document&);
  virtual ~TreeScope();

  void ResetTreeScope();
  void SetDocument(Document& document) { document_ = &document; }
  void SetParentTreeScope(TreeScope&);
  void SetNeedsStyleRecalcForViewportUnits();

 private:
  Element* HitTestPointInternal(Node*, HitTestPointType) const;
  Element* FindAnchorWithName(const String& name);

  Member<ContainerNode> root_node_;
  Member<Document> document_;
  Member<TreeScope> parent_tree_scope_;

  Member<TreeOrderedMap> elements_by_id_;
  Member<TreeOrderedMap> image_maps_by_name_;

  Member<IdTargetObserverRegistry> id_target_observer_registry_;

  Member<ScopedStyleResolver> scoped_style_resolver_;

  mutable Member<DOMSelection> selection_;

  RadioButtonGroupScope radio_button_group_scope_;

  Member<SVGTreeScopeResources> svg_tree_scoped_resources_;

  HeapVector<Member<CSSStyleSheet>> adopted_style_sheets_;
};

inline bool TreeScope::HasElementWithId(const AtomicString& id) const {
  DCHECK(!id.IsNull());
  return elements_by_id_ && elements_by_id_->Contains(id);
}

inline bool TreeScope::ContainsMultipleElementsWithId(
    const AtomicString& id) const {
  return elements_by_id_ && elements_by_id_->ContainsMultiple(id);
}

DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(TreeScope)

HitTestResult HitTestInDocument(
    Document*,
    double x,
    double y,
    const HitTestRequest& = HitTestRequest::kReadOnly |
                            HitTestRequest::kActive);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TREE_SCOPE_H_
