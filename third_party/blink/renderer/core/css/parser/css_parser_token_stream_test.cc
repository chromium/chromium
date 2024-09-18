// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_save_point.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Avoids the DCHECK that we've Peek()-ed before.
const CSSParserToken& ConsumeInTest(CSSParserTokenStream& stream) {
  stream.Peek();
  return stream.Consume();
}

CSSParserToken ConsumeIncludingWhitespaceInTest(CSSParserTokenStream& stream) {
  stream.Peek();
  return stream.ConsumeIncludingWhitespace();
}

String GetUntilEndOfBlock(CSSParserTokenStream& stream) {
  StringBuilder sb;
  while (!stream.AtEnd()) {
    ConsumeInTest(stream).Serialize(sb);
  }
  return sb.ReleaseString();
}

String SerializeTokens(const Vector<CSSParserToken, 32>& tokens) {
  StringBuilder sb;
  for (const CSSParserToken& token : tokens) {
    token.Serialize(sb);
  }
  return sb.ReleaseString();
}

TEST(CSSParserTokenStreamTest, EmptyStream) {
  CSSParserTokenStream stream("");
  EXPECT_TRUE(ConsumeInTest(stream).IsEOF());
  EXPECT_TRUE(stream.Peek().IsEOF());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, PeekThenConsume) {
  CSSParserTokenStream stream("A");  // kIdent
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeThenPeek) {
  CSSParserTokenStream stream("A");  // kIdent
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeMultipleTokens) {
  CSSParserTokenStream stream("A 1");  // kIdent kWhitespace kNumber
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
  EXPECT_EQ(kWhitespaceToken, ConsumeInTest(stream).GetType());
  EXPECT_EQ(kNumberToken, ConsumeInTest(stream).GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterPeek) {
  CSSParserTokenStream stream("A");  // kIdent
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterAtEnd) {
  CSSParserTokenStream stream("A");  // kIdent
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeWhitespace) {
  CSSParserTokenStream stream(" \t\n");  // kWhitespace

  EXPECT_EQ(kWhitespaceToken, ConsumeInTest(stream).GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeIncludingWhitespace) {
  CSSParserTokenStream stream("A \t\n");  // kIdent kWhitespace

  EXPECT_EQ(kIdentToken, ConsumeIncludingWhitespaceInTest(stream).GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, BlockErrorRecoveryConsumesRestOfBlock) {
  CSSParserTokenStream stream("{B }1");

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
    EXPECT_FALSE(stream.AtEnd());
  }  // calls destructor

  EXPECT_EQ(kNumberToken, ConsumeInTest(stream).GetType());
}

TEST(CSSParserTokenStreamTest, BlockErrorRecoveryOnSuccess) {
  CSSParserTokenStream stream("{B }1");

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
    EXPECT_EQ(kWhitespaceToken, ConsumeInTest(stream).GetType());
    EXPECT_TRUE(stream.AtEnd());
  }  // calls destructor

  EXPECT_EQ(kNumberToken, ConsumeInTest(stream).GetType());
}

TEST(CSSParserTokenStreamTest, OffsetAfterPeek) {
  CSSParserTokenStream stream("ABC");

  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(0U, stream.Offset());
}

TEST(CSSParserTokenStreamTest, OffsetAfterConsumes) {
  CSSParserTokenStream stream("ABC 1 {23 }");

  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());
  EXPECT_EQ(3U, stream.Offset());
  EXPECT_EQ(kWhitespaceToken, ConsumeInTest(stream).GetType());
  EXPECT_EQ(4U, stream.Offset());
  EXPECT_EQ(kNumberToken, ConsumeIncludingWhitespaceInTest(stream).GetType());
  EXPECT_EQ(6U, stream.Offset());
}

TEST(CSSParserTokenStreamTest, LookAheadOffset) {
  CSSParserTokenStream stream("ABC/* *//* */1");

  stream.EnsureLookAhead();
  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(0U, stream.LookAheadOffset());
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());

  stream.EnsureLookAhead();
  EXPECT_EQ(3U, stream.Offset());
  EXPECT_EQ(13U, stream.LookAheadOffset());
}

