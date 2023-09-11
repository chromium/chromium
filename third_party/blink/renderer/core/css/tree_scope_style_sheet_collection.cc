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
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/tree_scope_style_sheet_collection.h"

#include "third_party/blink/renderer/core/css/active_style_sheets.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_candidate.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"

namespace blink {

TreeScopeStyleSheetCollection::TreeScopeStyleSheetCollection(
    TreeScope& tree_scope)
    : tree_scope_(tree_scope) {}

void TreeScopeStyleSheetCollection::AddStyleSheetCandidateNode(Node& node) {
  if (node.isConnected()) {
    style_sheet_candidate_nodes_.Add(&node);
  }
}

void TreeScopeStyleSheetCollection::ApplyActiveStyleSheetChanges(
    StyleSheetCollection& new_collection) {
  GetDocument().GetStyleEngine().ApplyRuleSetChanges(
      GetTreeScope(), ActiveStyleSheets(), new_collection.ActiveStyleSheets(),
      new_collection.RuleSetDiffs());
  new_collection.Swap(*this);
}

void TreeScopeStyleSheetCollection::UpdateStyleSheetList() {
  if (!sheet_list_dirty_) {
    return;
  }

  HeapVector<Member<StyleSheet>> new_list;
  for (Node* node : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*node);
    DCHECK(!candidate.IsXSL());
    if (candidate.IsEnabledAndLoading()) {
      continue;
    }
    if (StyleSheet* sheet = candidate.Sheet()) {
      new_list.push_back(sheet);
    }
  }
  SwapSheetsForSheetList(new_list);
}

void TreeScopeStyleSheetCollection::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(style_sheet_candidate_nodes_);
  StyleSheetCollection::Trace(visitor);
}

}  // namespace blink
