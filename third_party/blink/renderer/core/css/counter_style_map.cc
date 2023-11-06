// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style_map.h"

#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

namespace {

bool CounterStyleShouldOverride(Document& document,
                                const TreeScope* tree_scope,
                                const StyleRuleCounterStyle& new_rule,
                                const StyleRuleCounterStyle& existing_rule) {
  const CascadeLayerMap* cascade_layer_map =
      tree_scope ? tree_scope->GetScopedStyleResolver()->GetCascadeLayerMap()
                 : document.GetStyleEngine().GetUserCascadeLayerMap();
  if (!cascade_layer_map) {
    return true;
  }
  return cascade_layer_map->CompareLayerOrder(existing_rule.GetCascadeLayer(),
                                              new_rule.GetCascadeLayer()) <= 0;
}

}  // namespace

// static
CounterStyleMap* CounterStyleMap::GetUserCounterStyleMap(Document& document) {
  return document.GetStyleEngine().GetUserCounterStyleMap();
}

// static
CounterStyleMap* CounterStyleMap::GetAuthorCounterStyleMap(
    const TreeScope& scope) {
  if (!scope.GetScopedStyleResolver()) {
    return nullptr;
  }
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
  if (tree_scope) {
    DCHECK_EQ(document, &tree_scope->GetDocument());
  }
#endif
}

void CounterStyleMap::AddCounterStyles(const RuleSet& rule_set) {
  DCHECK(owner_document_);

  if (!rule_set.CounterStyleRules().size()) {
    return;
  }

  for (StyleRuleCounterStyle* rule : rule_set.CounterStyleRules()) {
    AtomicString name = rule->GetName();
    auto replaced_iter = counter_styles_.find(name);
    if (replaced_iter != counter_styles_.end()) {
      if (!CounterStyleShouldOverride(*owner_document_, tree_scope_, *rule,
                                      replaced_iter->value->GetStyleRule())) {
        continue;
      }
    }
    CounterStyle* counter_style = CounterStyle::Create(*rule);
    if (!counter_style) {
      continue;
    }
    if (replaced_iter != counter_styles_.end()) {
      replaced_iter->value->SetIsDirty();
    }
    counter_styles_.Set(rule->GetName(), counter_style);
  }

  owner_document_->GetStyleEngine().MarkCounterStylesNeedUpdate();
}

CounterStyleMap* CounterStyleMap::GetAncestorMap() const {
  if (tree_scope_) {
    // Resursively walk up to parent scope to find an author CounterStyleMap.
    for (TreeScope* scope = tree_scope_->ParentTreeScope(); scope;
         scope = scope->ParentTreeScope()) {
      if (CounterStyleMap* map = GetAuthorCounterStyleMap(*scope)) {
        return map;
      }
    }

    // Fallback to user counter style map
    if (CounterStyleMap* user_map = GetUserCounterStyleMap(*owner_document_)) {
      return user_map;
    }
  }

  // Author and user counter style maps fall back to UA
  if (owner_document_) {
    return GetUACounterStyleMap();
  }

  // UA counter style map doesn't have any fallback
  return nullptr;
}

CounterStyle* CounterStyleMap::FindCounterStyleAcrossScopes(
    const AtomicString& name) const {
  if (!owner_document_) {
    const auto& iter = counter_styles_.find(name);
    if (iter == counter_styles_.end()) {
      return nullptr;
    }
    if (iter->value) {
      return iter->value.Get();
    }
    return &const_cast<CounterStyleMap*>(this)->CreateUACounterStyle(name);
  }
  auto it = counter_styles_.find(name);
  if (it != counter_styles_.end()) {
    return it->value.Get();
  }
  return GetAncestorMap()->FindCounterStyleAcrossScopes(name);
}

