/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "third_party/blink/renderer/core/dom/tree_scope_adopter.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"

namespace blink {

void TreeScopeAdopter::Execute() const {
  WillMoveTreeToNewDocument(*to_adopt_);
  MoveTreeToNewScope(*to_adopt_);
  Document& old_document = OldScope().GetDocument();
  if (old_document == NewScope().GetDocument())
    return;
  old_document.DidMoveTreeToNewDocument(*to_adopt_);
}

void TreeScopeAdopter::MoveTreeToNewScope(Node& root) const {
  DCHECK(NeedsScopeChange());

  // If an element is moved from a document and then eventually back again the
  // collection cache for that element may contain stale data as changes made to
  // it will have updated the DOMTreeVersion of the document it was moved to. By
  // increasing the DOMTreeVersion of the donating document here we ensure that
  // the collection cache will be invalidated as needed when the element is
  // moved back.
  Document& old_document = OldScope().GetDocument();
  Document& new_document = NewScope().GetDocument();
  bool will_move_to_new_document = old_document != new_document;
  bool is_document_unmodified_and_uninteracted =
      IsDocumentEligibleForFastAdoption(old_document);

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    UpdateTreeScope(node);

    if (will_move_to_new_document) {
      MoveNodeToNewDocument(node, old_document,
                            is_document_unmodified_and_uninteracted);
    } else if (NodeRareData* rare_data = node.RareData()) {
      if (rare_data->NodeLists())
        rare_data->NodeLists()->AdoptTreeScope();
    }

    auto* element = DynamicTo<Element>(node);
    if (!element)
      continue;

    if (HeapVector<Member<Attr>>* attrs = element->GetAttrNodeList()) {
      for (const auto& attr : *attrs)
        MoveTreeToNewScope(*attr);
    }

    if (ShadowRoot* shadow = element->GetShadowRoot()) {
      shadow->SetParentTreeScope(NewScope());
      if (will_move_to_new_document) {
        MoveShadowTreeToNewDocument(*shadow, old_document, new_document,
                                    is_document_unmodified_and_uninteracted);
      }
    }
  }
}

void TreeScopeAdopter::MoveShadowTreeToNewDocument(
    ShadowRoot& shadow_root,
    Document& old_document,
    Document& new_document,
    bool is_document_unmodified_and_uninteracted) const {
  DCHECK_NE(old_document, new_document);
  if (old_document.TemplateDocumentHost() != &new_document &&
      new_document.TemplateDocumentHost() != &old_document) {
    // If this is not a move from a document to a <template> within it or vice
    // versa, we need to clear |shadow_root|'s adoptedStyleSheets.
    shadow_root.ClearAdoptedStyleSheets();
  }

  if (!shadow_root.IsUserAgent()) {
    new_document.SetContainsShadowRoot();
  }

  shadow_root.SetDocument(new_document);

  if (shadow_root.registry()) {
    shadow_root.registry()->AssociatedWith(new_document);
  }

  MoveTreeToNewDocument(shadow_root, old_document, new_document,
                        is_document_unmodified_and_uninteracted);
}

void TreeScopeAdopter::MoveTreeToNewDocument(
    Node& root,
    Document& old_document,
    Document& new_document,
    bool is_document_unmodified_and_uninteracted) const {
  DCHECK_NE(old_document, new_document);
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    MoveNodeToNewDocument(node, old_document,
                          is_document_unmodified_and_uninteracted);

    auto* element = DynamicTo<Element>(node);
    if (!element)
      continue;

    if (HeapVector<Member<Attr>>* attrs = element->GetAttrNodeList()) {
      for (const auto& attr : *attrs) {
        MoveTreeToNewDocument(*attr, old_document, new_document,
                              is_document_unmodified_and_uninteracted);
      }
    }

    if (ShadowRoot* shadow_root = element->GetShadowRoot()) {
      MoveShadowTreeToNewDocument(*shadow_root, old_document, new_document,
                                  is_document_unmodified_and_uninteracted);
    }
  }
}

