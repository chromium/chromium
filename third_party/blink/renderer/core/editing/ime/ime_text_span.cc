// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/ime_text_span.h"

#include <algorithm>
#include "third_party/blink/public/web/web_ime_text_span.h"

namespace blink {

ImeTextSpan::ImeTextSpan(Type type,
                         unsigned start_offset,
                         unsigned end_offset,
                         const Color& underline_color,
                         ui::mojom::ImeTextSpanThickness thickness,
                         const Color& background_color,
                         const Color& suggestion_highlight_color,
                         bool remove_on_finish_composing,
                         const Vector<String>& suggestions)
    : type_(type),
      underline_color_(underline_color),
      thickness_(thickness),
      background_color_(background_color),
      suggestion_highlight_color_(suggestion_highlight_color),
      remove_on_finish_composing_(remove_on_finish_composing),
      suggestions_(suggestions) {
  // Sanitize offsets by ensuring a valid range corresponding to the last
  // possible position.
  // TODO(wkorman): Consider replacing with DCHECK_LT(startOffset, endOffset).
  start_offset_ =
      std::min(start_offset, std::numeric_limits<unsigned>::max() - 1u);
  end_offset_ = std::max(start_offset_ + 1u, end_offset);
}

namespace {

Vector<String> ConvertStdVectorOfStdStringsToVectorOfStrings(
    const std::vector<std::string>& input) {
  Vector<String> output;
  output.ReserveInitialCapacity(input.size());
  for (const std::string& val : input) {
    output.UncheckedAppend(String::FromUTF8(val));
  }
  return output;
}

ImeTextSpan::Type ConvertWebTypeToType(WebImeTextSpan::Type type) {
  switch (type) {
    case WebImeTextSpan::Type::kComposition:
      return ImeTextSpan::Type::kComposition;
    case WebImeTextSpan::Type::kSuggestion:
      return ImeTextSpan::Type::kSuggestion;
    case WebImeTextSpan::Type::kMisspellingSuggestion:
      return ImeTextSpan::Type::kMisspellingSuggestion;
  }

  NOTREACHED();
  return ImeTextSpan::Type::kComposition;
}

}  // namespace

ImeTextSpan::ImeTextSpan(const WebImeTextSpan& ime_text_span)
    : ImeTextSpan(ConvertWebTypeToType(ime_text_span.type),
                  ime_text_span.start_offset,
                  ime_text_span.end_offset,
                  Color(ime_text_span.underline_color),
                  ime_text_span.thickness,
                  Color(ime_text_span.background_color),
                  Color(ime_text_span.suggestion_highlight_color),
                  ime_text_span.remove_on_finish_composing,
                  ConvertStdVectorOfStdStringsToVectorOfStrings(
                      ime_text_span.suggestions)) {}
}  // namespace blink
