// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"

namespace blink {

using Result = CSSSupportsParser::Result;

class CSSSupportsParserTest : public testing::Test {
 public:
  CSSParserContext* MakeContext() {
    return MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
  }

  Vector<CSSParserToken, 32> Tokenize(const String& string) {
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

  Result StaticConsumeSupportsCondition(String string) {
    CSSParserImpl impl(MakeContext());
    CSSParserTokenStream stream(string);
    Result result = CSSSupportsParser::ConsumeSupportsCondition(stream, impl);
    return stream.AtEnd() ? result : Result::kParseFailure;
  }

  Result AtSupports(String string) {
    return StaticConsumeSupportsCondition(string);
  }

  Result WindowCSSSupports(String string) {
    String wrapped_condition = "(" + string + ")";
    return StaticConsumeSupportsCondition(wrapped_condition);
  }

  Result ConsumeSupportsCondition(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeSupportsCondition(stream);
  }

  Result ConsumeSupportsInParens(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeSupportsInParens(stream);
  }

  bool ConsumeSupportsFeature(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeSupportsFeature(stream);
  }

  bool ConsumeSupportsSelectorFn(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeSupportsSelectorFn(stream);
  }

  bool ConsumeSupportsDecl(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeSupportsDecl(stream);
  }

  bool ConsumeGeneralEnclosed(String string) {
    CSSParserImpl impl(MakeContext());
    CSSSupportsParser parser(impl);
    CSSParserTokenStream stream(string);
    return parser.ConsumeGeneralEnclosed(stream);
  }
};

TEST_F(CSSSupportsParserTest, ResultNot) {
  EXPECT_EQ(Result::kSupported, !Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, !Result::kSupported);
  EXPECT_EQ(Result::kParseFailure, !Result::kParseFailure);
}

TEST_F(CSSSupportsParserTest, ResultAnd) {
  EXPECT_EQ(Result::kSupported, Result::kSupported & Result::kSupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported & Result::kSupported);
  EXPECT_EQ(Result::kUnsupported, Result::kSupported & Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported & Result::kUnsupported);

  EXPECT_EQ(Result::kParseFailure, Result::kSupported & Result::kParseFailure);
  EXPECT_EQ(Result::kParseFailure, Result::kParseFailure & Result::kSupported);
}

TEST_F(CSSSupportsParserTest, ResultOr) {
  EXPECT_EQ(Result::kSupported, Result::kSupported | Result::kSupported);
  EXPECT_EQ(Result::kSupported, Result::kUnsupported | Result::kSupported);
  EXPECT_EQ(Result::kSupported, Result::kSupported | Result::kUnsupported);
  EXPECT_EQ(Result::kUnsupported, Result::kUnsupported | Result::kUnsupported);

  EXPECT_EQ(Result::kParseFailure, Result::kSupported | Result::kParseFailure);
  EXPECT_EQ(Result::kParseFailure, Result::kParseFailure | Result::kSupported);
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsCondition) {
  // not <supports-in-parens>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsCondition("not (asdf:red)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(not (color:red))"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsCondition("nay (color:red)"));

  // <supports-in-parens> [ and <supports-in-parens> ]*
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:red) and (color:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(color:red) and (asdf:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(asdf:red) and (asdf:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition(
                "(color:red) and (color:green) and (asdf:color)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition(
                "(color:red) and (color:green) and (not (asdf:color))"));

  // <supports-in-parens> [ or <supports-in-parens> ]*
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition("(color:asdf) or (color:green)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition("(asdf:red) or (asdf:green)"));
  EXPECT_EQ(
      Result::kSupported,
      ConsumeSupportsCondition("(color:red) or (color:green) or (asdf:color)"));
  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsCondition(
                "(color:asdf1) or (color:asdf2) or (asdf:asdf2)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsCondition(
                "(color:asdf) or (color:ghjk) or (not (asdf:color))"));

  // <supports-feature>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsCondition("(color:red)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsCondition("(color:asdf)"));

  // <general-enclosed>
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsCondition("asdf(1)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsCondition("asdf()"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsInParens) {
  // ( <supports-condition> )
  EXPECT_EQ(Result::kSupported, ConsumeSupportsInParens("(not (asdf:red))"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("(not (color:red))"));
  EXPECT_EQ(Result::kParseFailure,
            ConsumeSupportsInParens("(not (color:red)])"));

  EXPECT_EQ(Result::kUnsupported,
            ConsumeSupportsInParens("(not ( (color:gjhk) or (color:red) ))"));
  EXPECT_EQ(
      Result::kUnsupported,
      ConsumeSupportsInParens("(not ( ((color:gjhk)) or (color:blue) ))"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("(( (color:gjhk) or (color:red) ))"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("(( ((color:gjhk)) or (color:blue) ))"));

  // <supports-feature>
  EXPECT_EQ(Result::kSupported, ConsumeSupportsInParens("(color:red)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("(color:asdf)"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("(color]asdf)"));

  // <general-enclosed>
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("asdf(1)"));
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("asdf()"));

  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("(color:red)and (color:green)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("(color:red)or (color:green)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("selector(div)or (color:green)"));
  EXPECT_EQ(Result::kSupported,
            ConsumeSupportsInParens("selector(div)and (color:green)"));

  // Invalid <supports-selector-fn> formerly handled by
  // ConsumeSupportsSelectorFn()
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("#test"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("test"));

  // Invalid <supports-selector-fn> but valid <general-enclosed>
  EXPECT_EQ(Result::kUnsupported, ConsumeSupportsInParens("test(1)"));

  // Invalid <supports-decl> formerly handled by ConsumeSupportsDecl()
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens(""));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens(")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("color:red)"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("color:red"));

  // Invalid <general-enclosed> formerly handled by ConsumeGeneralEnclosed()
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens(""));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens(")"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("color:red"));
  EXPECT_EQ(Result::kParseFailure, ConsumeSupportsInParens("asdf"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsSelectorFn) {
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(*)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(*:hover)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(:hover)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(::before)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(div)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(div"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(.a)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(#a)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(div.a)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(div a)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a > div)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a ~ div)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a + div)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(*|a)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a + div#test)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a + div#test::before)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a.cls:hover)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(a.cls::before)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(div::-webkit-clear-button)"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(:is(.a))"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(:where(.a))"));
  EXPECT_TRUE(ConsumeSupportsSelectorFn("selector(:has(.a))"));

  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div::-webkit-asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(a + div::-webkit-asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div.cls::-webkit-asdf)"));

  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div.~cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div. ~cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div .~ cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div$ cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div $cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(div $ cls)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(unknown|a)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(a::asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(a:asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(a, body)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(*:asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(*::asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(::asdf)"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:is())"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:where())"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:not())"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:is(:foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:is(:has(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:where(:foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:where(:has(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(:foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(:is(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, :is(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, .b, :is(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:is(.a, :foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:where(.a, :foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, :foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, .b, :foo))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(:has(.a)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(:is(:has(.a))))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(:is(:has(.a), .b)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, :has(.b)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:has(.a, .b, :has(.c)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:host(:is(:foo)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(:host(:has(.a)))"));
  EXPECT_FALSE(ConsumeSupportsSelectorFn("selector(::part(foo):has(.a)))"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsDecl) {
  EXPECT_TRUE(ConsumeSupportsDecl("(color:red)"));
  EXPECT_TRUE(ConsumeSupportsDecl("(color:    red)"));
  EXPECT_TRUE(ConsumeSupportsDecl("(color   : red)"));
  EXPECT_TRUE(ConsumeSupportsDecl("(color   :red)"));
  EXPECT_TRUE(ConsumeSupportsDecl("( color:red )"));
  EXPECT_TRUE(ConsumeSupportsDecl("(--x:red)"));
  EXPECT_TRUE(ConsumeSupportsDecl("(--x:\tred) "));
  EXPECT_TRUE(ConsumeSupportsDecl("(--x:\tred) \t "));
  EXPECT_TRUE(ConsumeSupportsDecl("(color:green !important)"));
  // For some reason EOF is allowed in place of ')' (everywhere in Blink).
  // Seems to be the case in Firefox too.
  EXPECT_TRUE(ConsumeSupportsDecl("(color:red"));

  EXPECT_FALSE(ConsumeSupportsDecl("(color:asdf)"));
  EXPECT_FALSE(ConsumeSupportsDecl("(asdf)"));
  EXPECT_FALSE(ConsumeSupportsDecl("(color)"));
  EXPECT_FALSE(ConsumeSupportsDecl("(color:)"));

  EXPECT_FALSE(ConsumeSupportsDecl("("));
  EXPECT_FALSE(ConsumeSupportsDecl("()"));
}

TEST_F(CSSSupportsParserTest, ConsumeSupportsFeature) {
  EXPECT_TRUE(ConsumeSupportsFeature("(color:red)"));
  EXPECT_FALSE(ConsumeSupportsFeature("asdf(1)"));
}

TEST_F(CSSSupportsParserTest, ConsumeGeneralEnclosed) {
  EXPECT_TRUE(ConsumeGeneralEnclosed("(asdf)"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("( asdf )"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("(3)"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("max(1, 2)"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("asdf(1, 2)"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("asdf(1, 2)\t"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("("));
  EXPECT_TRUE(ConsumeGeneralEnclosed("()"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("( )"));

  // Invalid <any-value>:
  EXPECT_FALSE(ConsumeGeneralEnclosed("(asdf})"));
  EXPECT_FALSE(ConsumeGeneralEnclosed("(asd]f)"));
  EXPECT_FALSE(ConsumeGeneralEnclosed("(\"as\ndf\")"));
  EXPECT_FALSE(ConsumeGeneralEnclosed("(url(as'df))"));

  // Valid <any-value>
  EXPECT_TRUE(ConsumeGeneralEnclosed("(as;df)"));
  EXPECT_TRUE(ConsumeGeneralEnclosed("(as ! df)"));
}

TEST_F(CSSSupportsParserTest, AtSupportsCondition) {
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red)"));
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red) and (color:green)"));
  EXPECT_EQ(Result::kSupported, AtSupports("(--x:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            AtSupports("not ((color:gjhk) or (color:asdf))"));
  EXPECT_EQ(Result::kSupported,
            AtSupports("(display: none) and ( (display: none) )"));

  EXPECT_EQ(Result::kUnsupported, AtSupports("(color:ghjk) or (color:asdf)"));
  EXPECT_EQ(Result::kUnsupported, AtSupports("(color:ghjk) or asdf(1)"));
  EXPECT_EQ(Result::kParseFailure, AtSupports("color:red"));
  EXPECT_EQ(
      Result::kParseFailure,
      AtSupports("(display: none) and (display: block) or (display: inline)"));
  EXPECT_EQ(Result::kParseFailure,
            AtSupports("not (display: deadbeef) and (display: block)"));
  EXPECT_EQ(Result::kParseFailure,
            AtSupports("(margin: 0) and (display: inline) or (width:1em)"));

  // "and("/"or(" are function tokens, hence not allowed here.
  EXPECT_EQ(Result::kParseFailure, AtSupports("(left:0) and(top:0)"));
  EXPECT_EQ(Result::kParseFailure, AtSupports("(left:0) or(top:0)"));
}

TEST_F(CSSSupportsParserTest, WindowCSSSupportsCondition) {
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("(--x:red)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("( --x:red )"));
  EXPECT_EQ(Result::kSupported,
            WindowCSSSupports("(--x:red) and (color:green)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("(--x:red) or (color:asdf)"));
  EXPECT_EQ(Result::kSupported,
            WindowCSSSupports("not ((color:gjhk) or (color:asdf))"));

  EXPECT_EQ(Result::kUnsupported,
            WindowCSSSupports("(color:ghjk) or (color:asdf)"));
  EXPECT_EQ(Result::kUnsupported, WindowCSSSupports("(color:ghjk) or asdf(1)"));
  EXPECT_EQ(Result::kSupported, WindowCSSSupports("color:red"));
}

}  // namespace blink
