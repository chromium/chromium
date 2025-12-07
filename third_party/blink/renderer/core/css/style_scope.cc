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

StyleScope::StyleScope(const StyleScope& other)
    : from_(other.from_ ? To<StyleRule>(other.from_->Clone(
                              /*new_parent=*/nullptr,
                              /*mixin_parameter_bindings=*/nullptr))
                        : nullptr),
      to_(other.to_ ? other.to_->Renest(/*new_parent=*/nullptr) : nullptr),
      parent_(other.parent_) {}

StyleScope* StyleScope::CopyWithParent(const StyleScope* parent) const {
  StyleScope* copy = MakeGarbageCollected<StyleScope>(*this);
  copy->parent_ = parent;
  return copy;
}

const StyleScope* StyleScope::Clone(StyleRule* new_parent) const {
  StyleRule* reparented_from =
      from_ ? blink::To<StyleRule>(from_->Clone(
                  new_parent, /*mixin_parameter_bindings=*/nullptr))
            : nullptr;
  // Note that for the "to" selector, any '&' selectors must point
  // to the "from" selector.
  CSSSelectorList* reparented_to = to_ ? to_->Renest(reparented_from) : nullptr;
  // The `parent_` member should only be populated via calls to CopyWithParent
  // (RuleSet-time), and this StyleScope should not be one such copy.
  CHECK(!parent_);
  return MakeGarbageCollected<StyleScope>(reparented_from, reparented_to);
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

StyleScope* StyleScope::Parse(CSSParserTokenStream& stream,
                              const CSSParserContext* context,
                              CSSNestingType nesting_type,
                              StyleRule* parent_rule_for_nesting,
                              StyleSheetContents* style_sheet) {
  HeapVector<CSSSelector> arena;

  base::span<CSSSelector> from;
  base::span<CSSSelector> to;

  stream.ConsumeWhitespace();

  // <scope-start>
  if (stream.Peek().GetType() == kLeftParenthesisToken) {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    from = CSSSelectorParser::ParseScopeBoundary(stream, context, nesting_type,
                                                 parent_rule_for_nesting,
                                                 style_sheet, arena);
    if (from.empty()) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();

  StyleRule* from_rule = nullptr;
  if (!from.empty()) {
    auto* properties = ImmutableCSSPropertyValueSet::Create(
        base::span<CSSPropertyValue>(), CSSParserMode::kHTMLStandardMode);
    from_rule = StyleRule::Create(from, properties);
  }

  // to (<scope-end>)
  if (css_parsing_utils::ConsumeIfIdent(stream, "to")) {
    if (stream.Peek().GetType() != kLeftParenthesisToken) {
      return nullptr;
    }

    // Note that <scope-start> should act as the enclosing style rule for
    // the purposes of matching the parent pseudo-class (&) within <scope-end>,
    // hence we're not passing `nesting_type` or `parent_rule_for_nesting`
    // to `ParseScopeBoundary` here.
    //
    // https://drafts.csswg.org/css-nesting-1/#nesting-at-scope
    //
    // Note: We are in the process of changing this behavior. The '&' pseudo-
    // class should now behave like :where(:scope), which is what we
    // automatically get if we pass nullptr as the parent rule.
    // See crbug.com/445949406.
    StyleRule* parent_rule_for_to_selector =
        RuntimeEnabledFeatures::CSSScopeifiedParentPseudoClassEnabled()
            ? nullptr
            : from_rule;
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    to = CSSSelectorParser::ParseScopeBoundary(
        stream, context, CSSNestingType::kScope,
        /*parent_rule_for_nesting=*/parent_rule_for_to_selector, style_sheet,
        arena);
    if (to.empty()) {
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();

  CSSSelectorList* to_list =
      !to.empty() ? CSSSelectorList::AdoptSelectorVector(to) : nullptr;

  if (from.empty()) {
    // Implicitly rooted.
    return MakeGarbageCollected<StyleScope>(/*from=*/nullptr, to_list);
  }

  return MakeGarbageCollected<StyleScope>(from_rule, to_list);
}

StyleRule* StyleScope::RuleForNesting() const {
  if (RuntimeEnabledFeatures::CSSScopeifiedParentPseudoClassEnabled()) {
    return nullptr;
  }
  return from_.Get();
}

void StyleScope::Trace(blink::Visitor* visitor) const {
  visitor->Trace(from_);
  visitor->Trace(to_);
  visitor->Trace(parent_);
}

}  // namespace blink
