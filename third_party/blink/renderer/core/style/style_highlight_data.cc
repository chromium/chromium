// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_highlight_data.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

scoped_refptr<StyleHighlightData> StyleHighlightData::Create() {
  return base::AdoptRef(new StyleHighlightData);
}
scoped_refptr<StyleHighlightData> StyleHighlightData::Copy() const {
  return base::AdoptRef(new StyleHighlightData(*this));
}

StyleHighlightData::StyleHighlightData() = default;

StyleHighlightData::StyleHighlightData(const StyleHighlightData& other)
    : selection_(other.selection_),
      target_text_(other.target_text_),
      spelling_error_(other.spelling_error_),
      grammar_error_(other.grammar_error_),
      custom_highlights_(other.custom_highlights_) {}

// Compares two CustomHighlightsStyleMaps with DataEquivalent as comparison
// function on the values.
bool HighlightStyleMapEquals(const CustomHighlightsStyleMap& a,
                             const CustomHighlightsStyleMap& b) {
  if (a.size() != b.size())
    return false;

  CustomHighlightsStyleMap::const_iterator a_end = a.end();
  CustomHighlightsStyleMap::const_iterator b_end = b.end();
  for (CustomHighlightsStyleMap::const_iterator it = a.begin(); it != a_end;
       ++it) {
    CustomHighlightsStyleMap::const_iterator b_pos = b.find(it->key);
    if (b_pos == b_end || !DataEquivalent(it->value, b_pos->value))
      return false;
  }

  return true;
}

bool StyleHighlightData::operator==(const StyleHighlightData& other) const {
  return DataEquivalent(selection_, other.selection_) &&
         DataEquivalent(target_text_, other.target_text_) &&
         DataEquivalent(spelling_error_, other.spelling_error_) &&
         DataEquivalent(grammar_error_, other.grammar_error_) &&
         HighlightStyleMapEquals(custom_highlights_, other.custom_highlights_);
}

const ComputedStyle* StyleHighlightData::Selection() const {
  return selection_.get();
}

const ComputedStyle* StyleHighlightData::TargetText() const {
  return target_text_.get();
}

const ComputedStyle* StyleHighlightData::SpellingError() const {
  return spelling_error_.get();
}

const ComputedStyle* StyleHighlightData::GrammarError() const {
  return grammar_error_.get();
}

const ComputedStyle* StyleHighlightData::CustomHighlight(
    const AtomicString& highlight_name) const {
  if (highlight_name) {
    auto iter = custom_highlights_.find(highlight_name);
    if (iter != custom_highlights_.end())
      return iter->value.get();
  }
  return nullptr;
}

void StyleHighlightData::SetSelection(scoped_refptr<ComputedStyle>&& style) {
  selection_ = std::move(style);
}

void StyleHighlightData::SetTargetText(scoped_refptr<ComputedStyle>&& style) {
  target_text_ = std::move(style);
}

void StyleHighlightData::SetSpellingError(
    scoped_refptr<ComputedStyle>&& style) {
  spelling_error_ = std::move(style);
}

void StyleHighlightData::SetGrammarError(scoped_refptr<ComputedStyle>&& style) {
  grammar_error_ = std::move(style);
}

void StyleHighlightData::SetCustomHighlight(
    const AtomicString& highlight_name,
    scoped_refptr<ComputedStyle>&& style) {
  DCHECK(highlight_name);
  custom_highlights_.Set(highlight_name, std::move(style));
}

}  // namespace blink
