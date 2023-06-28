// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_STREAM_H_

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace detail {

template <typename...>
bool IsTokenTypeOneOf(CSSParserTokenType t) {
  return false;
}

template <CSSParserTokenType Head, CSSParserTokenType... Tail>
bool IsTokenTypeOneOf(CSSParserTokenType t) {
  return t == Head || IsTokenTypeOneOf<Tail...>(t);
}

}  // namespace detail

// A streaming interface to CSSTokenizer that tokenizes on demand.
// Abstractly, the stream ends at either EOF or the beginning/end of a block.
// To consume a block, a BlockGuard must be created first to ensure that
// we finish consuming a block even if there was an error.
//
// Methods prefixed with "Unchecked" can only be called after calls to Peek(),
// EnsureLookAhead(), or AtEnd() with no subsequent modifications to the stream
// such as a consume.
class CORE_EXPORT CSSParserTokenStream {
  DISALLOW_NEW();

 public:
  // Instantiate this to start reading from a block. When the guard is out of
  // scope, the rest of the block is consumed.
  class BlockGuard {
    STACK_ALLOCATED();

   public:
    explicit BlockGuard(CSSParserTokenStream& stream) : stream_(stream) {
      const CSSParserToken next = stream.ConsumeInternal();
      DCHECK_EQ(next.GetBlockType(), CSSParserToken::kBlockStart);
    }

    void SkipToEndOfBlock() {
      DCHECK(!skipped_to_end_of_block_);
      stream_.EnsureLookAhead();
      stream_.UncheckedSkipToEndOfBlock();
      skipped_to_end_of_block_ = true;
    }

    ~BlockGuard() {
      if (!skipped_to_end_of_block_) {
        SkipToEndOfBlock();
      }
    }

   private:
    CSSParserTokenStream& stream_;
    bool skipped_to_end_of_block_ = false;
  };

  static constexpr uint64_t FlagForTokenType(CSSParserTokenType token_type) {
    return 1ull << static_cast<uint64_t>(token_type);
  }

  // The specified token type will be treated as kEOF while the Boundary is on
  // the stack.
  class Boundary {
    STACK_ALLOCATED();

   public:
    Boundary(CSSParserTokenStream& stream, CSSParserTokenType boundary_type)
        : auto_reset_(&stream.boundaries_,
                      stream.boundaries_ | FlagForTokenType(boundary_type)) {}
    ~Boundary() = default;

   private:
    base::AutoReset<uint64_t> auto_reset_;
  };

  // We found that this value works well empirically by printing out the
  // maximum buffer size for a few top alexa websites. It should be slightly
  // above the expected number of tokens in the prelude of an at rule and
  // the number of tokens in a declaration.
  // TODO(crbug.com/661854): Can we streamify at rule parsing so that this is
  // only needed for declarations which are easier to think about?
  static constexpr int kInitialBufferSize = 128;

  explicit CSSParserTokenStream(CSSTokenizer& tokenizer)
      : tokenizer_(tokenizer), next_(kEOFToken) {}

  CSSParserTokenStream(CSSParserTokenStream&&) = default;
  CSSParserTokenStream(const CSSParserTokenStream&) = delete;
  CSSParserTokenStream& operator=(const CSSParserTokenStream&) = delete;

  inline void EnsureLookAhead() {
    if (!HasLookAhead()) {
      has_look_ahead_ = true;
      next_ = tokenizer_.TokenizeSingle();
    }
  }

  // Forcibly read a lookahead token.
  inline void LookAhead() {
    DCHECK(!HasLookAhead());
    next_ = tokenizer_.TokenizeSingle();
    has_look_ahead_ = true;
  }

  inline bool HasLookAhead() const { return has_look_ahead_; }

  inline const CSSParserToken& Peek() {
    EnsureLookAhead();
    return next_;
  }

  inline const CSSParserToken& UncheckedPeek() const {
    DCHECK(HasLookAhead());
    return next_;
  }

  inline const CSSParserToken& Consume() {
    EnsureLookAhead();
    return UncheckedConsume();
  }

  const CSSParserToken& UncheckedConsume() {
    DCHECK(HasLookAhead());
    DCHECK_NE(next_.GetBlockType(), CSSParserToken::kBlockStart);
    DCHECK_NE(next_.GetBlockType(), CSSParserToken::kBlockEnd);
    has_look_ahead_ = false;
    offset_ = tokenizer_.Offset();
    return next_;
  }

  // Allows you to read past the end of blocks without recursing,
  // which is normally not what you want to do when parsing
  // (it is really only useful during variable substitution).
  inline const CSSParserToken& ConsumeRaw() {
    EnsureLookAhead();
    return UncheckedConsumeRaw();
  }

