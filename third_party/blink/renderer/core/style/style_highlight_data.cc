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
    // TODO(crbug.com/1024156): init field for ::highlight()
    : selection_(other.selection_),
      target_text_(other.target_text_),
      spelling_error_(other.spelling_error_),
      grammar_error_(other.grammar_error_) {}

bool StyleHighlightData::operator==(const StyleHighlightData& other) const {
  // TODO(crbug.com/1024156): compare field for ::highlight()
  return DataEquivalent(selection_, other.selection_) &&
         DataEquivalent(target_text_, other.target_text_) &&
         DataEquivalent(spelling_error_, other.spelling_error_) &&
         DataEquivalent(grammar_error_, other.grammar_error_);
}

const scoped_refptr<const ComputedStyle>& StyleHighlightData::Selection()
    const {
  return selection_;
}

const scoped_refptr<const ComputedStyle>& StyleHighlightData::TargetText()
    const {
  return target_text_;
}

const scoped_refptr<const ComputedStyle>& StyleHighlightData::SpellingError()
    const {
  return spelling_error_;
}

const scoped_refptr<const ComputedStyle>& StyleHighlightData::GrammarError()
    const {
  return grammar_error_;
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

}  // namespace blink
