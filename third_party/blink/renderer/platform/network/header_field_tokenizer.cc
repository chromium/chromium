// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/header_field_tokenizer.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

using Mode = HeaderFieldTokenizer::Mode;

bool IsTokenCharacter(Mode mode, UChar c) {
  // TODO(cvazac) change this to use LChar
  // TODO(cvazac) Check HTTPArchive for usage and possible deprecation.
  // According to https://tools.ietf.org/html/rfc7230#appendix-B, the
  // following characters (ASCII decimal) should not be included in a TOKEN:
  // 123 ('{')
  // 125 ('}')
  // 127 (delete)

  if (c >= 128)
    return false;
  if (c < 0x20)
    return false;

  switch (c) {
    case ' ':
    case ';':
    case '"':
      return false;
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ':':
    case '\\':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
      return mode == Mode::kRelaxed;
    default:
      return true;
  }
}

}  // namespace

HeaderFieldTokenizer::HeaderFieldTokenizer(const String& header_field)
    : index_(0u), input_(header_field) {
  SkipOptionalWhitespace();
}

HeaderFieldTokenizer::HeaderFieldTokenizer(HeaderFieldTokenizer&&) = default;

bool HeaderFieldTokenizer::Consume(char c) {
  // TODO(cvazac) change this to use LChar
  DCHECK_NE(c, ' ');
  DCHECK_NE(c, '\t');

  if (IsConsumed() || input_[index_] != c)
    return false;

  ++index_;
  SkipOptionalWhitespace();
  return true;
}

bool HeaderFieldTokenizer::ConsumeQuotedString(String& output) {
  StringBuilder builder;

  DCHECK_EQ('"', input_[index_]);
  ++index_;

  while (!IsConsumed()) {
    if (input_[index_] == '"') {
      output = builder.ToString();
      ++index_;
      SkipOptionalWhitespace();
      return true;
    }
    if (input_[index_] == '\\') {
      ++index_;
      if (IsConsumed())
        return false;
    }
    builder.Append(input_[index_]);
    ++index_;
  }
  return false;
}

bool HeaderFieldTokenizer::ConsumeToken(Mode mode, StringView& output) {
  DCHECK(output.IsNull());

  auto start = index_;
  while (!IsConsumed() && IsTokenCharacter(mode, input_[index_]))
    ++index_;

  if (start == index_)
    return false;

  output = StringView(input_, start, index_ - start);
  SkipOptionalWhitespace();
  return true;
}

bool HeaderFieldTokenizer::ConsumeTokenOrQuotedString(Mode mode,
                                                      String& output) {
  if (IsConsumed())
    return false;

  if (input_[index_] == '"')
    return ConsumeQuotedString(output);

  StringView view;
  if (!ConsumeToken(mode, view))
    return false;
  output = view.ToString();
  return true;
}

void HeaderFieldTokenizer::SkipOptionalWhitespace() {
  while (!IsConsumed() && (input_[index_] == ' ' || input_[index_] == '\t'))
    ++index_;
}

void HeaderFieldTokenizer::ConsumeBeforeAnyCharMatch(Vector<LChar> chars) {
  // TODO(cvazac) move this to HeaderFieldTokenizer c'tor
  DCHECK(input_.Is8Bit());

  DCHECK_GT(chars.size(), 0U);
  DCHECK_LT(chars.size(), 3U);

  while (!IsConsumed()) {
    for (const auto& c : chars) {
      if (c == input_[index_]) {
        return;
      }
    }

    ++index_;
  }
}

}  // namespace blink
