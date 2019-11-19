// Copyright 2019 The Chromium Authors. All rights reserved.
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
  if (next == '\r' && input.PeekWithoutReplacement(1) == '\n')
    input.Advance(2);
  else if (IsHTMLSpace(next))
    input.Advance();
}

// https://drafts.csswg.org/css-syntax/#consume-an-escaped-code-point
UChar32 ConsumeEscape(CSSTokenizerInputStream& input) {
  UChar cc = input.NextInputChar();
  input.Advance();
  DCHECK(!IsCSSNewLine(cc));
  if (IsASCIIHexDigit(cc)) {
    unsigned consumed_hex_digits = 1;
    StringBuilder hex_chars;
    hex_chars.Append(cc);
    while (consumed_hex_digits < 6 &&
           IsASCIIHexDigit(input.PeekWithoutReplacement(0))) {
      cc = input.NextInputChar();
      input.Advance();
      hex_chars.Append(cc);
      consumed_hex_digits++;
    };
    ConsumeSingleWhitespaceIfNext(input);
    bool ok = false;
    UChar32 code_point = hex_chars.ToString().HexToUIntStrict(&ok);
    DCHECK(ok);
    if (code_point == 0 || (0xD800 <= code_point && code_point <= 0xDFFF) ||
        code_point > 0x10FFFF)
      return kReplacementCharacter;
    return code_point;
  }

  if (cc == kEndOfFileMarker)
    return kReplacementCharacter;
  return cc;
}

// http://www.w3.org/TR/css3-syntax/#consume-a-name
String ConsumeName(CSSTokenizerInputStream& input) {
  StringBuilder result;
  while (true) {
    UChar cc = input.NextInputChar();
    input.Advance();
    if (IsNameCodePoint(cc)) {
      result.Append(cc);
      continue;
    }
    if (TwoCharsAreValidEscape(cc, input.PeekWithoutReplacement(0))) {
      result.Append(ConsumeEscape(input));
      continue;
    }
    input.PushBack(cc);
    return result.ToString();
  }
}

// https://drafts.csswg.org/css-syntax/#would-start-an-identifier
bool NextCharsAreIdentifier(UChar first, const CSSTokenizerInputStream& input) {
  UChar second = input.PeekWithoutReplacement(0);
  if (IsNameStartCodePoint(first) || TwoCharsAreValidEscape(first, second))
    return true;

  if (first == '-') {
    return IsNameStartCodePoint(second) || second == '-' ||
           TwoCharsAreValidEscape(second, input.PeekWithoutReplacement(1));
  }

  return false;
}

}  // namespace blink