TEST(CSSParserTokenStreamTest, SkipUntilPeekedTypeOffset) {
  CSSParserTokenStream stream("a b c;d e f");

  // a
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(0u, stream.Offset());

  stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
  EXPECT_EQ(kSemicolonToken, stream.Peek().GetType());
  EXPECT_EQ(5u, stream.Offset());

  // Again, when we're already at kSemicolonToken.
  stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
  EXPECT_EQ(kSemicolonToken, stream.Peek().GetType());
  EXPECT_EQ(5u, stream.Offset());
}

TEST(CSSParserTokenStreamTest, SkipUntilPeekedTypeOffsetEndOfFile) {
  CSSParserTokenStream stream("a b c");

  // a
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(0u, stream.Offset());

  stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(5u, stream.Offset());

  // Again, when we're already at EOF.
  stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_EQ(5u, stream.Offset());
}

TEST(CSSParserTokenStreamTest, SkipUntilPeekedTypeOffsetEndOfBlock) {
  CSSParserTokenStream stream("a { a b c } d ;");

  // a
  EXPECT_EQ(0u, stream.Offset());
  EXPECT_EQ(kIdentToken, ConsumeInTest(stream).GetType());

  EXPECT_EQ(1u, stream.Offset());
  EXPECT_EQ(kWhitespaceToken, ConsumeInTest(stream).GetType());

  EXPECT_EQ(kLeftBraceToken, stream.Peek().GetType());
  EXPECT_EQ(2u, stream.Offset());

  {
    CSSParserTokenStream::BlockGuard guard(stream);

    EXPECT_EQ(kWhitespaceToken, stream.Peek().GetType());
    EXPECT_EQ(3u, stream.Offset());

    stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
    EXPECT_TRUE(stream.AtEnd());  // End of block.
    EXPECT_EQ(kRightBraceToken, stream.UncheckedPeek().GetType());
    EXPECT_EQ(10u, stream.Offset());

    // Again, when we're already at the end-of-block.
    stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
    EXPECT_TRUE(stream.AtEnd());  // End of block.
    EXPECT_EQ(kRightBraceToken, stream.UncheckedPeek().GetType());
    EXPECT_EQ(10u, stream.Offset());
  }

  EXPECT_EQ(kWhitespaceToken, stream.Peek().GetType());
  EXPECT_EQ(11u, stream.Offset());
}

TEST(CSSParserTokenStreamTest, SkipUntilPeekedTypeIsEmpty) {
  CSSParserTokenStream stream("{23 }");

  stream.SkipUntilPeekedTypeIs<>();
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, Boundary) {
  CSSParserTokenStream stream("foo:red;bar:blue;asdf");

  {
    CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
    stream.SkipUntilPeekedTypeIs<>();
    EXPECT_TRUE(stream.AtEnd());
  }

  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kSemicolonToken, ConsumeInTest(stream).GetType());

  {
    CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
    stream.SkipUntilPeekedTypeIs<>();
    EXPECT_TRUE(stream.AtEnd());
  }

  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kSemicolonToken, ConsumeInTest(stream).GetType());

  EXPECT_EQ("asdf", ConsumeInTest(stream).Value());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, MultipleBoundaries) {
  CSSParserTokenStream stream("a:b,c;d:,;e");

  {
    CSSParserTokenStream::Boundary boundary_semicolon(stream, kSemicolonToken);

    {
      CSSParserTokenStream::Boundary boundary_comma(stream, kCommaToken);

      {
        CSSParserTokenStream::Boundary boundary_colon(stream, kColonToken);
        stream.SkipUntilPeekedTypeIs<>();
        EXPECT_TRUE(stream.AtEnd());
      }

      EXPECT_FALSE(stream.AtEnd());
      EXPECT_EQ(kColonToken, ConsumeInTest(stream).GetType());

      stream.SkipUntilPeekedTypeIs<>();
      EXPECT_TRUE(stream.AtEnd());
    }

    EXPECT_FALSE(stream.AtEnd());
    EXPECT_EQ(kCommaToken, ConsumeInTest(stream).GetType());

    stream.SkipUntilPeekedTypeIs<>();
    EXPECT_TRUE(stream.AtEnd());
  }

  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kSemicolonToken, ConsumeInTest(stream).GetType());

  stream.SkipUntilPeekedTypeIs<>();
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, IneffectiveBoundary) {
  CSSParserTokenStream stream("a:b|");

  {
    CSSParserTokenStream::Boundary boundary_colon(stream, kColonToken);

    {
      // It's valid to add another boundary, but it has no affect in this
      // case, since kColonToken appears first.
      CSSParserTokenStream::Boundary boundary_semicolon(stream,
                                                        kSemicolonToken);

      stream.SkipUntilPeekedTypeIs<>();

      EXPECT_EQ(kColonToken, stream.Peek().GetType());
      EXPECT_TRUE(stream.AtEnd());
    }

    EXPECT_TRUE(stream.AtEnd());
  }

  EXPECT_FALSE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, BoundaryBlockGuard) {
  CSSParserTokenStream stream("a[b;c]d;e");

  {
    CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
    EXPECT_EQ("a", ConsumeInTest(stream).Value());

    {
      CSSParserTokenStream::BlockGuard guard(stream);
      // The boundary does not apply within blocks.
      EXPECT_EQ("b;c", GetUntilEndOfBlock(stream));
    }

    // However, now the boundary should apply.
    EXPECT_EQ("d", GetUntilEndOfBlock(stream));
  }
}