void TreeScopeAdopter::WillMoveTreeToNewDocument(Node& root) const {
  Document& old_document = OldScope().GetDocument();
  Document& new_document = NewScope().GetDocument();
  if (old_document == new_document)
    return;

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    DCHECK_EQ(old_document, node.GetDocument());
    node.WillMoveToNewDocument(new_document);

    if (auto* element = DynamicTo<Element>(node)) {
      if (ShadowRoot* shadow_root = element->GetShadowRoot())
        WillMoveTreeToNewDocument(*shadow_root);

      if (HeapVector<Member<Attr>>* attrs = element->GetAttrNodeList()) {
        for (const auto& attr : *attrs)
          WillMoveTreeToNewDocument(*attr);
      }
    }
  }
}

#if DCHECK_IS_ON()
static bool g_did_move_to_new_document_was_called = false;
static Document* g_old_document_did_move_to_new_document_was_called_with =
    nullptr;

void TreeScopeAdopter::EnsureDidMoveToNewDocumentWasCalled(
    Document& old_document) {
  DCHECK(!g_did_move_to_new_document_was_called);
  DCHECK_EQ(old_document,
            g_old_document_did_move_to_new_document_was_called_with);
  g_did_move_to_new_document_was_called = true;
}
#endif

inline void TreeScopeAdopter::UpdateTreeScope(Node& node) const {
  DCHECK(!node.IsTreeScope());
  DCHECK(node.GetTreeScope() == OldScope());
  node.SetTreeScope(new_scope_);
}

inline void TreeScopeAdopter::MoveNodeToNewDocument(
    Node& node,
    Document& old_document,
    bool is_document_unmodified_and_uninteracted) const {
  Document& new_document = node.GetDocument();
  DCHECK_NE(old_document, new_document);
  DCHECK_EQ(old_document, OldScope().GetDocument());
  DCHECK_EQ(new_document, NewScope().GetDocument());

  if (!is_document_unmodified_and_uninteracted) {
    // fast adoption can skip all the checks below
    if (NodeRareData* rare_data = node.RareData()) {
      if (rare_data->NodeLists()) {
        rare_data->NodeLists()->AdoptDocument(old_document, new_document);
      }
      if (old_document.HasMutationObservers()) {
        node.MoveMutationObserversToNewDocument(new_document);
      }
    }

    if (old_document.HasNodeIterators()) {
      old_document.MoveNodeIteratorsToNewDocument(node, new_document);
    }

    if (auto* element = DynamicTo<Element>(node)) {
      if (old_document.HasExplicitlySetAttrElements()) {
        old_document.MoveElementExplicitlySetAttrElementsMapToNewDocument(
            element, new_document);
      }
      if (old_document.HasCachedAttrAssociatedElements()) {
        old_document.MoveElementCachedAttrAssociatedElementsMapToNewDocument(
            element, new_document);
      }
    }

    if (old_document.HasAnyNodeWithEventListeners()) {
      node.MoveEventListenersToNewDocument(old_document, new_document);
    }
  } else {
    // DCHECK all the fast adoption conditions
    DCHECK(!old_document.HasNodeIterators());
    DCHECK(!old_document.HasRanges());
    DCHECK(!old_document.HasAnyNodeWithEventListeners());
    DCHECK(!old_document.HasMutationObservers());
    DCHECK(!old_document.ShouldInvalidateNodeListCaches());
    DCHECK(!old_document.HasExplicitlySetAttrElements());
    DCHECK(!old_document.HasCachedAttrAssociatedElements());
  }

  if (node.GetCustomElementState() == CustomElementState::kCustom) {
    CustomElement::EnqueueAdoptedCallback(To<Element>(node), old_document,
                                          new_document);
  }

#if DCHECK_IS_ON()
  g_did_move_to_new_document_was_called = false;
  g_old_document_did_move_to_new_document_was_called_with = &old_document;
#endif

  node.DidMoveToNewDocument(old_document);
#if DCHECK_IS_ON()
  DCHECK(g_did_move_to_new_document_was_called);
#endif
}

inline bool TreeScopeAdopter::IsDocumentEligibleForFastAdoption(
    Document& old_document) const {
  return !old_document.HasNodeIterators() && !old_document.HasRanges() &&
         !old_document.HasAnyNodeWithEventListeners() &&
         !old_document.HasMutationObservers() &&
         !old_document.ShouldInvalidateNodeListCaches() &&
         !old_document.HasExplicitlySetAttrElements() &&
         !old_document.HasCachedAttrAssociatedElements();
}

}  // namespace blink