  // See ConsumeRaw().
  const CSSParserToken& UncheckedConsumeRaw() {
    DCHECK(HasLookAhead());
    has_look_ahead_ = false;
    offset_ = tokenizer_.Offset();
    return next_;
  }

  inline bool AtEnd() {
    EnsureLookAhead();
    return UncheckedAtEnd();
  }

  inline bool UncheckedAtEnd() const {
    DCHECK(HasLookAhead());
    return (boundaries_ & FlagForTokenType(next_.GetType())) ||
           next_.GetBlockType() == CSSParserToken::kBlockEnd;
  }

  // Get the index of the character in the original string to be consumed next.
  wtf_size_t Offset() const { return offset_; }

  // Get the index of the starting character of the look-ahead token.
  wtf_size_t LookAheadOffset() const {
    DCHECK(HasLookAhead());
    return tokenizer_.PreviousOffset();
  }

  // Returns a view on a range of characters in the original string.
  StringView StringRangeAt(wtf_size_t start, wtf_size_t length) const;

  void ConsumeWhitespace();
  CSSParserToken ConsumeIncludingWhitespace();
  CSSParserToken ConsumeIncludingWhitespaceRaw();  // See ConsumeRaw().
  void UncheckedConsumeComponentValue();

  // Either consumes a comment token and returns true, or peeks at the next
  // token and return false.
  bool ConsumeCommentOrNothing();

  // Consume tokens until one of these is true:
  //
  //  - EOF is reached.
  //  - The next token would signal a premature end of the current block
  //    (an unbalanced } or similar).
  //  - The next token is of any of the given types, except if it occurs
  //    within a block.
  //
  // The range of tokens that we consume is returned. So e.g., if we ask
  // to stop at semicolons, and the rest of the input looks like
  // “.foo { color; } bar ; baz”, we would return “.foo { color; } bar ”
  // and stop there (the semicolon would remain in the lookahead slot).
  //
  // Invalidates any ranges created by previous calls to
  // ConsumeUntilPeekedTypeIs().
  template <CSSParserTokenType... Types>
  CSSParserTokenRange ConsumeUntilPeekedTypeIs() {
    EnsureLookAhead();

    // Check if the existing lookahead token already marks the end;
    // if so, try to exit as soon as possible. (This is a fairly common
    // case, because some places call ConsumeUntilPeekedTypeIs() just to
    // ignore garbage after a declaration, and there usually is no such
    // garbage.)
    if (next_.IsEOF() || TokenMarksEnd<Types...>(next_)) {
      return CSSParserTokenRange(base::span<CSSParserToken>{});
    }

    buffer_.Shrink(0);

    // Process the lookahead token.
    buffer_.push_back(next_);
    unsigned nesting_level = 0;
    if (next_.GetBlockType() == CSSParserToken::kBlockStart) {
      nesting_level++;
    }

    // Add tokens to our return vector until we see either EOF or we meet the
    // return condition. (The termination condition is within the loop.)
    while (true) {
      buffer_.push_back(tokenizer_.TokenizeSingle());
      if (buffer_.back().IsEOF() ||
          (nesting_level == 0 && TokenMarksEnd<Types...>(buffer_.back()))) {
        // Undo the token we just pushed; it goes into the lookahead slot
        // instead.
        next_ = buffer_.back();
        buffer_.pop_back();
        offset_ = tokenizer_.PreviousOffset();
        break;
      } else if (buffer_.back().GetBlockType() == CSSParserToken::kBlockStart) {
        nesting_level++;
      } else if (buffer_.back().GetBlockType() == CSSParserToken::kBlockEnd) {
        nesting_level--;
      }
    }
    return CSSParserTokenRange(buffer_);
  }