TEST(CSSParserTokenStreamTest, BoundaryRestoringBlockGuard) {
  CSSParserTokenStream stream("a[b;c]d;e");

  {
    CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
    EXPECT_EQ("a", ConsumeInTest(stream).Value());

    {
      stream.EnsureLookAhead();
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      // The boundary does not apply within blocks.
      EXPECT_EQ("b;c", GetUntilEndOfBlock(stream));
      EXPECT_TRUE(guard.Release());
    }

    // However, now the boundary should apply.
    EXPECT_EQ("d", GetUntilEndOfBlock(stream));
  }
}

TEST(CSSParserTokenStreamTest, SavePointRestoreWithoutLookahead) {
  CSSParserTokenStream stream("a b c");
  stream.EnsureLookAhead();

  {
    CSSParserSavePoint savepoint(stream);
    stream.Peek();
    stream.UncheckedConsume();  // a
    stream.EnsureLookAhead();
    stream.Peek();
    stream.UncheckedConsume();  // whitespace

    EXPECT_FALSE(stream.HasLookAhead());
    // Let `savepoint` go out of scope without being released.
  }

  // We should have restored to the beginning.
  EXPECT_EQ("a", stream.Peek().Value());
}

namespace {

Vector<CSSParserToken, 32> TokenizeAll(String string) {
  CSSTokenizer tokenizer(string);
  Vector<CSSParserToken, 32> tokens;
  while (true) {
    const CSSParserToken token = tokenizer.TokenizeSingle();
    if (token.GetType() == kEOFToken) {
      return tokens;
    } else {
      tokens.push_back(token);
    }
  }
}

// See struct RestartData.
std::pair<wtf_size_t, wtf_size_t> ParseRestart(String restart) {
  wtf_size_t restart_target = restart.find('^');
  wtf_size_t restart_offset = restart.find('<');
  return std::make_pair(restart_target, restart_offset);
}

// Consume all tokens in `stream`, and store them in `tokens`,
// restarting (once) at the token with offset `restart_offset`
// to the offset specified by `restart_target`.
void TokenizeInto(CSSParserTokenStream& stream,
                  wtf_size_t restart_target,
                  wtf_size_t restart_offset,
                  Vector<CSSParserToken, 32>& tokens) {
  std::optional<CSSParserTokenStream::State> saved_state;

  while (true) {
    stream.EnsureLookAhead();

    if (restart_target == stream.Offset()) {
      saved_state = stream.Save();
    }

    if (saved_state.has_value() && restart_offset == stream.Offset()) {
      stream.Restore(saved_state.value());
      saved_state.reset();
      // Do not restart again:
      restart_target = std::numeric_limits<wtf_size_t>::max();
      continue;
    }

    if (stream.AtEnd()) {
      return;
    }

    if (stream.UncheckedPeek().GetBlockType() == CSSParserToken::kBlockStart) {
      // Push block-start token about to be consumed by BlockGuard.
      tokens.push_back(stream.UncheckedPeek());
      CSSParserTokenStream::BlockGuard guard(stream);
      TokenizeInto(stream, restart_target, restart_offset, tokens);
      // Note that stream.AtEnd() is true for EOF, but also for
      // any block-end token.
      stream.EnsureLookAhead();
      DCHECK(stream.AtEnd());
      if (stream.UncheckedPeek().GetType() != kEOFToken) {
        // Add block-end token.
        tokens.push_back(stream.UncheckedPeek());
      }
    } else {
      tokens.push_back(stream.UncheckedConsume());
    }
  }
}

}  // namespace

