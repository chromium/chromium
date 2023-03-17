// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_RANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <algorithm>
#include <utility>

namespace blink {

CORE_EXPORT extern const CSSParserToken& g_static_eof_token;

// A CSSParserTokenRange is an iterator over a subrange of a vector of
// CSSParserTokens. Accessing outside of the range will return an endless stream
// of EOF tokens. This class refers to half-open intervals [first, last).
class CORE_EXPORT CSSParserTokenRange {
  DISALLOW_NEW();

 public:
  template <wtf_size_t InlineBuffer>
  CSSParserTokenRange(const Vector<CSSParserToken, InlineBuffer>& vector)
      : first_(vector.begin()), last_(vector.end()) {}
  explicit CSSParserTokenRange(base::span<CSSParserToken> tokens)
      : first_(tokens.data()), last_(tokens.data() + tokens.size()) {}

  // This should be called on a range with tokens returned by that range.
  CSSParserTokenRange MakeSubRange(const CSSParserToken* first,
                                   const CSSParserToken* last) const;

  bool AtEnd() const { return first_ == last_; }
  const CSSParserToken* end() const { return last_; }
  wtf_size_t size() const { return static_cast<wtf_size_t>(last_ - first_); }

  const CSSParserToken& Peek(wtf_size_t offset = 0) const {
    if (first_ + offset >= last_) {
      return g_static_eof_token;
    }
    return *(first_ + offset);
  }

  base::span<const CSSParserToken> RemainingSpan() const {
    return {first_, last_};
  }

  const CSSParserToken& Consume() {
    if (first_ == last_) {
      return g_static_eof_token;
    }
    return *first_++;
  }

  const CSSParserToken& ConsumeIncludingWhitespace() {
    const CSSParserToken& result = Consume();
    ConsumeWhitespace();
    return result;
  }

  // The returned range doesn't include the brackets
  CSSParserTokenRange ConsumeBlock();

  void ConsumeComponentValue();

  void ConsumeWhitespace() {
    while (Peek().GetType() == kWhitespaceToken) {
      ++first_;
    }
  }

  String Serialize() const;

  const CSSParserToken* begin() const { return first_; }

  static void InitStaticEOFToken();

 private:
  CSSParserTokenRange(const CSSParserToken* first, const CSSParserToken* last)
      : first_(first), last_(last) {}

  const CSSParserToken* first_;
  const CSSParserToken* last_;
};

// An auxiliary class that can recover the exact string used for a set of
// tokens. It stores per-token offsets (such as from
// CSSTokenizer::TokenizeToEOFWithOffsets()) and a pointer to the original
// string (which must live for at least as long as this class), and from that,
// it can give you the exact string that a given token range came from.
class CSSParserTokenOffsets {
 public:
  template <wtf_size_t InlineBuffer>
  CSSParserTokenOffsets(const Vector<CSSParserToken, InlineBuffer>& vector,
                        Vector<wtf_size_t, 32> offsets,
                        StringView string)
      : first_(vector.begin()), offsets_(std::move(offsets)), string_(string) {
    DCHECK_EQ(vector.size() + 1, offsets_.size());
  }
  CSSParserTokenOffsets(base::span<const CSSParserToken> tokens,
                        Vector<wtf_size_t, 32> offsets,
                        StringView string)
      : first_(tokens.data()), offsets_(std::move(offsets)), string_(string) {
    DCHECK_EQ(tokens.size() + 1, offsets_.size());
  }

  wtf_size_t OffsetFor(const CSSParserToken* token) const {
    DCHECK_GE(token, first_);
    DCHECK_LT(token, first_ + offsets_.size() - 1);
    wtf_size_t token_index = static_cast<wtf_size_t>(token - first_);
    return offsets_[token_index];
  }

  StringView StringForTokens(const CSSParserToken* begin,
                             const CSSParserToken* end) const {
    wtf_size_t begin_offset = OffsetFor(begin);
    wtf_size_t end_offset = OffsetFor(end);
    return StringView(string_, begin_offset, end_offset - begin_offset);
  }

 private:
  const CSSParserToken* first_;
  Vector<wtf_size_t, 32> offsets_;
  StringView string_;
};

bool NeedsInsertedComment(const CSSParserToken& a, const CSSParserToken& b);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_RANGE_H_
