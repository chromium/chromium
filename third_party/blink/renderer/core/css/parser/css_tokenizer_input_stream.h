// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_INPUT_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_INPUT_STREAM_H_

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSTokenizerInputStream {
 public:
  explicit CSSTokenizerInputStream(StringView input)
      : string_(input), rest_(input) {}

  CSSTokenizerInputStream(const CSSTokenizerInputStream&) = delete;
  CSSTokenizerInputStream& operator=(const CSSTokenizerInputStream&) = delete;

  // Gets the char in the stream replacing NUL characters and lone surrogates
  // with a unicode replacement character, per CSS input preprocessing:
  // https://www.w3.org/TR/css-syntax-3/#input-preprocessing
  // Will return (NUL) kEndOfFileMarker when at the end of the stream.
  UChar NextInputChar() const {
    if (rest_.empty()) {
      return '\0';
    }

    // "Replace any U+0000 NULL or surrogate code points in input with U+FFFD
    // REPLACEMENT CHARACTER"
    // "surrogate code points" refers to standalone surrogates in this scenario
    // (e.g. a leading without a subsequent trailing and vice versa).
    UChar result = rest_[0];
    if (!result ||
        (IsLeadingSurrogate(result) &&
         !IsTrailingSurrogate(PeekWithoutReplacement(1))) ||
        (IsTrailingSurrogate(result) &&
         !IsLeadingSurrogate(PeekPreviousCharWithoutReplacement()))) {
      return 0xFFFD;
    }
    return result;
  }

  // Gets the previous char in the stream without replacement. Returns NUL if
  // at the beginning of the stream.
  UChar PeekPreviousCharWithoutReplacement() const {
    if (Offset() == 0) {
      return '\0';
    }
    return string_[Offset() - 1];
  }
  // Gets the char at lookaheadOffset from the current stream position. Will
  // return NUL (kEndOfFileMarker) if the stream position is at the end.
  // NOTE: This may *also* return NUL if there's one in the input! Never
  // compare the return value to '\0'.
  UChar PeekWithoutReplacement(unsigned lookahead_offset) const {
    if (lookahead_offset >= rest_.length()) {
      return '\0';
    }
    return rest_[lookahead_offset];
  }
  StringView Peek() const { return rest_; }

  // NOTE: If there isn't enough data left to advance (e.g., because we are
  // already at the end), we will silently go to the end of the string.
  // In particular, this means if you Advance(1) and then PushBack(),
  // you will not end up at EOF even if that's where you started.
  void Advance(unsigned offset = 1) {
    rest_ = StringView(rest_, std::min(offset, rest_.length()));
  }
  void PushBack(UChar cc) {
    rest_ = StringView(string_, Offset() - 1);
    DCHECK_EQ(NextInputChar(), cc);
  }

  double GetDouble(unsigned start, unsigned end) const;

  // Like GetDouble(), but only for the case where the number matches
  // [0-9]+ (no decimal point, no exponent, no sign), and is faster.
  double GetNaturalNumberAsDouble(unsigned start, unsigned end) const;

  template <bool characterPredicate(UChar)>
  unsigned SkipWhilePredicate(unsigned offset) {
    if (string_.Is8Bit()) {
      for (const LChar ch : rest_.Span8().subspan(offset)) {
        if (!characterPredicate(ch)) {
          break;
        }
        ++offset;
      }
    } else {
      for (const UChar ch : rest_.Span16().subspan(offset)) {
        if (!characterPredicate(ch)) {
          break;
        }
        ++offset;
      }
    }
    return offset;
  }

  void AdvanceUntilNonWhitespace();

  unsigned length() const { return string_.length(); }
  unsigned Offset() const { return string_.length() - rest_.length(); }
  bool AtEnd() const { return rest_.empty(); }

  StringView RangeFrom(unsigned start) const {
    return StringView(string_, start);
  }

  StringView RangeAt(unsigned start, unsigned length) const {
    DCHECK_LE(start + length, string_.length());
    return StringView(string_, start, length);
  }

  void Restore(wtf_size_t offset) { rest_ = StringView(string_, offset); }

 private:
  // The original string.
  const StringView string_;

  // The subset of the original string that we have not parsed yet.
  StringView rest_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_TOKENIZER_INPUT_STREAM_H_
