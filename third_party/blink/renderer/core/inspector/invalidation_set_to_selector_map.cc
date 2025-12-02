// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_tracing_flag.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/selector_statistics_flag.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/inspector/style_rule_to_style_sheet_contents_map.h"

namespace blink {

InvalidationSetToSelectorMap::IndexedSelector::IndexedSelector(
    StyleRule* style_rule,
    unsigned selector_index)
    : style_rule_(style_rule), selector_index_(selector_index) {}

void InvalidationSetToSelectorMap::IndexedSelector::Trace(
    Visitor* visitor) const {
  visitor->Trace(style_rule_);
}

StyleRule* InvalidationSetToSelectorMap::IndexedSelector::GetStyleRule() const {
  return style_rule_;
}

unsigned InvalidationSetToSelectorMap::IndexedSelector::GetSelectorIndex()
    const {
  return selector_index_;
}

String InvalidationSetToSelectorMap::IndexedSelector::GetSelectorText() const {
  return style_rule_->SelectorAt(selector_index_).SelectorText();
}

const StyleSheetContents*
InvalidationSetToSelectorMap::IndexedSelector::GetStyleSheetContents() const {
  const StyleRuleToStyleSheetContentsMap* map =
      GetInstanceReference().Get()->style_rule_to_sheet_map_;
  return map->Lookup(style_rule_);
}

// static
void InvalidationSetToSelectorMap::StartOrStopTrackingIfNeeded(
    const TreeScope& tree_scope,
    const StyleEngine& style_engine) {
  Persistent<InvalidationSetToSelectorMap>& instance = GetInstanceReference();
  const bool is_tracing_enabled =
      InvalidationTracingFlag::IsEnabled() || SelectorStatisticsFlag::IsEnabled();
  if (is_tracing_enabled) [[unlikely]] {
    if (instance == nullptr) {
      instance = MakeGarbageCollected<InvalidationSetToSelectorMap>();
      instance->RevisitActiveStyleSheets(style_engine.ActiveUserStyleSheets(),
                                         style_engine);
      CSSDefaultStyleSheets::Instance().ForEachRuleFeatureSet(
          tree_scope.GetDocument(),
          /*call_for_each_stylesheet=*/true,
          BindRepeating(
              [](InvalidationSetToSelectorMap* instance,
                 const StyleEngine* style_engine,
                 const RuleFeatureSet& features, StyleSheetContents* contents) {
                if (contents) {
                  instance->RevisitStylesheetOnce(style_engine, contents,
                                                  &features);
                }
              },
              instance, WrapPersistent(&style_engine)));
    }
    const ScopedStyleResolver* scoped_style_resolver =
        tree_scope.GetScopedStyleResolver();
    if (scoped_style_resolver != nullptr) {
      instance->RevisitActiveStyleSheets(
          scoped_style_resolver->GetActiveStyleSheets(), style_engine);
    }
  } else if (!is_tracing_enabled && instance != nullptr) [[unlikely]] {
    instance.Clear();
  }
}

// static
bool InvalidationSetToSelectorMap::IsTracking() {
  return GetInstanceReference().Get() != nullptr;
}

void InvalidationSetToSelectorMap::RevisitActiveStyleSheets(
    const ActiveStyleSheetVector& active_style_sheets,
    const StyleEngine& style_engine) {
  for (const ActiveStyleSheet& sheet : active_style_sheets) {
    StyleSheetContents* contents = sheet.first->Contents();
    const RuleFeatureSet* features =
        contents->HasRuleSet() ? &contents->GetRuleSet().Features() : nullptr;
    RevisitStylesheetOnce(&style_engine, contents, features);
  }
}

void InvalidationSetToSelectorMap::RevisitStylesheetOnce(
    const StyleEngine* style_engine,
    StyleSheetContents* contents,
    const RuleFeatureSet* features) {
  if (!revisited_style_sheets_.Contains(contents)) {
    revisited_style_sheets_.insert(contents);
    style_engine->RevisitStyleSheetForInspector(contents, features);
  }
}

// static
void InvalidationSetToSelectorMap::BeginStyleSheetContents(
    const StyleSheetContents* contents) {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  CHECK(instance->current_style_sheet_contents_ == nullptr);
  instance->current_style_sheet_contents_ = contents;
}

// static
void InvalidationSetToSelectorMap::EndStyleSheetContents() {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  CHECK(instance->current_style_sheet_contents_ != nullptr);
  instance->current_style_sheet_contents_.Clear();
}

InvalidationSetToSelectorMap::StyleSheetContentsScope::StyleSheetContentsScope(
    const StyleSheetContents* contents) {
  InvalidationSetToSelectorMap::BeginStyleSheetContents(contents);
}
InvalidationSetToSelectorMap::StyleSheetContentsScope::
    ~StyleSheetContentsScope() {
  InvalidationSetToSelectorMap::EndStyleSheetContents();
}

// static
void InvalidationSetToSelectorMap::BeginSelector(StyleRule* style_rule,
                                                 unsigned selector_index) {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  CHECK(instance->current_selector_ == nullptr);
  instance->current_selector_ =
      MakeGarbageCollected<IndexedSelector>(style_rule, selector_index);

  // Some StyleRules, such as those created for speculation rules, do not exist
  // in a style sheet.
  if (instance->current_style_sheet_contents_ != nullptr) {
    instance->style_rule_to_sheet_map_->Add(
        style_rule, instance->current_style_sheet_contents_);
  }
}

// static
void InvalidationSetToSelectorMap::EndSelector() {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  CHECK(instance->current_selector_ != nullptr);
  instance->current_selector_.Clear();
}

InvalidationSetToSelectorMap::SelectorScope::SelectorScope(
    StyleRule* style_rule,
    unsigned selector_index) {
  InvalidationSetToSelectorMap::BeginSelector(style_rule, selector_index);
}
InvalidationSetToSelectorMap::SelectorScope::~SelectorScope() {
  InvalidationSetToSelectorMap::EndSelector();
}

// static
void InvalidationSetToSelectorMap::RecordInvalidationSetEntry(
    const InvalidationSet* invalidation_set,
    SelectorFeatureType type,
    const AtomicString& value) {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  // Ignore entries that get added during a combine operation. Those get
  // handled when the combine operation begins.
  if (instance->combine_recursion_depth_ > 0) {
    return;
  }

  CHECK(instance->current_selector_ != nullptr);
  InvalidationSetEntryMap* entry_map =
      instance->invalidation_set_map_
          ->insert(invalidation_set,
                   MakeGarbageCollected<InvalidationSetEntryMap>())
          .stored_value->value.Get();
  IndexedSelectorList* indexed_selector_list =
      entry_map
          ->insert(InvalidationSetEntry(type, value),
                   MakeGarbageCollected<IndexedSelectorList>())
          .stored_value->value.Get();
  indexed_selector_list->insert(instance->current_selector_);
}

// static
void InvalidationSetToSelectorMap::BeginInvalidationSetCombine(
    const InvalidationSet* target,
    const InvalidationSet* source) {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }
  instance->combine_recursion_depth_++;

