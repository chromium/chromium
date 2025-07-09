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

#include "third_party/blink/renderer/core/css/shadow_tree_style_sheet_collection.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_observable_array_css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_candidate.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

ShadowTreeStyleSheetCollection::ShadowTreeStyleSheetCollection(
    ShadowRoot& shadow_root)
    : TreeScopeStyleSheetCollection(shadow_root) {}

void ShadowTreeStyleSheetCollection::CollectStyleSheets(
    StyleEngine& engine,
    StyleSheetCollection& collection) {
  StyleEngine::RuleSetScope rule_set_scope;

  for (Node* n : style_sheet_candidate_nodes_) {
    StyleSheetCandidate candidate(*n);
    DCHECK(!candidate.IsXSL());

    StyleSheet* sheet = candidate.Sheet();
    if (!sheet) {
      continue;
    }

    collection.AppendSheetForList(sheet);
    if (candidate.CanBeActivated(g_null_atom)) {
      CSSStyleSheet* css_sheet = To<CSSStyleSheet>(sheet);
      collection.AppendActiveStyleSheet(std::make_pair(
          css_sheet, rule_set_scope.RuleSetForSheet(engine, css_sheet)));
    }
  }

  const TreeScope& tree_scope = GetTreeScope();
  if (!tree_scope.HasAdoptedStyleSheets()) {
    return;
  }

  for (CSSStyleSheet* sheet : *tree_scope.AdoptedStyleSheets()) {
    if (!sheet || !sheet->CanBeActivated(g_null_atom)) {
      continue;
    }
    DCHECK_EQ(GetTreeScope().GetDocument(), sheet->ConstructorDocument());
    collection.AppendActiveStyleSheet(
        std::make_pair(sheet, engine.RuleSetForSheet(*sheet)));
  }
}

void ShadowTreeStyleSheetCollection::UpdateActiveStyleSheets(
    StyleEngine& engine) {
  // StyleSheetCollection is GarbageCollected<>, allocate it on the heap.
  auto* collection = MakeGarbageCollected<StyleSheetCollection>();
  CollectStyleSheets(engine, *collection);
  ApplyActiveStyleSheetChanges(*collection);
}

}  // namespace blink