struct RestartData {
  // The string to tokenize.
  const char* input;
  // Specifies where to restart from and to as follows:
  //
  // '^' - Restart to this offset.
  // '<' - Instead of consuming the token at this offset, restart to the
  //       offset indicated '^' instead.
  //
  // Example:
  //
  //  Input:   "foo bar baz"
  //  Restart: "    ^   <  "
  //
  // The above will consume foo, <space>, <bar>, <space>, then restart
  // at bar.
  //
  // Note that the '<' can appear at an offset equal to the length of the
  // input string, to represent restarts that happen when the stream is
  // at EOF.
  const char* restart;
  // Represents the expected token sequence, including the restart.
  // Continuing the example above, the appropriate 'ref' would be:
  //
  //  "foo bar bar baz"
  const char* ref;
};

RestartData restart_data[] = {
    // clang-format off
    {
      "x y z",
      "^ <  ",
      "x x y z"
    },
    {
      "x y z",
      "  ^ <",
      "x y y z"
    },
    {
      "x y z",
      "   ^<",
      "x y /**/ z"
    },
    {
      "x y z",
      "^<   ",
      "x/**/x y z"
    },
    {
      "x y z",
      "^  <",
      "x y/**/x y z"
    },

    // Restarting on block-start:
    {
      "x y { a b c } z",
      "  ^ <          ",
      "x y y { a b c } z"
    },
    {
      "x y ( a b c ) z",
      "  ^ <          ",
      "x y y ( a b c ) z"
    },
    {
      "x y { a b c } z",
      "  ^ <          ",
      "x y y { a b c } z"
    },
    {
      "x y foo( a b c ) z",
      "  ^ <          ",
      "x y y foo( a b c ) z"
    },

    // Restarting over a block:
    {
      "x y { a b c } z w",
      "  ^           <  ",
      "x y { a b c } y { a b c } z w"
    },
    {
      "x y { a b c } z w",
      "  ^          <   ",
      "x y { a b c }y { a b c } z w"
    },
    // Restart to block-start:
    {
      "x y { a b c } z w",
      "    ^         <   ",
      "x y { a b c } { a b c } z w"
    },

    // Restarting over an EOF-terminated block
    {
      "x y { a b c ",
      "  ^         <",
      "x y { a b c y { a b c "
    },

    // Restart within block:
    {
      "x y { a b c } z",
      "      ^   <    ",
      "x y { a b a b c } z"
    },
    {
      "x y { a b c } z",
      "     ^     <   ",
      "x y { a b c a b c } z"
    },
    {
      "x y { a b c } z",
      "     ^      <  ",
      "x y { a b c /**/ a b c } z"
    },
    // Restart within EOF-terminated block.
    {
      "x y {([ a b c d",
      "        ^   <  ",
      "x y {([ a b a b c d"
    },
    {
      "x y {([ a b c d",
      "        ^     <",
      "x y {([ a b c a b c d"
    },

    // clang-format on
};

class RestartTest : public testing::Test,
                    public testing::WithParamInterface<RestartData> {};

INSTANTIATE_TEST_SUITE_P(CSSParserTokenStreamTest,
                         RestartTest,
                         testing::ValuesIn(restart_data));