  // Restarts
  // ========
  //
  // CSSParserTokenStream has limited restart capabilities through the
  // Save and Restore functions.
  //
  // Saving the stream is allowed under the following conditions:
  //
  //  1. There are no boundaries, except for the regular EOF boundary.
  //     (See inner class Boundary). This avoids having to store the boundaries
  //     in the stream snapshot.
  //  2. The lookahead token is present. (See HasLookAhead). This avoids having
  //     to store whether or not we have a lookahead token.
  //
  // Restoring the stream is allowed under the following conditions:
  //
  //  1. The lookahead token is present (at the time of Restore). This is
  //     important for undoing mutations to the tokenizer's block stack (see
  //     CSSTokenizer::Restore).
  //  2. The Save/Restore pair does not cross a BlockGuard.
  //
  //
  // Restoring
  // =========
  //
  // Suppose that we had a short string to tokenize.
  //
  //  - The '^' indicates the position of the tokenizer (CSSTokenizer).
  //  - The 'offset' indicates the value of CSSParserTokenStream::offset_.
  //
  // These values temporarily go out of sync when producing lookahead values,
  // because doing so moves the position of the tokenizer only. The stream
  // offset does not catch up until the lookahead is Consumed.
  //
  // The initial state looks like this:
  //
  //   span:hover { X }  [offset=0]
  //   ^
  // Ensuring lookahead moves the tokenizer position (but not the stream
  // offset):
  //
  //   span:hover { X }  [offset=0, lookahead=span]
  //       ^
  // Consuming that lookahead token makes the offset catch up:
  //
  //   span:hover { X }  [offset=4]
  //       ^
  // Ensure lookahead again:
  //
  //   span:hover { X }  [offset=4, lookahead=:]
  //        ^
  // Consuming again:
  //
  //   span:hover { X }  [offset=5]
  //        ^
  // Now suppose that we had saved the stream state earlier,
  // at [offset=0, lookahead=span] (keeping in mind that having lookahead is
  // a prerequisite for saving the stream). We can restore to that position,
  // provided that we first ensure lookahead:
  //
  //   span:hover { X }  [offset=5, lookahead=hover]
  //             ^
  // The restore process will then do two things. First, rewind the tokenizer's
  // position to that of the saved stream offset (0):
  //
  //   span:hover { X }  [offset=5, lookahead=hover]
  //   ^
  // Then, set the stream offset to that rewound tokenizer position (0),
  // and recreate the lookahead from that point:
  //
  //   span:hover { X }  [offset=0, lookahead=span]
  //       ^
  // Now that the restore is finished, we have exactly the same state as when
  // it was saved: [offset=0, lookahead=span].
  //
  //
  // Blocks
  // ======
  //
  // Suppose instead that we want to restore to offset=0 in this state:
  //
  //   span:hover { X }  [offset=11, lookahead={]
  //               ^
  // Now we have a problem, because producing the lookahead token for '{'
  // modified the block stack of the CSSTokenizer. This is why the restore
  // process requires a lookahead token: we inspect the block type of that
  // lookahead token to *undo* the mutation before the rest of the restore
  // process.
  //
  //  - If the lookahead token has BlockType::kBlockStart,
  //    then we simply pop the recently pushed token type from the stack.
  //  - If the lookahead token has BlockType::kBlockEnd,
  //    then we push the matching token type to the stack to restore the
  //    recently popped token type.
  //
  // Note that it's not possible to Consume past a block-start or block-end:
  // a BlockGuard is required to enter blocks, which also ensures that we always
  // consume the entire block. Note also that block-end tokens are treated as
  // EOF (see UncheckedAtEnd): it is therefore not possible to escape the
  // current block during a BlockGuard. For these reasons, we only ever need to
  // undo at most one mutation to the block stack: the block stack mutation
  // caused by the "final" lookahead before the restore process.

  wtf_size_t Save() const {
    DCHECK_EQ(boundaries_, FlagForTokenType(kEOFToken));
    DCHECK(has_look_ahead_);
    return offset_;
  }

  void Restore(wtf_size_t offset) {
    DCHECK(has_look_ahead_);
    offset_ = offset;
    boundaries_ = FlagForTokenType(kEOFToken);
    next_ = tokenizer_.Restore(next_, offset_);
  }

 private:
  template <CSSParserTokenType... EndTypes>
  ALWAYS_INLINE bool TokenMarksEnd(const CSSParserToken& token) {
    return (boundaries_ & FlagForTokenType(token.GetType())) ||
           token.GetBlockType() == CSSParserToken::kBlockEnd ||
           detail::IsTokenTypeOneOf<EndTypes...>(token.GetType());
  }

  const CSSParserToken& PeekInternal() {
    EnsureLookAhead();
    return UncheckedPeekInternal();
  }

  const CSSParserToken& UncheckedPeekInternal() const {
    DCHECK(HasLookAhead());
    return next_;
  }

  const CSSParserToken& ConsumeInternal() {
    EnsureLookAhead();
    return UncheckedConsumeInternal();
  }

  const CSSParserToken& UncheckedConsumeInternal() {
    DCHECK(HasLookAhead());
    has_look_ahead_ = false;
    offset_ = tokenizer_.Offset();
    return next_;
  }

  // Assuming the last token was a BlockStart token, ignores tokens
  // until the matching BlockEnd token or EOF. Requires but does _not_
  // leave a lookahead token active (for unknown reasons).
  void UncheckedSkipToEndOfBlock();

  Vector<CSSParserToken, kInitialBufferSize> buffer_;
  CSSTokenizer& tokenizer_;
  CSSParserToken next_;
  wtf_size_t offset_ = 0;
  bool has_look_ahead_ = false;
  uint64_t boundaries_ = FlagForTokenType(kEOFToken);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_STREAM_H_