void CounterStyleMap::ResolveExtendsFor(CounterStyle& counter_style) {
  DCHECK(counter_style.HasUnresolvedExtends());

  HeapVector<Member<CounterStyle>, 2> extends_chain;
  HeapHashSet<Member<CounterStyle>> unresolved_styles;
  extends_chain.push_back(&counter_style);
  do {
    unresolved_styles.insert(extends_chain.back());
    AtomicString extends_name = extends_chain.back()->GetExtendsName();
    extends_chain.push_back(FindCounterStyleAcrossScopes(extends_name));
  } while (extends_chain.back() &&
           extends_chain.back()->HasUnresolvedExtends() &&
           !unresolved_styles.Contains(extends_chain.back()));

  // If one or more @counter-style rules form a cycle with their extends values,
  // all of the counter styles participating in the cycle must be treated as if
  // they were extending the 'decimal' counter style instead.
  if (extends_chain.back() && extends_chain.back()->HasUnresolvedExtends()) {
    // Predefined counter styles should not have 'extends' cycles, otherwise
    // we'll enter an infinite recursion to look for 'decimal'.
    DCHECK(owner_document_)
        << "'extends' cycle detected for predefined counter style "
        << counter_style.GetName();
    CounterStyle* cycle_start = extends_chain.back();
    do {
      extends_chain.back()->ResolveExtends(CounterStyle::GetDecimal());
      extends_chain.pop_back();
    } while (extends_chain.back() != cycle_start);
  }

  CounterStyle* next = extends_chain.back();
  while (extends_chain.size() > 1u) {
    extends_chain.pop_back();
    if (next) {
      extends_chain.back()->ResolveExtends(*next);
    } else {
      // Predefined counter styles should not use inexistent 'extends' names,
      // otherwise we'll enter an infinite recursion to look for 'decimal'.
      DCHECK(owner_document_) << "Can't resolve 'extends: "
                              << extends_chain.back()->GetExtendsName()
                              << "' for predefined counter style "
                              << extends_chain.back()->GetName();
      extends_chain.back()->ResolveExtends(CounterStyle::GetDecimal());
      extends_chain.back()->SetHasInexistentReferences();
    }

    next = extends_chain.back();
  }
}

void CounterStyleMap::ResolveFallbackFor(CounterStyle& counter_style) {
  DCHECK(counter_style.HasUnresolvedFallback());
  AtomicString fallback_name = counter_style.GetFallbackName();
  CounterStyle* fallback_style = FindCounterStyleAcrossScopes(fallback_name);
  if (fallback_style) {
    counter_style.ResolveFallback(*fallback_style);
  } else {
    // UA counter styles shouldn't use inexistent fallback style names,
    // otherwise we'll enter an infinite recursion to look for 'decimal'.
    DCHECK(owner_document_)
        << "Can't resolve fallback " << fallback_name
        << " for predefined counter style " << counter_style.GetName();
    counter_style.ResolveFallback(CounterStyle::GetDecimal());
    counter_style.SetHasInexistentReferences();
  }
}

void CounterStyleMap::ResolveSpeakAsReferenceFor(CounterStyle& counter_style) {
  DCHECK(counter_style.HasUnresolvedSpeakAsReference());

  HeapVector<Member<CounterStyle>, 2> speak_as_chain;
  HeapHashSet<Member<CounterStyle>> unresolved_styles;
  speak_as_chain.push_back(&counter_style);
  do {
    unresolved_styles.insert(speak_as_chain.back());
    AtomicString speak_as_name = speak_as_chain.back()->GetSpeakAsName();
    speak_as_chain.push_back(FindCounterStyleAcrossScopes(speak_as_name));
  } while (speak_as_chain.back() &&
           speak_as_chain.back()->HasUnresolvedSpeakAsReference() &&
           !unresolved_styles.Contains(speak_as_chain.back()));

  if (!speak_as_chain.back()) {
    // If the specified style does not exist, this value is treated as 'auto'.
    DCHECK_GE(speak_as_chain.size(), 2u);
    speak_as_chain.pop_back();
    speak_as_chain.back()->ResolveInvalidSpeakAsReference();
    speak_as_chain.back()->SetHasInexistentReferences();
  } else if (speak_as_chain.back()->HasUnresolvedSpeakAsReference()) {
    // If a loop is detected when following 'speak-as' references, this value is
    // treated as 'auto' for the counter styles participating in the loop.
    CounterStyle* cycle_start = speak_as_chain.back();
    do {
      speak_as_chain.back()->ResolveInvalidSpeakAsReference();
      speak_as_chain.pop_back();
    } while (speak_as_chain.back() != cycle_start);
  }

  CounterStyle* back = speak_as_chain.back();
  while (speak_as_chain.size() > 1u) {
    speak_as_chain.pop_back();
    speak_as_chain.back()->ResolveSpeakAsReference(*back);
  }
}