TEST_P(RestartTest, All) {
  RestartData param = GetParam();

  String ref(param.ref);
  Vector<CSSParserToken, 32> ref_tokens = TokenizeAll(ref);

  String input(param.input);
  CSSParserTokenStream stream(input);

  auto [restart_target, restart_offset] = ParseRestart(param.restart);
  Vector<CSSParserToken, 32> actual_tokens;
  TokenizeInto(stream, restart_target, restart_offset, actual_tokens);

  SCOPED_TRACE(testing::Message()
               << "Expected (serialized): " << SerializeTokens(ref_tokens));
  SCOPED_TRACE(testing::Message()
               << "Actual (serialized): " << SerializeTokens(actual_tokens));

  SCOPED_TRACE(param.ref);
  SCOPED_TRACE(param.restart);
  SCOPED_TRACE(param.input);

  EXPECT_EQ(actual_tokens, ref_tokens);
}

// Same as RestartTest, except performs all restarts during a boundary.
class BoundaryRestartTest : public testing::Test,
                            public testing::WithParamInterface<RestartData> {};

INSTANTIATE_TEST_SUITE_P(CSSParserTokenStreamTest,
                         BoundaryRestartTest,
                         testing::ValuesIn(restart_data));

TEST_P(BoundaryRestartTest, All) {
  RestartData param = GetParam();

  String ref(param.ref);
  Vector<CSSParserToken, 32> ref_tokens = TokenizeAll(ref);

  String input(param.input);
  CSSParserTokenStream stream(input);

  CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);

  auto [restart_target, restart_offset] = ParseRestart(param.restart);
  Vector<CSSParserToken, 32> actual_tokens;
  TokenizeInto(stream, restart_target, restart_offset, actual_tokens);

  SCOPED_TRACE(testing::Message()
               << "Expected (serialized): " << SerializeTokens(ref_tokens));
  SCOPED_TRACE(testing::Message()
               << "Actual (serialized): " << SerializeTokens(actual_tokens));

  SCOPED_TRACE(param.ref);
  SCOPED_TRACE(param.restart);
  SCOPED_TRACE(param.input);

  EXPECT_EQ(actual_tokens, ref_tokens);
}

class NullRestartTest : public testing::Test,
                        public testing::WithParamInterface<RestartData> {};

INSTANTIATE_TEST_SUITE_P(CSSParserTokenStreamTest,
                         NullRestartTest,
                         testing::ValuesIn(restart_data));

// Ignores RestartData.restart, and instead tests restarting to and from
// the same offset, i.e. "restarting" to the offset we're already on.
TEST_P(NullRestartTest, All) {
  RestartData param = GetParam();

  String input(param.input);
  Vector<CSSParserToken, 32> ref_tokens = TokenizeAll(input);

  for (wtf_size_t restart_offset = 0; restart_offset <= input.length();
       ++restart_offset) {
    CSSParserTokenStream stream(input);

    Vector<CSSParserToken, 32> actual_tokens;
    TokenizeInto(stream, /* restart_target */ restart_offset, restart_offset,
                 actual_tokens);

    SCOPED_TRACE(testing::Message()
                 << "Expected (serialized): " << SerializeTokens(ref_tokens));
    SCOPED_TRACE(testing::Message()
                 << "Actual (serialized): " << SerializeTokens(actual_tokens));

    SCOPED_TRACE(param.input);
    SCOPED_TRACE(testing::Message() << "restart_offset:" << restart_offset);

    EXPECT_EQ(actual_tokens, ref_tokens);
  }
}

class TestStream {
  STACK_ALLOCATED();

 public:
  explicit TestStream(String input) : input_(input), stream_(input) {
    stream_.EnsureLookAhead();
  }

  void EnsureLookahead() { stream_.EnsureLookAhead(); }

  const CSSParserToken& Peek() { return stream_.Peek(); }

  bool AtEnd() { return stream_.AtEnd(); }

  bool ConsumeTokens(String expected) {
    CSSTokenizer tokenizer(expected);
    while (true) {
      CSSParserToken expected_token = tokenizer.TokenizeSingle();
      if (expected_token.GetType() == kEOFToken) {
        break;
      }
      if (stream_.Peek() != expected_token) {
        return false;
      }
      stream_.Consume();
    }
    return true;
  }

  CSSParserTokenStream::State Save() {
    stream_.EnsureLookAhead();
    return stream_.Save();
  }

 private:
  friend class TestRestoringBlockGuard;
  friend class TestBlockGuard;
  friend class TestBoundary;
  String input_;
  CSSParserTokenStream stream_;
};

