// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_scope.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

StyleScope::StyleScope(CSSSelectorList* from, CSSSelectorList* to)
    : from_(from), to_(to) {}

StyleScope::StyleScope(StyleSheetContents* contents) : contents_(contents) {}

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
    return from_->First();
  }
  return nullptr;
}

const CSSSelector* StyleScope::To() const {
  if (to_) {
    return to_->First();
  }
  return nullptr;
}

bool StyleScope::HasImplicitRoot(Element* element) const {
  if (!contents_) {
    return false;
  }
  return contents_->HasOwnerParentNode(element);
}

unsigned StyleScope::Specificity() const {
  if (!specificity_.has_value()) {
    specificity_ = (from_ ? from_->MaximumSpecificity() : 0) +
                   (parent_ ? parent_->Specificity() : 0);
  }
  return *specificity_;
}

StyleScope* StyleScope::Parse(CSSParserTokenRange prelude,
                              const CSSParserContext* context,
                              StyleSheetContents* style_sheet) {
  HeapVector<CSSSelector> arena;

  absl::optional<base::span<CSSSelector>> from;
  absl::optional<base::span<CSSSelector>> to;

  prelude.ConsumeWhitespace();

  if (prelude.AtEnd()) {
    // Implicitly rooted.
    return MakeGarbageCollected<StyleScope>(style_sheet);
  }

  if (prelude.Peek().GetType() != kLeftParenthesisToken) {
    return nullptr;
  }

  // <scope-start>
  {
    auto block = prelude.ConsumeBlock();
    from = CSSSelectorParser::ParseScopeBoundary(block, context, style_sheet,
                                                 arena);
    if (!from.has_value()) {
      return nullptr;
    }
  }

  prelude.ConsumeWhitespace();

  // to (<scope-end>)
  if (css_parsing_utils::ConsumeIfIdent(prelude, "to")) {
    if (prelude.Peek().GetType() != kLeftParenthesisToken) {
      return nullptr;
    }

    auto block = prelude.ConsumeBlock();
    to = CSSSelectorParser::ParseScopeBoundary(block, context, style_sheet,
                                               arena);
    if (!to.has_value()) {
      return nullptr;
    }
  }

  prelude.ConsumeWhitespace();

  if (!prelude.AtEnd()) {
    return nullptr;
  }

  CSSSelectorList* from_list =
      from.has_value() ? CSSSelectorList::AdoptSelectorVector(from.value())
                       : nullptr;
  CSSSelectorList* to_list =
      to.has_value() ? CSSSelectorList::AdoptSelectorVector(to.value())
                     : nullptr;

  return MakeGarbageCollected<StyleScope>(from_list, to_list);
}

void StyleScope::Trace(blink::Visitor* visitor) const {
  visitor->Trace(contents_);
  visitor->Trace(from_);
  visitor->Trace(to_);
  visitor->Trace(parent_);
}

}  // namespace blink
