// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_tokenizer_input_stream.h"

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

void CSSTokenizerInputStream::AdvanceUntilNonWhitespace() {
  // Using HTML space here rather than CSS space since we don't do preprocessing
  if (string_.Is8Bit()) {
    const LChar* characters = string_.Characters8();
    while (offset_ < string_length_ && IsHTMLSpace(characters[offset_])) {
      ++offset_;
    }
  } else {
    const UChar* characters = string_.Characters16();
    while (offset_ < string_length_ && IsHTMLSpace(characters[offset_])) {
      ++offset_;
    }
  }
}

double CSSTokenizerInputStream::GetDouble(unsigned start, unsigned end) const {
  DCHECK(start <= end && ((offset_ + end) <= string_length_));
  bool is_result_ok = false;
  double result = 0.0;
  if (start < end) {
    if (string_.Is8Bit()) {
      result = CharactersToDouble(string_.Characters8() + offset_ + start,
                                  end - start, &is_result_ok);
    } else {
      result = CharactersToDouble(string_.Characters16() + offset_ + start,
                                  end - start, &is_result_ok);
    }
  }
  // FIXME: It looks like callers ensure we have a valid number
  return is_result_ok ? result : 0.0;
}

}  // namespace blink