// The following various Test* classes only exist to accept
// a TestStream instead of a CSSParserTokenStream.

class TestRestoringBlockGuard {
  STACK_ALLOCATED();

 public:
  explicit TestRestoringBlockGuard(TestStream& stream)
      : guard_(stream.stream_) {}
  bool Release() { return guard_.Release(); }

 private:
  CSSParserTokenStream::RestoringBlockGuard guard_;
};

class TestBlockGuard {
  STACK_ALLOCATED();

 public:
  explicit TestBlockGuard(TestStream& stream) : guard_(stream.stream_) {}

 private:
  CSSParserTokenStream::BlockGuard guard_;
};

class TestBoundary {
  STACK_ALLOCATED();

 public:
  explicit TestBoundary(TestStream& stream, CSSParserTokenType boundary_type)
      : boundary_(stream.stream_, boundary_type) {}

 private:
  CSSParserTokenStream::Boundary boundary_;
};

class RestoringBlockGuardTest : public testing::Test {};

TEST_F(RestoringBlockGuardTest, Restore) {
  TestStream stream("a b c (d e f) g h i");
  EXPECT_TRUE(stream.ConsumeTokens("a b c "));

  // Restore immediately after guard.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Restore after consuming one token.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_TRUE(stream.ConsumeTokens("d"));
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Restore in the middle.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_TRUE(stream.ConsumeTokens("d e"));
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Restore with one token left.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_TRUE(stream.ConsumeTokens("d e "));
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Restore at the end (of the block).
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_TRUE(stream.ConsumeTokens("d e f"));
    EXPECT_TRUE(stream.AtEnd());
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());
}

TEST_F(RestoringBlockGuardTest, NestedRestore) {
  TestStream stream("a b [c (d e f) g] h i");
  EXPECT_TRUE(stream.ConsumeTokens("a b "));

  // Restore immediately after inner guard.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard outer_guard(stream);  // [
    EXPECT_TRUE(stream.ConsumeTokens("c "));
    {
      stream.EnsureLookahead();
      TestRestoringBlockGuard inner_guard(stream);  // (
    }
    EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());
  }
  EXPECT_EQ(kLeftBracketToken, stream.Peek().GetType());

  // Restore in the middle of inner block.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard outer_guard(stream);  // [
    EXPECT_TRUE(stream.ConsumeTokens("c "));
    {
      stream.EnsureLookahead();
      TestRestoringBlockGuard inner_guard(stream);  // (
      EXPECT_TRUE(stream.ConsumeTokens("d "));
    }
    EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());
  }
  EXPECT_EQ(kLeftBracketToken, stream.Peek().GetType());

  // Restore at the end of inner block.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard outer_guard(stream);  // [
    EXPECT_TRUE(stream.ConsumeTokens("c "));
    {
      stream.EnsureLookahead();
      TestRestoringBlockGuard inner_guard(stream);  // (
      EXPECT_TRUE(stream.ConsumeTokens("d e f"));
    }
    EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());
  }
  EXPECT_EQ(kLeftBracketToken, stream.Peek().GetType());
}

TEST_F(RestoringBlockGuardTest, Release) {
  TestStream stream("a b c (d e f) g h i");
  EXPECT_TRUE(stream.ConsumeTokens("a b c "));

  // Cannot release unless we're AtEnd.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens("d");
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens(" e ");
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens("f");
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Same again, except this time with a Release after consuming 'f'.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens("d");
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens(" e ");
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens("f");
    EXPECT_TRUE(guard.Release());
  }
  EXPECT_TRUE(stream.ConsumeTokens(" g h i"));
}

TEST_F(RestoringBlockGuardTest, ReleaseEOF) {
  TestStream stream("a b c (d e f");
  EXPECT_TRUE(stream.ConsumeTokens("a b c "));

  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    EXPECT_FALSE(guard.Release());
    stream.ConsumeTokens("d e f");
    EXPECT_TRUE(guard.Release());
  }

  EXPECT_TRUE(stream.Peek().IsEOF());
}