  // `source` may not be in the map if it contains only information that is not
  // tracked such as self-invalidation.
  if (instance->invalidation_set_map_->Contains(source)) {
    InvalidationSetEntryMap* target_entry_map =
        instance->invalidation_set_map_
            ->insert(target, MakeGarbageCollected<InvalidationSetEntryMap>())
            .stored_value->value.Get();
    auto source_entry_it = instance->invalidation_set_map_->find(source);
    CHECK(source_entry_it != instance->invalidation_set_map_->end());
    for (const auto& source_selector_list_it : *(source_entry_it->value)) {
      IndexedSelectorList* target_selector_list =
          target_entry_map
              ->insert(source_selector_list_it.key,
                       MakeGarbageCollected<IndexedSelectorList>())
              .stored_value->value.Get();
      for (const auto& source_selector : *(source_selector_list_it.value)) {
        target_selector_list->insert(source_selector);
      }
    }
  }
}

// static
void InvalidationSetToSelectorMap::EndInvalidationSetCombine() {
  InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  CHECK_GT(instance->combine_recursion_depth_, 0u);
  instance->combine_recursion_depth_--;
}

InvalidationSetToSelectorMap::CombineScope::CombineScope(
    const InvalidationSet* target,
    const InvalidationSet* source) {
  InvalidationSetToSelectorMap::BeginInvalidationSetCombine(target, source);
}

InvalidationSetToSelectorMap::CombineScope::~CombineScope() {
  InvalidationSetToSelectorMap::EndInvalidationSetCombine();
}

// static
void InvalidationSetToSelectorMap::RemoveEntriesForInvalidationSet(
    const InvalidationSet* invalidation_set) {
  const InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return;
  }

  instance->invalidation_set_map_->erase(invalidation_set);
}

// static
const InvalidationSetToSelectorMap::IndexedSelectorList*
InvalidationSetToSelectorMap::Lookup(const InvalidationSet* invalidation_set,
                                     SelectorFeatureType type,
                                     const AtomicString& value) {
  const InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance == nullptr) {
    return nullptr;
  }

  auto entry_it = instance->invalidation_set_map_->find(invalidation_set);
  if (entry_it != instance->invalidation_set_map_->end()) {
    auto selector_list_it =
        entry_it->value->find(InvalidationSetEntry(type, value));
    if (selector_list_it != entry_it->value->end()) {
      return selector_list_it->value;
    }
  }

  return nullptr;
}

// static
const StyleSheetContents*
InvalidationSetToSelectorMap::LookupStyleSheetContentsForRule(
    const StyleRule* style_rule) {
  const InvalidationSetToSelectorMap* instance = GetInstanceReference().Get();
  if (instance != nullptr) {
    const StyleRuleToStyleSheetContentsMap* map =
        instance->style_rule_to_sheet_map_;
    return map->Lookup(style_rule);
  }
  return nullptr;
}

InvalidationSetToSelectorMap::InvalidationSetToSelectorMap() {
  invalidation_set_map_ = MakeGarbageCollected<InvalidationSetMap>();
  style_rule_to_sheet_map_ =
      MakeGarbageCollected<StyleRuleToStyleSheetContentsMap>();
}

void InvalidationSetToSelectorMap::Trace(Visitor* visitor) const {
  visitor->Trace(invalidation_set_map_);
  visitor->Trace(revisited_style_sheets_);
  visitor->Trace(current_style_sheet_contents_);
  visitor->Trace(current_selector_);
  visitor->Trace(style_rule_to_sheet_map_);
}

// static
Persistent<InvalidationSetToSelectorMap>&
InvalidationSetToSelectorMap::GetInstanceReference() {
  DEFINE_STATIC_LOCAL(Persistent<InvalidationSetToSelectorMap>, instance, ());
  return instance;
}

}  // namespace blink
