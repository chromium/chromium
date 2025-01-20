// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ghost_rules.h"

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"

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
      alternative_stylesheets.push_back(ActiveStyleSheet{
          sheet, style_engine.CreateUnconnectedRuleSet(*sheet)});
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
                                MakeGarbageCollected<ActiveStyleSheetVector>(
                                    std::move(alternative_stylesheets)));
  }
}

InspectorGhostRules::~InspectorGhostRules() {
  for (const Member<CSSStyleSheet>& style_sheet : affected_stylesheets_) {
    DepopulateSheet(*style_sheet);
  }
  // Restore original active stylesheets.
  for (auto [tree_scope, active_stylesheet_vector] : affected_tree_scopes) {
    tree_scope->GetScopedStyleResolver()->QuietlySwapActiveStyleSheets(
        *active_stylesheet_vector);
  }
}

namespace {

template <typename T>
bool HasNestedDeclarationsAtIndex(T& rule, wtf_size_t index) {
  if (index == kNotFound || index >= rule.length()) {
    return false;
  }
  return rule.ItemInternal(index)->GetType() ==
         CSSRule::kNestedDeclarationsRule;
}

}  // namespace

void InspectorGhostRules::PopulateSheet(
    const ExecutionContext& execution_context,
    CSSStyleSheet& sheet) {
  ForEachRule(sheet, [&](auto& rule) {
    // This is just to document that the incoming 'auto' is either
    // CSSStyleRule or CSSGroupingRule.
    using Type = std::remove_reference<decltype(rule)>::type;
    static_assert(std::is_same_v<Type, CSSStyleRule> ||
                  std::is_same_v<Type, CSSGroupingRule>);

    // Only "nested group rules" should be affected.
    // https://drafts.csswg.org/css-nesting-1/#nested-group-rules
    if constexpr (std::is_same_v<Type, CSSGroupingRule>) {
      if (!IsA<CSSStyleRule>(rule.parentRule())) {
        return;
      }
    }

    // The end_index is '0' for style rules to account for the built-in
    // leading declaration block.
    wtf_size_t end_index = std::is_same_v<Type, CSSStyleRule> ? 0 : kNotFound;

    // Insert a ghost rule between any two adjacent non-CSSNestedDeclaration
    // rules, using reverse order to keep indices stable.
    static_assert((static_cast<wtf_size_t>(0) - 1) == kNotFound);
    for (wtf_size_t i = rule.length(); i != end_index; --i) {
      if (HasNestedDeclarationsAtIndex(rule, i) ||
          HasNestedDeclarationsAtIndex(rule, i - 1)) {
        // Don't insert a ghost rule (i.e. a CSSNestedDeclarations rule) next to
        // an existing CSSNestedDeclarations rule.
        continue;
      }

      // It's not valid to insert an empty nested decl. rule, so we temporarily
      // insert --dummy, then remove it immediately.
      rule.QuietlyInsertRule(&execution_context, "--dummy:1", i);
      auto* inserted_rule = To<CSSNestedDeclarationsRule>(rule.ItemInternal(i));
      inserted_rule->style()->QuietlyRemoveProperty("--dummy");
      inserted_rules_.insert(inserted_rule);
      inner_rules_.insert(To<CSSStyleRule>(inserted_rule->InnerCSSStyleRule()));
    }
  });
}

void InspectorGhostRules::DepopulateSheet(CSSStyleSheet& sheet) {
  ForEachRule(sheet, [&](auto& rule) {
    using Type = std::remove_reference<decltype(rule)>::type;
    static_assert(std::is_same_v<Type, CSSStyleRule> ||
                  std::is_same_v<Type, CSSGroupingRule>);

    static_assert((static_cast<wtf_size_t>(0) - 1) == kNotFound);
    for (wtf_size_t i = rule.length() - 1; i != kNotFound; --i) {
      auto* nested_declarations_rule =
          DynamicTo<CSSNestedDeclarationsRule>(rule.ItemInternal(i));
      if (nested_declarations_rule &&
          inserted_rules_.Contains(nested_declarations_rule)) {
        rule.QuietlyDeleteRule(i);
      }
    }
  });
}

}  // namespace blink
