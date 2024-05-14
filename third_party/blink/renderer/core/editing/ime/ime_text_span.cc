// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/ime_text_span.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/ime_types.mojom-blink.h"

namespace blink {

ImeTextSpan::Type ConvertUiTypeToType(ui::ImeTextSpan::Type type) {
  switch (type) {
    case ui::ImeTextSpan::Type::kComposition:
      return ImeTextSpan::Type::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return ImeTextSpan::Type::kSuggestion;
    case ui::ImeTextSpan::Type::kMisspellingSuggestion:
      return ImeTextSpan::Type::kMisspellingSuggestion;
    case ui::ImeTextSpan::Type::kAutocorrect:
      return ImeTextSpan::Type::kAutocorrect;
    case ui::ImeTextSpan::Type::kGrammarSuggestion:
      return ImeTextSpan::Type::kGrammarSuggestion;
  }

  NOTREACHED_IN_MIGRATION();
  return ImeTextSpan::Type::kComposition;
}

ImeTextSpan::ImeTextSpan(Type type,
                         wtf_size_t start_offset,
                         wtf_size_t end_offset,
                         const Color& underline_color,
                         ui::mojom::ImeTextSpanThickness thickness,
                         ui::mojom::ImeTextSpanUnderlineStyle underline_style,
                         const Color& text_color,
                         const Color& background_color,
                         const Color& suggestion_highlight_color,
                         bool remove_on_finish_composing,
                         bool interim_char_selection,
                         const Vector<String>& suggestions)
    : type_(type),
      underline_color_(underline_color),
      thickness_(thickness),
      underline_style_(underline_style),
      text_color_(text_color),
      background_color_(background_color),
      suggestion_highlight_color_(suggestion_highlight_color),
      remove_on_finish_composing_(remove_on_finish_composing),
      interim_char_selection_(interim_char_selection),
      suggestions_(suggestions) {
  // Sanitize offsets by ensuring a valid range corresponding to the last
  // possible position.
  // TODO(wkorman): Consider replacing with DCHECK_LT(startOffset, endOffset).
  start_offset_ =
      std::min(start_offset, std::numeric_limits<wtf_size_t>::max() - 1u);
  end_offset_ = std::max(start_offset_ + 1u, end_offset);
}

namespace {

Vector<String> ConvertStdVectorOfStdStringsToVectorOfStrings(
    const std::vector<std::string>& input) {
  Vector<String> output;
  output.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(input.size()));
  for (const std::string& val : input) {
    output.UncheckedAppend(String::FromUTF8(val));
  }
  return output;
}

std::vector<std::string> ConvertVectorOfStringsToStdVectorOfStdStrings(
    const Vector<String>& input) {
  std::vector<std::string> output;
  output.reserve(input.size());
  for (const String& val : input) {
    output.push_back(val.Utf8());
  }
  return output;
}

ui::mojom::ImeTextSpanThickness ConvertUiThicknessToThickness(
    ui::ImeTextSpan::Thickness thickness) {
  switch (thickness) {
    case ui::ImeTextSpan::Thickness::kNone:
      return ui::mojom::ImeTextSpanThickness::kNone;
    case ui::ImeTextSpan::Thickness::kThin:
      return ui::mojom::ImeTextSpanThickness::kThin;
    case ui::ImeTextSpan::Thickness::kThick:
      return ui::mojom::ImeTextSpanThickness::kThick;
  }

  NOTREACHED_IN_MIGRATION();
  return ui::mojom::ImeTextSpanThickness::kNone;
}

ui::mojom::ImeTextSpanUnderlineStyle ConvertUiUnderlineToUnderline(
    ui::ImeTextSpan::UnderlineStyle underline) {
  switch (underline) {
    case ui::ImeTextSpan::UnderlineStyle::kNone:
      return ui::mojom::ImeTextSpanUnderlineStyle::kNone;
    case ui::ImeTextSpan::UnderlineStyle::kSolid:
      return ui::mojom::ImeTextSpanUnderlineStyle::kSolid;
    case ui::ImeTextSpan::UnderlineStyle::kDot:
      return ui::mojom::ImeTextSpanUnderlineStyle::kDot;
    case ui::ImeTextSpan::UnderlineStyle::kDash:
      return ui::mojom::ImeTextSpanUnderlineStyle::kDash;
    case ui::ImeTextSpan::UnderlineStyle::kSquiggle:
      return ui::mojom::ImeTextSpanUnderlineStyle::kSquiggle;
  }

  NOTREACHED_IN_MIGRATION();
  return ui::mojom::ImeTextSpanUnderlineStyle::kNone;
}

ui::ImeTextSpan::Type ConvertImeTextSpanTypeToUiType(ImeTextSpan::Type type) {
  switch (type) {
    case ImeTextSpan::Type::kAutocorrect:
      return ui::ImeTextSpan::Type::kAutocorrect;
    case ImeTextSpan::Type::kComposition:
      return ui::ImeTextSpan::Type::kComposition;
    case ImeTextSpan::Type::kGrammarSuggestion:
      return ui::ImeTextSpan::Type::kGrammarSuggestion;
    case ImeTextSpan::Type::kMisspellingSuggestion:
      return ui::ImeTextSpan::Type::kMisspellingSuggestion;
    case ImeTextSpan::Type::kSuggestion:
      return ui::ImeTextSpan::Type::kSuggestion;
  }
}

}  // namespace

ImeTextSpan::ImeTextSpan(const ui::ImeTextSpan& ime_text_span)
    : ImeTextSpan(ConvertUiTypeToType(ime_text_span.type),
                  base::checked_cast<wtf_size_t>(ime_text_span.start_offset),
                  base::checked_cast<wtf_size_t>(ime_text_span.end_offset),
                  Color::FromSkColor(ime_text_span.underline_color),
                  ConvertUiThicknessToThickness(ime_text_span.thickness),
                  ConvertUiUnderlineToUnderline(ime_text_span.underline_style),
                  Color::FromSkColor(ime_text_span.text_color),
                  Color::FromSkColor(ime_text_span.background_color),
                  Color::FromSkColor(ime_text_span.suggestion_highlight_color),
                  ime_text_span.remove_on_finish_composing,
                  ime_text_span.interim_char_selection,
                  ConvertStdVectorOfStdStringsToVectorOfStrings(
                      ime_text_span.suggestions)) {}

ui::ImeTextSpan ImeTextSpan::ToUiImeTextSpan() {
  auto span = ui::ImeTextSpan(ConvertImeTextSpanTypeToUiType(GetType()),
                              StartOffset(), EndOffset());
  span.suggestions =
      ConvertVectorOfStringsToStdVectorOfStdStrings(Suggestions());
  return span;
}

}  // namespace blink