void CounterStyleMap::ResolveReferences(
    HeapHashSet<Member<CounterStyleMap>>& visited_maps) {
  if (visited_maps.Contains(this)) {
    return;
  }
  visited_maps.insert(this);

  // References in ancestor scopes must be resolved first.
  if (CounterStyleMap* ancestor_map = GetAncestorMap()) {
    ancestor_map->ResolveReferences(visited_maps);
  }

  for (CounterStyle* counter_style : counter_styles_.Values()) {
    if (counter_style->HasUnresolvedExtends()) {
      ResolveExtendsFor(*counter_style);
    }
    if (counter_style->HasUnresolvedFallback()) {
      ResolveFallbackFor(*counter_style);
    }
    if (RuntimeEnabledFeatures::
            CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()) {
      if (counter_style->HasUnresolvedSpeakAsReference()) {
        ResolveSpeakAsReferenceFor(*counter_style);
      }
    }
  }
}

void CounterStyleMap::MarkDirtyCounterStyles(
    HeapHashSet<Member<CounterStyle>>& visited_counter_styles) {
  for (CounterStyle* counter_style : counter_styles_.Values()) {
    counter_style->TraverseAndMarkDirtyIfNeeded(visited_counter_styles);
  }

  // Replace dirty CounterStyles by clean ones with unresolved references.
  for (Member<CounterStyle>& counter_style_ref : counter_styles_.Values()) {
    if (counter_style_ref->IsDirty()) {
      CounterStyle* clean_style =
          MakeGarbageCollected<CounterStyle>(counter_style_ref->GetStyleRule());
      counter_style_ref = clean_style;
    }
  }
}

// static
void CounterStyleMap::MarkAllDirtyCounterStyles(
    Document& document,
    const HeapHashSet<Member<TreeScope>>& active_tree_scopes) {
  // Traverse all CounterStyle objects in the document to mark dirtiness.
  // We assume that there are not too many CounterStyle objects, so this won't
  // be a performance bottleneck.
  TRACE_EVENT0("blink", "CounterStyleMap::MarkAllDirtyCounterStyles");

  HeapHashSet<Member<CounterStyle>> visited_counter_styles;

  if (CounterStyleMap* user_map = GetUserCounterStyleMap(document)) {
    user_map->MarkDirtyCounterStyles(visited_counter_styles);
  }

  if (CounterStyleMap* document_map = GetAuthorCounterStyleMap(document)) {
    document_map->MarkDirtyCounterStyles(visited_counter_styles);
  }

  for (const TreeScope* scope : active_tree_scopes) {
    if (CounterStyleMap* scoped_map = GetAuthorCounterStyleMap(*scope)) {
      scoped_map->MarkDirtyCounterStyles(visited_counter_styles);
    }
  }
}

// static
void CounterStyleMap::ResolveAllReferences(
    Document& document,
    const HeapHashSet<Member<TreeScope>>& active_tree_scopes) {
  // Traverse all counter style maps to find and update CounterStyles that are
  // dirty or have unresolved references. We assume there are not too many
  // CounterStyles, so that this won't be a performance bottleneck.
  TRACE_EVENT0("blink", "CounterStyleMap::ResolveAllReferences");

  HeapHashSet<Member<CounterStyleMap>> visited_maps;
  visited_maps.insert(GetUACounterStyleMap());

  if (CounterStyleMap* user_map = GetUserCounterStyleMap(document)) {
    user_map->ResolveReferences(visited_maps);
  }

  if (CounterStyleMap* document_map = GetAuthorCounterStyleMap(document)) {
    document_map->ResolveReferences(visited_maps);
  }

  for (const TreeScope* scope : active_tree_scopes) {
    if (CounterStyleMap* scoped_map = GetAuthorCounterStyleMap(*scope)) {
      scoped_map->ResolveReferences(visited_maps);

#if DCHECK_IS_ON()
      for (CounterStyle* counter_style : scoped_map->counter_styles_.Values()) {
        DCHECK(!counter_style->IsDirty());
        DCHECK(!counter_style->HasUnresolvedExtends());
        DCHECK(!counter_style->HasUnresolvedFallback());
        DCHECK(!counter_style->HasUnresolvedSpeakAsReference());
      }
#endif
    }
  }
}

void CounterStyleMap::Dispose() {
  if (!counter_styles_.size()) {
    return;
  }

  for (CounterStyle* counter_style : counter_styles_.Values()) {
    counter_style->SetIsDirty();
  }
  counter_styles_.clear();

  if (owner_document_) {
    owner_document_->GetStyleEngine().MarkCounterStylesNeedUpdate();
  }
}

void CounterStyleMap::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(tree_scope_);
  visitor->Trace(counter_styles_);
}

}  // namespace blink
