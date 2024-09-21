// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_highlight_data.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// Compares two CustomHighlightsStyleMaps with base::ValuesEquivalent as
// comparison function on the values.
bool HighlightStyleMapEquals(const CustomHighlightsStyleMap& a,
                             const CustomHighlightsStyleMap& b) {
  if (a.size() != b.size()) {
    return false;
  }

  CustomHighlightsStyleMap::const_iterator a_end = a.end();
  CustomHighlightsStyleMap::const_iterator b_end = b.end();
  for (CustomHighlightsStyleMap::const_iterator it = a.begin(); it != a_end;
       ++it) {
    CustomHighlightsStyleMap::const_iterator b_pos = b.find(it->key);
    if (b_pos == b_end || !base::ValuesEquivalent(it->value, b_pos->value)) {
      return false;
    }
  }

  return true;
}

bool StyleHighlightData::operator==(const StyleHighlightData& other) const {
  return base::ValuesEquivalent(selection_, other.selection_) &&
         base::ValuesEquivalent(target_text_, other.target_text_) &&
         base::ValuesEquivalent(search_text_current_,
                                other.search_text_current_) &&
         base::ValuesEquivalent(search_text_not_current_,
                                other.search_text_not_current_) &&
         base::ValuesEquivalent(spelling_error_, other.spelling_error_) &&
         base::ValuesEquivalent(grammar_error_, other.grammar_error_) &&
         HighlightStyleMapEquals(custom_highlights_, other.custom_highlights_);
}

const ComputedStyle* StyleHighlightData::Style(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  DCHECK(IsHighlightPseudoElement(pseudo_id));
  switch (pseudo_id) {
    case kPseudoIdSelection:
      return Selection();
    case kPseudoIdSearchText:
      // For ::search-text:current, call SearchTextCurrent() directly.
      return SearchTextNotCurrent();
    case kPseudoIdTargetText:
      return TargetText();
    case kPseudoIdSpellingError:
      return SpellingError();
    case kPseudoIdGrammarError:
      return GrammarError();
    case kPseudoIdHighlight:
      return CustomHighlight(pseudo_argument);
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

const ComputedStyle* StyleHighlightData::Selection() const {
  return selection_.Get();
}

const ComputedStyle* StyleHighlightData::SearchTextCurrent() const {
  return search_text_current_.Get();
}

const ComputedStyle* StyleHighlightData::SearchTextNotCurrent() const {
  return search_text_not_current_.Get();
}

const ComputedStyle* StyleHighlightData::TargetText() const {
  return target_text_.Get();
}

const ComputedStyle* StyleHighlightData::SpellingError() const {
  return spelling_error_.Get();
}

const ComputedStyle* StyleHighlightData::GrammarError() const {
  return grammar_error_.Get();
}

const ComputedStyle* StyleHighlightData::CustomHighlight(
    const AtomicString& highlight_name) const {
  if (highlight_name) {
    auto iter = custom_highlights_.find(highlight_name);
    if (iter != custom_highlights_.end()) {
      CHECK(iter->value);
      return iter->value.Get();
    }
  }
  return nullptr;
}

void StyleHighlightData::SetSelection(const ComputedStyle* style) {
  selection_ = style;
}

void StyleHighlightData::SetSearchTextCurrent(const ComputedStyle* style) {
  search_text_current_ = style;
}

void StyleHighlightData::SetSearchTextNotCurrent(const ComputedStyle* style) {
  search_text_not_current_ = style;
}

void StyleHighlightData::SetTargetText(const ComputedStyle* style) {
  target_text_ = style;
}

void StyleHighlightData::SetSpellingError(const ComputedStyle* style) {
  spelling_error_ = style;
}

void StyleHighlightData::SetGrammarError(const ComputedStyle* style) {
  grammar_error_ = style;
}

void StyleHighlightData::SetCustomHighlight(const AtomicString& highlight_name,
                                            const ComputedStyle* style) {
  DCHECK(highlight_name);
  if (style) {
    custom_highlights_.Set(highlight_name, style);
  } else {
    custom_highlights_.erase(highlight_name);
  }
}

bool StyleHighlightData::DependsOnSizeContainerQueries() const {
  if ((selection_ && (selection_->DependsOnSizeContainerQueries() ||
                      selection_->HasContainerRelativeUnits())) ||
      (target_text_ && (target_text_->DependsOnSizeContainerQueries() ||
                        target_text_->HasContainerRelativeUnits())) ||
      (spelling_error_ && (spelling_error_->DependsOnSizeContainerQueries() ||
                           spelling_error_->HasContainerRelativeUnits())) ||
      (grammar_error_ && (grammar_error_->DependsOnSizeContainerQueries() ||
                          grammar_error_->HasContainerRelativeUnits()))) {
    return true;
  }
  for (auto style : custom_highlights_) {
    if (style.value->DependsOnSizeContainerQueries() ||
        style.value->HasContainerRelativeUnits()) {
      return true;
    }
  }
  return false;
}

void StyleHighlightData::Trace(Visitor* visitor) const {
  visitor->Trace(selection_);
  visitor->Trace(search_text_current_);
  visitor->Trace(search_text_not_current_);
  visitor->Trace(target_text_);
  visitor->Trace(spelling_error_);
  visitor->Trace(grammar_error_);
  visitor->Trace(custom_highlights_);
}

}  // namespace blink
