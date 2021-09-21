// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_highlight_data.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

StyleHighlightData::StyleHighlightData(PkComputedStyle)
    : StyleHighlightData() {}
StyleHighlightData::StyleHighlightData(PkComputedStyle,
                                       const StyleHighlightData& other)
    : StyleHighlightData(other) {}
scoped_refptr<StyleHighlightData> StyleHighlightData::Create(PkComputedStyle) {
  return base::AdoptRef(new StyleHighlightData);
}
scoped_refptr<StyleHighlightData> StyleHighlightData::Copy(
    PkComputedStyle) const {
  return base::AdoptRef(new StyleHighlightData(*this));
}

StyleHighlightData::StyleHighlightData() {
  // Call the internal ctor, *not* CreateInitialStyleSingleton or similar, to
  // avoid an infinite tree of StyleHighlightData under each highlight style.
  selection_ = ComputedStyle::Create(PassKey());
  target_text_ = ComputedStyle::Create(PassKey());
  spelling_error_ = ComputedStyle::Create(PassKey());
  grammar_error_ = ComputedStyle::Create(PassKey());
}

StyleHighlightData::StyleHighlightData(const StyleHighlightData& other)
    // TODO(crbug.com/1024156): init field for ::highlight()
    : selection_(other.selection_),
      target_text_(other.target_text_),
      spelling_error_(other.spelling_error_),
      grammar_error_(other.grammar_error_) {}

bool StyleHighlightData::operator==(const StyleHighlightData& other) const {
  // TODO(crbug.com/1024156): compare field for ::highlight()
  return selection_ == other.selection_ && target_text_ == other.target_text_ &&
         spelling_error_ == other.spelling_error_ &&
         grammar_error_ == other.grammar_error_;
}

const scoped_refptr<ComputedStyle>& StyleHighlightData::Selection() const {
  return selection_;
}

const scoped_refptr<ComputedStyle>& StyleHighlightData::TargetText() const {
  return target_text_;
}

const scoped_refptr<ComputedStyle>& StyleHighlightData::SpellingError() const {
  return spelling_error_;
}

const scoped_refptr<ComputedStyle>& StyleHighlightData::GrammarError() const {
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