TEST_F(RestoringBlockGuardTest, NestedRelease) {
  TestStream stream("a b [c (d e f) g] h i");
  EXPECT_TRUE(stream.ConsumeTokens("a b "));

  // Inner guard released, but outer guard is not.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard outer_guard(stream);  // [
    EXPECT_TRUE(stream.ConsumeTokens("c "));
    EXPECT_FALSE(outer_guard.Release());
    {
      stream.EnsureLookahead();
      TestRestoringBlockGuard inner_guard(stream);  // (
      EXPECT_FALSE(inner_guard.Release());
      EXPECT_TRUE(stream.ConsumeTokens("d e f"));
      EXPECT_TRUE(inner_guard.Release());
    }
    EXPECT_TRUE(stream.ConsumeTokens(" g"));
  }
  EXPECT_EQ(kLeftBracketToken, stream.Peek().GetType());

  // Both guards released.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard outer_guard(stream);  // [
    EXPECT_TRUE(stream.ConsumeTokens("c "));
    EXPECT_FALSE(outer_guard.Release());
    {
      stream.EnsureLookahead();
      TestRestoringBlockGuard inner_guard(stream);  // (
      EXPECT_FALSE(inner_guard.Release());
      EXPECT_TRUE(stream.ConsumeTokens("d e f"));
      EXPECT_TRUE(inner_guard.Release());
    }
    EXPECT_FALSE(outer_guard.Release());
    EXPECT_TRUE(stream.ConsumeTokens(" g"));
    EXPECT_TRUE(outer_guard.Release());
  }
  EXPECT_TRUE(stream.ConsumeTokens(" h i"));
}

TEST_F(RestoringBlockGuardTest, ReleaseImmediate) {
  TestStream stream("a b (c d) e");
  EXPECT_TRUE(stream.ConsumeTokens("a b "));

  stream.EnsureLookahead();
  TestRestoringBlockGuard guard(stream);
  EXPECT_FALSE(guard.Release());
  EXPECT_TRUE(stream.ConsumeTokens("c d"));
  EXPECT_TRUE(guard.Release());
  // The above Release() call should consume the block-end,
  // even if RestoringBlockGuard hasn't gone out of scope.

  EXPECT_EQ(kWhitespaceToken, stream.Peek().GetType());
  EXPECT_TRUE(stream.ConsumeTokens(" e"));
  EXPECT_TRUE(stream.Peek().IsEOF());
}

TEST_F(RestoringBlockGuardTest, BlockStack) {
  TestStream stream("a (b c) d) e");
  EXPECT_TRUE(stream.ConsumeTokens("a "));

  // Start consuming the block, but abort (restart).
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);  // (
  }
  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());

  // Now fully consume the block.
  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);  // (
    EXPECT_TRUE(stream.ConsumeTokens("b c"));
    EXPECT_TRUE(guard.Release());
  }
  EXPECT_TRUE(stream.ConsumeTokens(" d"));

  // The restart should have adjusted the block stack, otherwise
  // the final ")" will incorrectly appear as kBlockEnd.
  EXPECT_EQ(stream.Peek().GetType(), kRightParenthesisToken);
  EXPECT_EQ(stream.Peek().GetBlockType(), CSSParserToken::kNotBlock);
}

TEST_F(RestoringBlockGuardTest, RestoreDuringBoundary) {
  TestStream stream("a (b c ; d e) f; g h");
  EXPECT_TRUE(stream.ConsumeTokens("a "));

  TestBoundary boundary(stream, kSemicolonToken);

  {
    stream.EnsureLookahead();
    TestRestoringBlockGuard guard(stream);
    // The outer boundary should not apply here, hence we should be able
    // to consume the inner kSemicolonToken.
    EXPECT_TRUE(stream.ConsumeTokens("b c ; d"));

    // We didn't consume everything in the block, so we should restore
    // `guard` goes out of scope.
  }

  EXPECT_EQ(kLeftParenthesisToken, stream.Peek().GetType());
  // Skip past the block.
  { TestBlockGuard block_guard(stream); }
  EXPECT_TRUE(stream.ConsumeTokens(" f"));

  // We're at the outer kSemicolonToken, which is considered to be AtEnd
  // due to the boundary.
  EXPECT_TRUE(stream.AtEnd());
}

}  // namespace

}  // namespace blink
