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
    explicit BlockGuard(CSSParserTokenStream& stream)
        : stream_(stream), boundaries_(stream.boundaries_) {
      const CSSParserToken next = stream.ConsumeInternal();
      DCHECK_EQ(next.GetBlockType(), CSSParserToken::kBlockStart);
      // Boundaries do not apply within blocks.
      stream.boundaries_ = FlagForTokenType(kEOFToken);
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
      stream_.boundaries_ = boundaries_;
    }

   private:
    CSSParserTokenStream& stream_;
    bool skipped_to_end_of_block_ = false;
    uint64_t boundaries_;
  };

  static constexpr uint64_t FlagForTokenType(CSSParserTokenType token_type) {
    return 1ull << static_cast<uint64_t>(token_type);
  }

  // The specified token type will be treated as kEOF while the Boundary is on
  // the stack. However, this does not apply within blocks. For example, if the
  // current boundary is kSemicolonToken, the parsing following will only treat
  // the ';' between 'b' and 'c' as kEOF, because the other ';'-tokens are
  // within a block:
  //
  //  a[1;2;3]b;c
  //
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

  // https://drafts.csswg.org/css-syntax-3/#consume-a-component-value
  //
  // This is similar to ConsumeUntilPeekedTypeIs, in that it returns
  // a range to an internal buffer that's invalidated on the next call
  // to either ConsumeComponentValue() or ConsumeUntilPeekedTypeIs(),
  // but instead of consuming until a specified token type, it just consumes
  // a single component value and returns the corresponding range.
  CSSParserTokenRange ConsumeComponentValue();

  CSSParserTokenRange ConsumeComponentValueIncludingWhitespace() {
    CSSParserTokenRange range = ConsumeComponentValue();
    ConsumeWhitespace();
    return range;
  }

  // Restarts
  // ========
  //
  // CSSParserTokenStream has limited restart capabilities through the
  // Save and Restore functions.
  //
  // Saving the stream is allowed under the condition that the lookahead token
  // is present. (See HasLookAhead). This avoids having to store whether or not
  // we have a lookahead token.
  //
  // Restoring the stream is allowed under the following conditions:
  //
  //  1. The lookahead token is present (at the time of Restore). This is
  //     important for undoing mutations to the tokenizer's block stack (see
  //     CSSTokenizer::Restore).
  //  2. The Save/Restore pair does not cross a BlockGuard.
  //  3. The Save/Restore pair does not cross a Boundary. (See section below).
  //     This limitation avoids having to store the boundary.
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
  //
  // Boundaries
  // ==========
  //
  // The state may be saved and restored during a Boundary, but the boundary
  // conditions must be the same during the call to Restore as they were
  // during the call to Save. For example, can you use a boundary that's
  // created and destroyed between the Save/Restore calls:
  //
  //  State s = stream.Save();
  //  {
  //    CSSParserTokenStream::Boundary boundary(...);
  //    ConsumeSomething(stream);
  //  }
  //  stream.Restore(s);
  //
  // Or you can use a boundary that exists during both Save and Restore calls:
  //
  //  CSSParserTokenStream::Boundary boundary(...);
  //  State s = stream.Save();
  //  ConsumeSomething(stream);
  //  stream.Restore(s);
  //
  // However, a Save/Restore pair must not cross the boundary. The following
  // will trigger a DCHECK:
  //
  //  State s = stream.Save();
  //  ConsumeSomething(stream);
  //  {
  //    CSSParserTokenStream::Boundary boundary(...);
  //    stream.Restore(s);
  //  }

#if DCHECK_IS_ON()
  struct State {
    STACK_ALLOCATED();

   private:
    friend class CSSParserTokenStream;
    State(wtf_size_t offset, uint64_t boundaries)
        : offset_(offset), boundaries_(boundaries) {}
    wtf_size_t offset_;
    uint64_t boundaries_;
  };
#else   // !DCHECK_IS_ON()
  using State = wtf_size_t;
