// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ghost_rules.h"

#include <algorithm>

#include "base/debug/dump_without_crashing.h"
#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"

namespace blink {

namespace {

template <typename Func>
void ForEachRule(CSSRule& rule, Func func) {
  if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
    func(*style_rule);
    for (wtf_size_t i = 0; i < style_rule->length(); ++i) {
      ForEachRule(*style_rule->ItemInternal(i), func);
    }
  } else if (auto* grouping_rule = DynamicTo<CSSGroupingRule>(rule)) {
    func(*grouping_rule);
    for (wtf_size_t i = 0; i < grouping_rule->length(); ++i) {
      ForEachRule(*grouping_rule->ItemInternal(i), func);
    }
  }
}

template <typename Func>
void ForEachRule(CSSStyleSheet& sheet, Func func) {
  for (wtf_size_t i = 0; i < sheet.length(); ++i) {
    ForEachRule(*sheet.ItemInternal(i), func);
  }
}

}  // namespace

void InspectorGhostRules::Populate(CSSStyleSheet& sheet) {
  Document* document = sheet.OwnerDocument();
  if (!document) {
    return;
  }
  wtf_size_t size_before = inserted_rules_.size();
  PopulateSheet(*document->GetExecutionContext(), sheet);
  wtf_size_t size_after = inserted_rules_.size();
  if (size_before != size_after) {
    affected_stylesheets_.insert(&sheet);
  }
}

bool InspectorGhostRules::PopulateSheets(
    HeapVector<Member<CSSStyleSheet>> sheets) {
  // Collect all StyleSheetContents that claim to not be shared between
  // multiple CSSStyleSheets.
  HeapHashCountedSet<Member<StyleSheetContents>> unshared_contents;
  for (const Member<CSSStyleSheet>& sheet : sheets) {
    if (!sheet->IsContentsShared()) {
      unshared_contents.insert(sheet->Contents());
    }
  }

  wtf_size_t size_before = sheets.size();

  // Remove all CSSStyleSheets that share a StyleSheetContents instance
  // with another CSSStyleSheet without being aware of it.
  auto new_end =
      std::remove_if(sheets.begin(), sheets.end(),
                     [&unshared_contents](const Member<CSSStyleSheet>& sheet) {
                       auto it = unshared_contents.find(sheet->Contents());
                       return it != unshared_contents.end() && it->value > 1;
                     });
  sheets.erase(new_end, sheets.end());

  wtf_size_t size_after = sheets.size();

  for (const Member<CSSStyleSheet>& sheet : sheets) {
    Populate(*sheet);
  }

  return size_before == size_after;
}

void InspectorGhostRules::PopulateSheetsWithAssertion(
    HeapVector<Member<CSSStyleSheet>> sheets) {
  bool success = PopulateSheets(std::move(sheets));
  if (!success) {
    base::debug::DumpWithoutCrashing();
    DCHECK(false) << "Invalid sharing of StyleSheetContents";
  }
}

void InspectorGhostRules::Activate(Document& document) {
  ActivateTreeScope(document);
  for (const Member<TreeScope>& tree_scope :
       document.GetStyleEngine().GetActiveTreeScopes()) {
    ActivateTreeScope(*tree_scope);
  }
  // TODO(crbug.com/363985597): Handle User stylesheets.
}

void InspectorGhostRules::ActivateTreeScope(TreeScope& tree_scope) {
  ScopedStyleResolver* resolver = tree_scope.GetScopedStyleResolver();
  if (!resolver) {
    return;
  }
  StyleEngine& style_engine = tree_scope.GetDocument().GetStyleEngine();
  bool any_affected = false;
  // Build an alternative ActiveStyleSheetVector for the TreeScope,
  // where the RuleSet of each (affected) entry is replaced with a freshly
  // created "unconnected" RuleSet. That unconnected RuleSet will contain
  // any populated ghost rules.
  ActiveStyleSheetVector alternative_stylesheets;
  alternative_stylesheets.ReserveInitialCapacity(
      resolver->GetActiveStyleSheets().size());
  for (const ActiveStyleSheet& active_stylesheet :
       resolver->GetActiveStyleSheets()) {
    CSSStyleSheet* sheet = active_stylesheet.first.Get();
    if (affected_stylesheets_.Contains(sheet)) {
      // TODO(sesse): Collect mixins from here.
      alternative_stylesheets.push_back(ActiveStyleSheet{
          sheet, style_engine.CreateUnconnectedRuleSet(*sheet, /*mixins=*/{})});
      any_affected = true;
    } else {
      alternative_stylesheets.push_back(active_stylesheet);
    }
  }
  // If at least one stylesheet was affected, we swap the active stylesheets.
  // Otherwise, the vector of alternative sheets is just discarded.
  if (any_affected) {
    resolver->QuietlySwapActiveStyleSheets(alternative_stylesheets);
    // Note that `alternative_stylesheets` contains the original vector at
    // this point. We keep track of it so we can restore it in the destructor.
    affected_tree_scopes.insert(&tree_scope,
                                std::move(alternative_stylesheets));
  }
}

InspectorGhostRules::~InspectorGhostRules() {
  for (const Member<CSSStyleSheet>& style_sheet : affected_stylesheets_) {
    DepopulateSheet(*style_sheet);
  }
  // Restore original active stylesheets.
  for (auto& [tree_scope, active_stylesheet_vector] : affected_tree_scopes) {
    tree_scope->GetScopedStyleResolver()->QuietlySwapActiveStyleSheets(
        active_stylesheet_vector);
  }
}

