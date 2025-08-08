/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHEET_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHEET_COLLECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/tree_ordered_list.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MediaQueryEvaluator;
class Node;
class StyleSheet;
class StyleEngine;

// StyleSheetCollection is responsible for keeping track of which style sheets
// are relevant for a given tree scope. Style sheets may be relevant for either
// or both of:
//
//  - DOM visibility purposes (the “style sheet list”), or
//  - Style calculation (“active style sheets”)
//
// The style sheets are collected from places like <link> nodes, <style> nodes,
// adopted style sheets, inspector-added style sheets and so on. (You tell the
// collection about relevant DOM nodes by calling AddStyleSheetCandidateNode()
// and RemoveStyleSheetCandidateNode().)
//
// The precise logic for which sheets are in which list (e.g., are adopted
// style sheets visible to the DOM or not?) will depend on what kind of
// tree scope you are interested in; the actual collection logic lies in
// subclasses DocumentStyleSheetCollection and ShadowTreeStyleSheetCollection.
//
// Note, however, that even ShadowTreeStyleSheetCollection only considers
// the list of stylesheets for _one_ tree scope, not any parent tree scopes.
// Thus, ScopedStyleResolver also needs to consider StyleSheetCollections
// for parent tree scopes (for nodes in Shadow DOM), since style sheets
// there may be relevant for ::host, ::part() and similar.
class CORE_EXPORT StyleSheetCollection
    : public GarbageCollected<StyleSheetCollection>,
      public NameClient {
 public:
  StyleSheetCollection(const StyleSheetCollection&) = delete;
  StyleSheetCollection& operator=(const StyleSheetCollection&) = delete;
  ~StyleSheetCollection() override = default;

  const ActiveStyleSheetVector& ActiveStyleSheets() const {
    return active_style_sheets_;
  }
  const HeapVector<Member<StyleSheet>>& StyleSheetsForStyleSheetList() const {
    return style_sheets_for_style_sheet_list_;
  }

  void MarkSheetListDirty() { sheet_list_dirty_ = true; }

  virtual void Trace(Visitor*) const;
  const char* GetHumanReadableName() const override {
    return "StyleSheetCollection";
  }

  void AddStyleSheetCandidateNode(Node&);
  void RemoveStyleSheetCandidateNode(Node& node) {
    style_sheet_candidate_nodes_.Remove(&node);
  }
  bool HasStyleSheetCandidateNodes() const {
    return !style_sheet_candidate_nodes_.IsEmpty();
  }

  virtual bool IsShadowTreeStyleSheetCollection() const { return false; }
  void UpdateStyleSheetList();

 protected:
  friend class StyleCascadeTest;  // For ReplaceActiveStyleSheets().

  explicit StyleSheetCollection(TreeScope&);

  // Called by child classes after collecting a new set of style sheets.
  // Creates RuleSets, notifies the StyleEngine and moves the new values into
  // place.
  void ReplaceActiveStyleSheets(
      const MediaQueryEvaluator& medium,
      ActiveStyleSheetVector new_active_style_sheets,
      HeapVector<Member<StyleSheet>> new_style_sheets_for_style_sheet_list);

  Document& GetDocument() const { return GetTreeScope().GetDocument(); }
  TreeScope& GetTreeScope() const { return *tree_scope_; }

  // TODO(sesse): We should not have protected data members.
  TreeOrderedList<Node> style_sheet_candidate_nodes_;

 private:
  Member<TreeScope> tree_scope_;
  HeapVector<Member<StyleSheet>> style_sheets_for_style_sheet_list_;
  ActiveStyleSheetVector active_style_sheets_;
  bool sheet_list_dirty_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_SHEET_COLLECTION_H_
