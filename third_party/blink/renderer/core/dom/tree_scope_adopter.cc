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

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"

namespace blink {

void TreeScopeAdopter::Execute() const {
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

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    UpdateTreeScope(node);

    if (will_move_to_new_document) {
      MoveNodeToNewDocument(node, old_document, new_document);
    } else if (node.HasRareData()) {
      NodeRareData* rare_data = node.RareData();
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
      if (will_move_to_new_document)
        MoveShadowTreeToNewDocument(*shadow, old_document, new_document);
    }
  }
}

void TreeScopeAdopter::MoveShadowTreeToNewDocument(
    ShadowRoot& shadow_root,
    Document& old_document,
    Document& new_document) const {
  DCHECK_NE(old_document, new_document);
  HeapVector<Member<CSSStyleSheet>> empty_vector;
  shadow_root.SetAdoptedStyleSheets(empty_vector);

  if (shadow_root.GetType() == ShadowRootType::V0) {
    new_document.SetShadowCascadeOrder(ShadowCascadeOrder::kShadowCascadeV0);
  } else if (shadow_root.IsV1() && !shadow_root.IsUserAgent()) {
    new_document.SetShadowCascadeOrder(ShadowCascadeOrder::kShadowCascadeV1);
  }
  MoveTreeToNewDocument(shadow_root, old_document, new_document);
}

void TreeScopeAdopter::MoveTreeToNewDocument(Node& root,
                                             Document& old_document,
                                             Document& new_document) const {
  DCHECK_NE(old_document, new_document);
  for (Node& node : NodeTraversal::InclusiveDescendantsOf(root)) {
    MoveNodeToNewDocument(node, old_document, new_document);

    auto* element = DynamicTo<Element>(node);
    if (!element)
      continue;

    if (HeapVector<Member<Attr>>* attrs = element->GetAttrNodeList()) {
      for (const auto& attr : *attrs)
        MoveTreeToNewDocument(*attr, old_document, new_document);
    }

    if (ShadowRoot* shadow_root = element->GetShadowRoot())
      MoveShadowTreeToNewDocument(*shadow_root, old_document, new_document);
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
    Document& new_document) const {
  DCHECK_NE(old_document, new_document);
  // Note: at the start of this function, node.document() may already have
  // changed to match |newDocument|, which is why |oldDocument| is passed in.

  if (node.HasRareData()) {
    NodeRareData* rare_data = node.RareData();
    if (rare_data->NodeLists())
      rare_data->NodeLists()->AdoptDocument(old_document, new_document);
  }

  node.WillMoveToNewDocument(old_document, new_document);
  old_document.MoveNodeIteratorsToNewDocument(node, new_document);

  if (node.GetCustomElementState() == CustomElementState::kCustom) {
    CustomElement::EnqueueAdoptedCallback(To<Element>(node), old_document,
                                          new_document);
  }

  if (auto* shadow_root = DynamicTo<ShadowRoot>(node))
    shadow_root->SetDocument(new_document);

#if DCHECK_IS_ON()
  g_did_move_to_new_document_was_called = false;
  g_old_document_did_move_to_new_document_was_called_with = &old_document;
#endif

  node.DidMoveToNewDocument(old_document);
#if DCHECK_IS_ON()
  DCHECK(g_did_move_to_new_document_was_called);
#endif
}

}  // namespace blink