namespace {

// CSSStyleRule should inherit from CSSGroupingRule [1], but unfortunately
// we have not been able to make this change yet, so we need to dispatch
// some function calls manually to CSSStyleRule/CSSGroupingRule.
//
// https://github.com/w3c/csswg-drafts/issues/8940

void QuietlyInsertDummyRule(const ExecutionContext& execution_context,
                            CSSRule& rule,
                            wtf_size_t index) {
  if (IsA<CSSStyleRule>(rule)) {
    To<CSSStyleRule>(rule).QuietlyInsertRule(&execution_context, "--dummy:1",
                                             index);
  } else {
    To<CSSGroupingRule>(rule).QuietlyInsertRule(&execution_context, "--dummy:1",
                                                index);
  }
}

void QuietlyDeleteRule(CSSRule& rule, wtf_size_t index) {
  if (IsA<CSSStyleRule>(rule)) {
    To<CSSStyleRule>(rule).QuietlyDeleteRule(index);
  } else {
    To<CSSGroupingRule>(rule).QuietlyDeleteRule(index);
  }
}

wtf_size_t NumItems(CSSRule& rule) {
  if (IsA<CSSStyleRule>(rule)) {
    return To<CSSStyleRule>(rule).length();
  }
  return To<CSSGroupingRule>(rule).length();
}

CSSRule* ItemAt(CSSRule& rule, wtf_size_t index) {
  if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
    return style_rule->ItemInternal(index);
  }
  return To<CSSGroupingRule>(rule).ItemInternal(index);
}

bool HasNestedDeclarationsAtIndex(CSSRule& rule, wtf_size_t index) {
  if (index == kNotFound || index >= NumItems(rule)) {
    return false;
  }
  return ItemAt(rule, index)->GetType() == CSSRule::kNestedDeclarationsRule;
}

}  // namespace

void InspectorGhostRules::PopulateSheet(
    const ExecutionContext& execution_context,
    CSSStyleSheet& sheet) {
  ForEachRule(sheet, [&](CSSRule& rule) {
    CHECK(IsA<CSSStyleRule>(rule) || IsA<CSSGroupingRule>(rule));

    // Only "nested group rules" should be affected.
    // https://drafts.csswg.org/css-nesting-1/#nested-group-rules
    if (IsA<CSSGroupingRule>(rule)) {
      if (!IsA<CSSStyleRule>(rule.parentRule())) {
        return;
      }
    } else {
      // For investigating crbug.com/389011795.
      auto& style_rule = To<CSSStyleRule>(rule);
      if (style_rule.length() != style_rule.WrapperCountForDebugging()) {
        base::debug::DumpWithoutCrashing();
        DCHECK(false) << "Mismatched wrapper count";
        return;
      }
    }

    // The end_index is '0' for style rules to account for the built-in
    // leading declaration block.
    wtf_size_t end_index = IsA<CSSStyleRule>(rule) ? 0 : kNotFound;

    // Insert a ghost rule between any two adjacent non-CSSNestedDeclaration
    // rules, using reverse order to keep indices stable.
    static_assert((static_cast<wtf_size_t>(0) - 1) == kNotFound);
    for (wtf_size_t i = NumItems(rule); i != end_index; --i) {
      if (HasNestedDeclarationsAtIndex(rule, i) ||
          HasNestedDeclarationsAtIndex(rule, i - 1)) {
        // Don't insert a ghost rule (i.e. a CSSNestedDeclarations rule) next to
        // an existing CSSNestedDeclarations rule.
        continue;
      }

      quiet_mutation_scope_.Add(sheet);

      // It's not valid to insert an empty nested decl. rule, so we temporarily
      // insert --dummy, then remove it immediately.
      QuietlyInsertDummyRule(execution_context, rule, i);
      auto* inserted_rule = To<CSSNestedDeclarationsRule>(ItemAt(rule, i));
      inserted_rule->style()->QuietlyRemoveProperty("--dummy");
      inserted_rules_.insert(inserted_rule);
      inner_rules_.insert(To<CSSStyleRule>(inserted_rule->InnerCSSStyleRule()));
    }

    // For investigating crbug.com/389011795.
    if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
      CHECK_EQ(style_rule->length(), style_rule->WrapperCountForDebugging());
    }
  });
}

void InspectorGhostRules::DepopulateSheet(CSSStyleSheet& sheet) {
  ForEachRule(sheet, [&](CSSRule& rule) {
    CHECK(IsA<CSSStyleRule>(rule) || IsA<CSSGroupingRule>(rule));

    static_assert((static_cast<wtf_size_t>(0) - 1) == kNotFound);
    for (wtf_size_t i = NumItems(rule) - 1; i != kNotFound; --i) {
      auto* nested_declarations_rule =
          DynamicTo<CSSNestedDeclarationsRule>(ItemAt(rule, i));
      if (nested_declarations_rule &&
          inserted_rules_.Contains(nested_declarations_rule)) {
        QuietlyDeleteRule(rule, i);
      }
    }

    // For investigating crbug.com/389011795.
    if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
      CHECK_EQ(style_rule->length(), style_rule->WrapperCountForDebugging());
    }
  });
}

}  // namespace blink
