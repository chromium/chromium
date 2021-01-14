// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style_map.h"

#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

namespace {

const char* predefined_symbol_markers[] = {
    "disc", "square", "circle", "disclosure-open", "disclosure-closed"};

CounterStyleMap* CreateUACounterStyleMap() {
  CounterStyleMap* map =
      MakeGarbageCollected<CounterStyleMap>(nullptr, nullptr);
  map->AddCounterStyles(*CSSDefaultStyleSheets::Instance().DefaultStyle());
  for (const char* symbol_marker : predefined_symbol_markers) {
    map->FindCounterStyleAcrossScopes(symbol_marker)
        .SetIsPredefinedSymbolMarker();
  }
  map->ResolveReferences();
  return map;
}

}  // namespace

// static
CounterStyleMap* CounterStyleMap::GetUACounterStyleMap() {
  DEFINE_STATIC_LOCAL(Persistent<CounterStyleMap>, ua_counter_style_map,
                      (CreateUACounterStyleMap()));
  return ua_counter_style_map;
}

// static
CounterStyleMap* CounterStyleMap::GetUserCounterStyleMap(Document& document) {
  return document.GetStyleEngine().GetUserCounterStyleMap();
}

// static
CounterStyleMap* CounterStyleMap::GetAuthorCounterStyleMap(
    const TreeScope& scope) {
  if (!scope.GetScopedStyleResolver())
    return nullptr;
  return scope.GetScopedStyleResolver()->GetCounterStyleMap();
}

// static
CounterStyleMap* CounterStyleMap::CreateUserCounterStyleMap(
    Document& document) {
  return MakeGarbageCollected<CounterStyleMap>(&document, nullptr);
}

// static
CounterStyleMap* CounterStyleMap::CreateAuthorCounterStyleMap(
    TreeScope& tree_scope) {
  return MakeGarbageCollected<CounterStyleMap>(&tree_scope.GetDocument(),
                                               &tree_scope);
}

CounterStyleMap::CounterStyleMap(Document* document, TreeScope* tree_scope)
    : owner_document_(document), tree_scope_(tree_scope) {
#if DCHECK_IS_ON()
  if (tree_scope)
    DCHECK_EQ(document, &tree_scope->GetDocument());
#endif
}

void CounterStyleMap::AddCounterStyles(const RuleSet& rule_set) {
  for (StyleRuleCounterStyle* rule : rule_set.CounterStyleRules()) {
    CounterStyle* counter_style = CounterStyle::Create(*rule);
    if (!counter_style)
      continue;
    counter_styles_.Set(rule->GetName(), counter_style);
    if (counter_style->HasUnresolvedExtends() ||
        counter_style->HasUnresolvedFallback())
      has_unresolved_references_ = true;
  }
}

CounterStyleMap* CounterStyleMap::GetAncestorMap() const {
  if (tree_scope_) {
    // Resursively walk up to parent scope to find an author CounterStyleMap.
    for (TreeScope* scope = tree_scope_->ParentTreeScope(); scope;
         scope = scope->ParentTreeScope()) {
      if (CounterStyleMap* map = GetAuthorCounterStyleMap(*scope))
        return map;
    }

    // Fallback to user counter style map
    if (CounterStyleMap* user_map = GetUserCounterStyleMap(*owner_document_))
      return user_map;
  }

  // Author and user counter style maps fall back to UA
  if (owner_document_)
    return GetUACounterStyleMap();

  // UA counter style map doesn't have any fallback
  return nullptr;
}

CounterStyle& CounterStyleMap::FindCounterStyleAcrossScopes(
    const AtomicString& name) const {
  if (CounterStyle* style = counter_styles_.at(name))
    return *style;

  if (CounterStyleMap* ancestor_map = GetAncestorMap())
    return ancestor_map->FindCounterStyleAcrossScopes(name);

  return CounterStyle::GetDecimal();
}