#endif  // DCHECK_IS_ON()

  State Save() const {
    DCHECK(has_look_ahead_);
#if DCHECK_IS_ON()
    return State(offset_, boundaries_);
#else   // !DCHECK_IS_ON()
    return offset_;
#endif  // DCHECK_IS_ON()
  }

  void Restore(State state) {
    DCHECK(has_look_ahead_);
#if DCHECK_IS_ON()
    offset_ = state.offset_;
    DCHECK_EQ(state.boundaries_, boundaries_) << "Boundary-crossing restore";
#else   // !DCHECK_IS_ON()
    offset_ = state;
#endif  // DCHECK_IS_ON()
    next_ = tokenizer_.Restore(next_, offset_);
  }

  // A RestoringBlockGuard is an object that allows you to enter a block,
  // and guarantees that (once destroyed) the stream is not left in the middle
  // of that block. This guarantee is met one of two ways:
  //
  //  1. The guard is *released*, and no action is taken when it goes
  //     out of scope, except consuming the block-end. Releasing a guard
  //     is only possible at the block-end (or EOF).
  //  2. The guard is not released, and the stream is restored to the
  //     specified state when the guard goes out of scope. This is the default
  //     behavior.
  //
  // This is useful in situations where you need to speculatively enter a block,
  // but then expect to "abort" parsing depending on the first few tokens within
  // that block.
  //
  // Note that the provided state must not cross another [Restoring]BlockGuard.
  class RestoringBlockGuard {
    STACK_ALLOCATED();

   public:
    RestoringBlockGuard(CSSParserTokenStream& stream, State state)
        : stream_(stream), boundaries_(stream.boundaries_), state_(state) {
      const CSSParserToken next = stream.ConsumeInternal();
      DCHECK_EQ(next.GetBlockType(), CSSParserToken::kBlockStart);
      stream.boundaries_ = FlagForTokenType(kEOFToken);
    }

    // Attempts to release the guard. If the guard could not be released
    // (i.e. we are not at the end of the block or EOF), then this call
    // has no effect, and ~RestoringBlockGuard will restore the stream to
    // the pre-guard state.
    //
    // The return value of this function is useful for checking whether or
    // not we are at the end of the block. If we expect to be at the end
    // of a block, we can try to Release the guard. If that succeeded
    // we were indeed at the end, otherwise we had unexpected trailing
    // tokens (a parse failure of whatever we're trying to parse).
    //
    // Note that the following examples ignore whitespace tokens.
    //
    //  Example 1: rgb(0, 128, 64)
    //                           ^
    //  After consuming '64' from this stream, we try to Release the guard.
    //  Since we're at the end of the block, the guard is released.
    //
    //  Example 2: rgb(0, 128, 64 nonsense)
    //                            ^
    //  After consuming '64' from this stream, we try to Release the guard.
    //  Since we're not at the end of the block, the guard isn't released,
    //  which means that we have unknown trailing tokens.
    bool Release() {
      stream_.EnsureLookAhead();
      if (stream_.next_.IsEOF() ||
          stream_.next_.GetBlockType() == CSSParserToken::kBlockEnd) {
        released_ = true;
        return true;
      }
      return false;
    }

    ~RestoringBlockGuard() {
      stream_.EnsureLookAhead();
      if (released_) {
        // The guard has been released, nothing to do except move past
        // the block-end.
        const CSSParserToken& token = stream_.UncheckedConsumeInternal();
        DCHECK(token.GetType() == kEOFToken ||
               token.GetBlockType() == CSSParserToken::kBlockEnd);
      } else {
        // The guard has not been released, and we need to restore to the
        // pre-guard state.
        stream_.Restore(state_);
        // Pops the item pushed by the call to UncheckedConsumeInternal.
        // Note that if we happen to be at the end of the block, then we already
        // popped the block stack, but Restore would have pushed to the stack
        // again.
        stream_.PopBlockStack();
      }
      stream_.boundaries_ = boundaries_;
    }

   private:
    CSSParserTokenStream& stream_;
    uint64_t boundaries_;
    State state_;
    bool released_ = false;
  };

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

  void PopBlockStack() { tokenizer_.block_stack_.pop_back(); }

  Vector<CSSParserToken, kInitialBufferSize> buffer_;
  CSSTokenizer& tokenizer_;
  CSSParserToken next_;
  wtf_size_t offset_ = 0;
  bool has_look_ahead_ = false;
  uint64_t boundaries_ = FlagForTokenType(kEOFToken);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_TOKEN_STREAM_H_
