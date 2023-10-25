// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

StyleScope::StyleScope(StyleRule* from, CSSSelectorList* to)
    : from_(from), to_(to) {}

StyleScope::StyleScope(StyleSheetContents* contents, CSSSelectorList* to)
    : contents_(contents), to_(to) {}

StyleScope::StyleScope(const StyleScope& other)
    : contents_(other.contents_),
      from_(other.from_ ? other.from_->Copy() : nullptr),
      to_(other.to_ ? other.to_->Copy() : nullptr),
      parent_(other.parent_) {}

StyleScope* StyleScope::CopyWithParent(const StyleScope* parent) const {
  StyleScope* copy = MakeGarbageCollected<StyleScope>(*this);
  copy->parent_ = parent;
  return copy;
}

const CSSSelector* StyleScope::From() const {
  if (from_) {
    return from_->FirstSelector();
  }
  return nullptr;
}

const CSSSelector* StyleScope::To() const {
  if (to_) {
    return to_->First();
  }
  return nullptr;
}

StyleScope* StyleScope::Parse(CSSParserTokenRange prelude,
                              const CSSParserContext* context,
                              CSSNestingType nesting_type,
                              StyleRule* parent_rule_for_nesting,
                              StyleSheetContents* style_sheet) {
  HeapVector<CSSSelector> arena;

  absl::optional<base::span<CSSSelector>> from;
  absl::optional<base::span<CSSSelector>> to;

  prelude.ConsumeWhitespace();

  // <scope-start>
  if (prelude.Peek().GetType() == kLeftParenthesisToken) {
    auto block = prelude.ConsumeBlock();
    from = CSSSelectorParser::ParseScopeBoundary(block, context, nesting_type,
                                                 parent_rule_for_nesting,
                                                 style_sheet, arena);
    if (!from.has_value()) {
      return nullptr;
    }
  }

  StyleRule* from_rule = nullptr;
  if (from.has_value() && !from.value().empty()) {
    auto* properties = MakeGarbageCollected<ImmutableCSSPropertyValueSet>(
        /* properties */ nullptr, /* count */ 0,
        CSSParserMode::kHTMLStandardMode);
    from_rule = StyleRule::Create(from.value(), properties);
  }

  prelude.ConsumeWhitespace();

  // to (<scope-end>)
  if (css_parsing_utils::ConsumeIfIdent(prelude, "to")) {
    if (prelude.Peek().GetType() != kLeftParenthesisToken) {
      return nullptr;
    }

    // Note that <scope-start> should act as the enclosing style rule for
    // the purposes of matching the parent pseudo-class (&) within <scope-end>,
    // hence we're not passing `nesting_type` and `parent_rule_for_nesting`
    // to `ParseScopeBoundary` here.
    //
    // https://drafts.csswg.org/css-nesting-1/#nesting-at-scope
    auto block = prelude.ConsumeBlock();
    to = CSSSelectorParser::ParseScopeBoundary(
        block, context, CSSNestingType::kScope,
        /* parent_rule_for_nesting */ from_rule, style_sheet, arena);
    if (!to.has_value()) {
      return nullptr;
    }
  }

  prelude.ConsumeWhitespace();

  if (!prelude.AtEnd()) {
    return nullptr;
  }

  CSSSelectorList* to_list =
      to.has_value() ? CSSSelectorList::AdoptSelectorVector(to.value())
                     : nullptr;

  if (!from.has_value()) {
    // Implicitly rooted.
    return MakeGarbageCollected<StyleScope>(style_sheet, to_list);
  }

  return MakeGarbageCollected<StyleScope>(from_rule, to_list);
}

void StyleScope::Trace(blink::Visitor* visitor) const {
  visitor->Trace(contents_);
  visitor->Trace(from_);
  visitor->Trace(to_);
  visitor->Trace(parent_);
}

}  // namespace blink