void CounterStyleMap::ResolveExtendsFor(CounterStyle& counter_style) {
  DCHECK(counter_style.HasUnresolvedExtends());

  HeapVector<Member<CounterStyle>, 2> extends_chain;
  HeapHashSet<Member<CounterStyle>> unresolved_styles;
  extends_chain.push_back(&counter_style);
  do {
    unresolved_styles.insert(extends_chain.back());
    AtomicString extends_name = extends_chain.back()->GetExtendsName();
    extends_chain.push_back(&FindCounterStyleAcrossScopes(extends_name));
  } while (extends_chain.back()->HasUnresolvedExtends() &&
           !unresolved_styles.Contains(extends_chain.back()));

  // If one or more @counter-style rules form a cycle with their extends values,
  // all of the counter styles participating in the cycle must be treated as if
  // they were extending the 'decimal' counter style instead.
  if (extends_chain.back()->HasUnresolvedExtends()) {
    CounterStyle* cycle_start = extends_chain.back();
    do {
      extends_chain.back()->ResolveExtends(CounterStyle::GetDecimal());
      extends_chain.pop_back();
    } while (extends_chain.back() != cycle_start);
  }

  CounterStyle* next = extends_chain.back();
  while (extends_chain.size() > 1u) {
    extends_chain.pop_back();
    extends_chain.back()->ResolveExtends(*next);
    next = extends_chain.back();
  }
}

void CounterStyleMap::ResolveFallbackFor(CounterStyle& counter_style) {
  DCHECK(counter_style.HasUnresolvedFallback());
  AtomicString fallback_name = counter_style.GetFallbackName();
  CounterStyle& fallback_style = FindCounterStyleAcrossScopes(fallback_name);
  counter_style.ResolveFallback(fallback_style);
}

void CounterStyleMap::ResolveReferences() {
  // References in ancestor scopes must be resolved first.
  if (ancestors_have_unresolved_references_) {
    if (CounterStyleMap* ancestor_map = GetAncestorMap())
      ancestor_map->ResolveReferences();
    ancestors_have_unresolved_references_ = false;
  }

  if (!has_unresolved_references_)
    return;
  has_unresolved_references_ = false;
  for (auto iter : counter_styles_) {
    if (iter.value->HasUnresolvedExtends())
      ResolveExtendsFor(*iter.value);
    if (iter.value->HasUnresolvedFallback())
      ResolveFallbackFor(*iter.value);
  }
}

void CounterStyleMap::ResetReferences() {
  for (auto iter : counter_styles_) {
    CounterStyle* counter_style = iter.value;
    counter_style->ResetExtends();
    counter_style->ResetFallback();
    if (counter_style->HasUnresolvedExtends() ||
        counter_style->HasUnresolvedFallback())
      has_unresolved_references_ = true;
  }
}

// static
void CounterStyleMap::ResolveAllReferences(
    Document& document,
    const HeapHashSet<Member<TreeScope>>& active_tree_scopes) {
  // Make sure the UA counter style map is already set up, so that we don't
  // enter a recursion when resolving references in user and author rules.
  GetUACounterStyleMap();

  if (CounterStyleMap* user_map = GetUserCounterStyleMap(document))
    user_map->ResolveReferences();

  if (CounterStyleMap* document_map = GetAuthorCounterStyleMap(document))
    document_map->ResolveReferences();

  // It is hard to keep track of whether we should update references in a
  // shadow tree scope. They may need update even when the active style
  // sheets remain unchanged in the scope, but some ancestor scope changed.
  // So we reset and re-resolve all shadow tree scopes unconditionally.
  // TODO(crbug.com/687225): This might need optimizations in some cases. For
  // example, we don't want to invalidate the whole document when inserting a
  // web component.
  for (const TreeScope* scope : active_tree_scopes) {
    if (CounterStyleMap* scoped_map = GetAuthorCounterStyleMap(*scope)) {
      scoped_map->ResetReferences();
      scoped_map->ancestors_have_unresolved_references_ = true;
    }
  }
  for (const TreeScope* scope : active_tree_scopes) {
    if (CounterStyleMap* scoped_map = GetAuthorCounterStyleMap(*scope))
      scoped_map->ResolveReferences();
  }
}

void CounterStyleMap::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(tree_scope_);
  visitor->Trace(counter_styles_);
}

}  // namespace blink
