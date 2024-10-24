// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_ghost_rules.h"

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
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

InspectorGhostRules::~InspectorGhostRules() {
  for (const Member<CSSStyleSheet>& style_sheet : affected_stylesheets_) {
    DepopulateSheet(*style_sheet);
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
      rule.insertRule(&execution_context, "--dummy:1", i, ASSERT_NO_EXCEPTION);
      auto* inserted_rule = To<CSSNestedDeclarationsRule>(rule.ItemInternal(i));
      inserted_rule->style()->removeProperty("--dummy", ASSERT_NO_EXCEPTION);
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
        rule.deleteRule(i, ASSERT_NO_EXCEPTION);
      }
    }
  });
}

}  // namespace blink
