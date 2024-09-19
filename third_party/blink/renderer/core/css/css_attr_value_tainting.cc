// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_attr_value_tainting.h"

#include <mutex>

#include "base/containers/span.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

class CSSParserTokenStream;

static char blink_taint_token[64];
static unsigned blink_taint_token_length = 0;

StringView GetCSSAttrTaintToken() {
  static std::once_flag flag;
  std::call_once(flag, [] {
    base::UnguessableToken token = base::UnguessableToken::Create();

    // The token is chosen so that it is very unlikely to show up in an
    // actual stylesheet. (It also contains a very unusual character,
    // namely NUL, so that it is easy to fast-reject strings that do not
    // contain it.) It should not be guessable, but even if it were,
    // the worst thing the user could do it to cause a false positive,
    // causing their own URLs not to load.
    StringBuilder sb;
    sb.Append("/*");
    sb.Append('\0');
    sb.Append("blinktaint-");
    for (const uint8_t ch : token.AsBytes()) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%02x", ch);
      sb.Append(buf);
    }
    sb.Append("*/");

    String str = sb.ReleaseString();
    SECURITY_CHECK(str.length() < sizeof(blink_taint_token));
    blink_taint_token_length = str.length();
    memcpy(blink_taint_token, str.Characters8(), blink_taint_token_length);
  });
  SECURITY_CHECK(blink_taint_token_length > 0);
  return {blink_taint_token, blink_taint_token_length};
}

bool IsAttrTainted(const CSSParserTokenStream& stream,
                   wtf_size_t start_offset,
                   wtf_size_t end_offset) {
  return IsAttrTainted(
      stream.StringRangeAt(start_offset, end_offset - start_offset));
}

bool IsAttrTainted(StringView str) {
  if (str.Is8Bit() &&
      memchr(str.Characters8(), '\0', str.length()) == nullptr) {
    // Fast reject. This is important, because it allows us to skip
    // ToString() below (the only usable substring search in WTF
    // seems to be on a StringImpl).
    return false;
  }
  return str.ToString().Contains(GetCSSAttrTaintToken());
}

String RemoveAttrTaintToken(StringView str) {
  StringBuilder out;
  CSSTokenizer tokenizer(str);
  StringView taint_token = GetCSSAttrTaintToken();
  wtf_size_t prev_offset = 0;
  while (true) {
    CSSParserToken token = tokenizer.TokenizeSingleWithComments();
    if (token.IsEOF()) {
      break;
    }
    wtf_size_t offset = tokenizer.Offset();
    StringView token_str =
        tokenizer.StringRangeAt(prev_offset, offset - prev_offset);
    if (token.GetType() != kCommentToken || token_str != taint_token) {
      out.Append(token_str);
    }
    prev_offset = offset;
  }
  return out.ToString();
}

}  // namespace blink
