// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"

#include "third_party/blink/renderer/core/css/parser/css_tokenizer_input_stream.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

void ConsumeSingleWhitespaceIfNext(CSSTokenizerInputStream& input) {
  // We check for \r\n and HTML spaces since we don't do preprocessing
  UChar next = input.PeekWithoutReplacement(0);
  if (next == '\r' && input.PeekWithoutReplacement(1) == '\n') {
    input.Advance(2);
  } else if (IsHTMLSpace(next)) {
    input.Advance();
  }
}

// https://drafts.csswg.org/css-syntax/#consume-an-escaped-code-point
UChar32 ConsumeEscape(CSSTokenizerInputStream& input) {
  UChar cc = input.NextInputChar();
  input.Advance();
  DCHECK(!IsCSSNewLine(cc));
  if (IsAsciiHexDigit(cc)) {
    // At most six hex digits, so this cannot overflow.
    UChar32 code_point = ToAsciiHexValue(cc);
    unsigned consumed_hex_digits = 1;
    while (consumed_hex_digits < 6) {
      cc = input.PeekWithoutReplacement(0);
      if (!IsAsciiHexDigit(cc)) {
        break;
      }
      input.Advance();
      code_point = (code_point << 4) | ToAsciiHexValue(cc);
      consumed_hex_digits++;
    }
    ConsumeSingleWhitespaceIfNext(input);
    if (code_point == 0 || (0xD800 <= code_point && code_point <= 0xDFFF) ||
        code_point > 0x10FFFF) {
      return kReplacementCharacter;
    }
    return code_point;
  }

  if (cc == kEndOfFileMarker) {
    return kReplacementCharacter;
  }
  return cc;
}

// http://www.w3.org/TR/css3-syntax/#consume-a-name
String ConsumeName(CSSTokenizerInputStream& input) {
  StringBuilder result;
  while (true) {
    UChar cc = input.NextInputChar();
    if (IsNameCodePoint(cc)) {
      input.Advance();
      result.Append(cc);
    } else if (TwoCharsAreValidEscape(cc, input.PeekWithoutReplacement(1))) {
      input.Advance();
      result.Append(ConsumeEscape(input));
    } else {
      return result.ReleaseString();
    }
  }
}

// https://drafts.csswg.org/css-syntax/#would-start-an-identifier
bool NextCharsAreIdentifier(UChar first, const CSSTokenizerInputStream& input) {
  UChar second = input.NextInputChar();
  if (IsNameStartCodePoint(first) || TwoCharsAreValidEscape(first, second)) {
    return true;
  }

  if (first == '-') {
    return IsNameStartCodePoint(second) || second == '-' ||
           TwoCharsAreValidEscape(second, input.PeekWithoutReplacement(1));
  }

  return false;
}

}  // namespace blink
