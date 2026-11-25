// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// A SegmentedString at end-of-file, like HTMLInputStream::MarkEndOfFile().
SegmentedString ClosedInput(const String& data) {
  SegmentedString input(data);
  input.Append(SegmentedString(String(base::span_from_ref(kEndOfFileMarker))));
  input.Close();
  return input;
}

}  // namespace

// This is a regression test for crbug.com/619141
TEST(HTMLTokenizerTest, ZeroOffsetAttributeNameRange) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<script ");
  EXPECT_EQ(nullptr, tokenizer->NextToken(input));

  SegmentedString input2("type='javascript'");
  // Below should not fail ASSERT
  EXPECT_EQ(nullptr, tokenizer->NextToken(input2));
}

TEST(HTMLTokenizerTest, ProcessingInstruction) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input("<?target data?>");
  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kProcessingInstruction, token->GetType());
  EXPECT_EQ("target", token->GetProcessingInstructionTarget().AsString());
  EXPECT_EQ("data", String(token->Data().AsString()));
}

// Regression test for crbug.com/578804516: tokenizing a long processing
// instruction target should not copy the target buffer on every character.
TEST(HTMLTokenizerTest, ProcessingInstructionLongTarget) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  std::string long_target(100000, 'a');
  SegmentedString input(String("<?" + long_target + " data?>"));
  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kProcessingInstruction, token->GetType());
  EXPECT_EQ(String(long_target),
            token->GetProcessingInstructionTarget().AsString());
  EXPECT_EQ("data", String(token->Data().AsString()));
}

// Regression test for crbug.com/40727112: an incomplete markup declaration at
// end of input becomes a bogus comment instead of being dropped.
TEST(HTMLTokenizerTest, IncompleteDoctypeAtEndOfInput) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest enabled(true);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("<!DOC>");
  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kComment, token->GetType());
  EXPECT_EQ("DOC", String(token->Data().AsString()));
}

// Text after the bogus comment must not be dropped.
TEST(HTMLTokenizerTest, IncompleteDoctypeDoesNotDropTrailingText) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest enabled(true);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("Abc<!d>Hi");

  HTMLToken* token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ("Abc", String(token->Data().AsString()));
  tokenizer->ClearToken();

  token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kComment, token->GetType());
  EXPECT_EQ("d", String(token->Data().AsString()));
  tokenizer->ClearToken();

  token = tokenizer->NextToken(input);
  ASSERT_TRUE(token);
  EXPECT_EQ(HTMLToken::kCharacter, token->GetType());
  EXPECT_EQ("Hi", String(token->Data().AsString()));
}

// Killswitch off: the tokenizer stalls and emits no token, as before.
TEST(HTMLTokenizerTest, IncompleteDoctypeKillswitchPreservesOldBehavior) {
  ScopedHTMLParserTruncatedMarkupDeclarationForTest disabled(false);

  test::TaskEnvironment task_environment;
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);

  SegmentedString input = ClosedInput("<!DOC>");
  EXPECT_EQ(nullptr, tokenizer->NextToken(input));
}

TEST(HTMLTokenizerTest, HasEntity) {
  test::TaskEnvironment task_environment;
  HTMLParserOptions options;

  std::unique_ptr<HTMLTokenizer> tokenizer =
      std::make_unique<HTMLTokenizer>(options);
  SegmentedString input1("a<");
  HTMLToken* token1 = tokenizer->NextToken(input1);
  EXPECT_TRUE(token1);
  if (token1) {
    EXPECT_EQ(HTMLToken::kCharacter, token1->GetType());
    EXPECT_FALSE(token1->HasEntity());
  }

  tokenizer = std::make_unique<HTMLTokenizer>(options);
  SegmentedString input2("&amp;<");
  HTMLToken* token2 = tokenizer->NextToken(input2);
  EXPECT_TRUE(token2);
  if (token2) {
    EXPECT_EQ(HTMLToken::kCharacter, token2->GetType());
    EXPECT_TRUE(token2->HasEntity());
  }

  tokenizer = std::make_unique<HTMLTokenizer>(options);
  SegmentedString input3("&auml;<");
  HTMLToken* token3 = tokenizer->NextToken(input3);
  EXPECT_TRUE(token3);
  if (token3) {
    EXPECT_EQ(HTMLToken::kCharacter, token3->GetType());
    EXPECT_TRUE(token3->HasEntity());
  }
}

namespace {

struct RecordedToken {
  HTMLToken::TokenType type;
  String data;
  bool is_8bit = true;
  bool operator==(const RecordedToken&) const = default;
};

std::vector<RecordedToken> TokenizeAll(const std::vector<String>& chunks) {
  HTMLParserOptions options;
  HTMLTokenizer tokenizer(options);
  SegmentedString input;
  for (const String& chunk : chunks) {
    input.Append(SegmentedString(chunk));
  }
  input.Append(SegmentedString(String(base::span_from_ref(kEndOfFileMarker))));
  input.Close();

  std::vector<RecordedToken> result;
  while (HTMLToken* token = tokenizer.NextToken(input)) {
    const HTMLToken::TokenType type = token->GetType();
    String data;
    bool is_8bit = true;
    if (type == HTMLToken::kCharacter || type == HTMLToken::kComment ||
        type == HTMLToken::kStartTag || type == HTMLToken::kEndTag) {
      data = token->Data().AsString();
      is_8bit = data.Is8Bit();
    }
    if (type == HTMLToken::kStartTag) {
      tokenizer.UpdateStateFor(*token);
    }
    result.push_back({type, data, is_8bit});
    tokenizer.ClearToken();
    if (type == HTMLToken::kEndOfFile) {
      break;
    }
  }
  return result;
}

}  // namespace

TEST(HTMLTokenizerTest, SimdScanScriptDataAndRawtextMatchesBaseline) {
  test::TaskEnvironment task_environment;

  // 8-bit input with long script/style blocks, '<', '\n', '\r\n', '\r', '\0'.
  std::string long_a(200, 'A');
  std::string long_b(200, 'B');
  std::string raw_8bit = "<script>var s='data:image/jpeg;base64," + long_a +
                         "';\r\nif (a < b) { s += '" + long_b + "'; }\r" +
                         std::string(1, '\0') + long_a +
                         "</script><style>.cls{" + long_a + "\n" + long_b +
                         "\r\n" + std::string(1, '\0') + long_a + "}</style>";
  String str_8bit(base::as_byte_span(raw_8bit));
  ASSERT_TRUE(str_8bit.Is8Bit());

  // 16-bit input (contains non-Latin-1 emoji characters).
  std::string raw_16bit =
      "<script>var j='😀';" + long_a + "\r\nvar k='🎉' < '😀';" + long_b +
      "</script><style>/* 😀🎉 */" + long_a + "\n" + long_b + "</style>";
  String str_16bit = String::FromUtf8(base::as_byte_span(raw_16bit));
  ASSERT_FALSE(str_16bit.Is8Bit());

  std::vector<std::vector<String>> cases = {
      {str_8bit},
      {StringView(str_8bit, 0, 100).ToString(),
       StringView(str_8bit, 100).ToString()},
      {str_16bit},
      {StringView(str_16bit, 0, 120).ToString(),
       StringView(str_16bit, 120).ToString()},
  };

  for (const auto& chunks : cases) {
    std::vector<RecordedToken> baseline;
    {
      base::test::ScopedFeatureList disabled;
      disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
      baseline = TokenizeAll(chunks);
    }
    std::vector<RecordedToken> simd;
    {
      base::test::ScopedFeatureList enabled;
      enabled.InitAndEnableFeature(features::kHTMLTokenizerSimdScan);
      simd = TokenizeAll(chunks);
    }
    EXPECT_EQ(baseline, simd);
  }
}

TEST(HTMLTokenizerTest, SimdScanCrlfAcrossChunkBoundary) {
  test::TaskEnvironment task_environment;
  std::string pad(80, 'x');
  String chunk1(base::as_byte_span("<script>" + pad + "\r"));
  String chunk2(
      base::as_byte_span("\n" + pad + "</script><style>" + pad + "\r"));
  String chunk3(base::as_byte_span("\n" + pad + "</style>"));

  std::vector<RecordedToken> baseline;
  {
    base::test::ScopedFeatureList disabled;
    disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
    baseline = TokenizeAll({chunk1, chunk2, chunk3});
  }
  std::vector<RecordedToken> simd;
  {
    base::test::ScopedFeatureList enabled;
    enabled.InitAndEnableFeature(features::kHTMLTokenizerSimdScan);
    simd = TokenizeAll({chunk1, chunk2, chunk3});
  }
  EXPECT_EQ(baseline, simd);
}

TEST(HTMLTokenizerTest, SimdScanFeatureParams) {
  test::TaskEnvironment task_environment;
  std::string pad(80, 'z');
  String input(base::as_byte_span("<script>" + pad + "</script><style>" + pad +
                                  "</style>"));

  std::vector<RecordedToken> baseline;
  {
    base::test::ScopedFeatureList disabled;
    disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
    baseline = TokenizeAll({input});
  }

  const std::vector<base::FieldTrialParams> param_sets = {
      {{"min_length", "0"}},
      {{"min_length", "1"}},
      {{"min_length", "15"}},
      {{"min_length", "16"}},
      {{"min_length", "17"}},
      {{"min_length", "64"}},
      {{"script_data", "true"}, {"rawtext", "false"}},
      {{"script_data", "false"}, {"rawtext", "true"}},
      {{"script_data", "false"}, {"rawtext", "false"}},
  };
  for (const auto& params : param_sets) {
    base::test::ScopedFeatureList enabled;
    enabled.InitAndEnableFeatureWithParameters(features::kHTMLTokenizerSimdScan,
                                               params);
    EXPECT_EQ(baseline, TokenizeAll({input}));
  }
}

TEST(HTMLTokenizerTest, SimdScanVectorStrideBoundaries) {
  test::TaskEnvironment task_environment;
  // Cover 0 .. 2 * 16 + 1 prefix lengths to exercise SIMD loop + scalar tail.
  const std::vector<std::string> specials = {"<", "\n", "\r",
                                             std::string(1, '\0')};
  for (size_t prefix_len = 0; prefix_len <= 33; ++prefix_len) {
    std::string prefix(prefix_len, 'a');
    std::string suffix(20, 'b');
    for (const std::string& sp : specials) {
      std::string raw8 = "<script>" + prefix + sp + suffix + "</script>";
      String str8(base::as_byte_span(raw8));
      ASSERT_TRUE(str8.Is8Bit());

      // 16-bit with non-Latin-1 only in the tail (checks bitwise_or in tail).
      std::string raw16_tail =
          "<script>" + prefix + sp + suffix + "😀</script>";
      String str16_tail = String::FromUtf8(base::as_byte_span(raw16_tail));
      ASSERT_FALSE(str16_tail.Is8Bit());

      // 16-bit buffer whose script content is pure ASCII (checks Is8Bit()
      // remains true on the emitted character token).
      std::string raw16_ascii =
          "<!--😀--><script>" + prefix + sp + suffix + "</script>";
      String str16_ascii = String::FromUtf8(base::as_byte_span(raw16_ascii));
      ASSERT_FALSE(str16_ascii.Is8Bit());

      for (const String& s : {str8, str16_tail, str16_ascii}) {
        std::vector<RecordedToken> baseline;
        {
          base::test::ScopedFeatureList disabled;
          disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
          baseline = TokenizeAll({s});
        }
        std::vector<RecordedToken> simd;
        {
          base::test::ScopedFeatureList enabled;
          enabled.InitAndEnableFeatureWithParameters(
              features::kHTMLTokenizerSimdScan, {{"min_length", "1"}});
          simd = TokenizeAll({s});
        }
        EXPECT_EQ(baseline, simd);
      }
    }
  }
}

TEST(HTMLTokenizerTest, SimdScanNullInRawtextAndScript) {
  test::TaskEnvironment task_environment;
  std::string pad(80, 'a');
  std::string raw_style =
      "<style>" + pad + std::string(1, '\0') + pad + "</style>";
  std::string raw_script =
      "<script>" + pad + std::string(1, '\0') + pad + "</script>";

  for (const std::string& raw : {raw_style, raw_script}) {
    String input(base::as_byte_span(raw));
    std::vector<RecordedToken> baseline;
    {
      base::test::ScopedFeatureList disabled;
      disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
      baseline = TokenizeAll({input});
    }
    std::vector<RecordedToken> simd;
    {
      base::test::ScopedFeatureList enabled;
      enabled.InitAndEnableFeature(features::kHTMLTokenizerSimdScan);
      simd = TokenizeAll({input});
    }
    EXPECT_EQ(baseline, simd);
    // Ensure the character token does not contain raw NUL bytes.
    ASSERT_GE(simd.size(), 2u);
    EXPECT_EQ(HTMLToken::kCharacter, simd[1].type);
    EXPECT_EQ(kNotFound, simd[1].data.find('\0'));
  }
}

TEST(HTMLTokenizerTest, SimdScanQuotedAttributeValuesMatchesBaseline) {
  test::TaskEnvironment task_environment;
  std::string long_a(100, 'A');
  std::string long_b(100, 'B');
  std::string raw_8bit = "<img src=\"data:image/png;base64," + long_a +
                         "&amp;" + long_b + "\r\n" + std::string(1, '\0') +
                         long_a + "\" data-s='" + long_b + "&lt;" + long_a +
                         "\n" + std::string(1, '\0') + long_b + "'>";
  String str_8bit(base::as_byte_span(raw_8bit));
  ASSERT_TRUE(str_8bit.Is8Bit());

  std::string raw_16bit = "<a href=\"https://example.com/?q=" + long_a +
                          "&amp;x=" + long_b + "😀\" title='" + long_b + "🎉" +
                          long_a + "'>";
  String str_16bit = String::FromUtf8(base::as_byte_span(raw_16bit));
  ASSERT_FALSE(str_16bit.Is8Bit());

  auto tokenize_attrs = [](const std::vector<String>& chunks,
                           bool track_ranges) {
    HTMLParserOptions options;
    options.track_attributes_ranges = track_ranges;
    HTMLTokenizer tokenizer(options);
    SegmentedString input;
    for (const String& chunk : chunks) {
      input.Append(SegmentedString(chunk));
    }
    input.Append(
        SegmentedString(String(base::span_from_ref(kEndOfFileMarker))));
    input.Close();

    using AttrRecord = std::tuple<String, String, bool>;
    using RangeRecord = std::tuple<int, int, int, int>;
    std::vector<AttrRecord> attrs;
    std::vector<RangeRecord> ranges;
    while (HTMLToken* token = tokenizer.NextToken(input)) {
      const auto type = token->GetType();
      if (type == HTMLToken::kStartTag) {
        for (const auto& attr : token->Attributes()) {
          String val = attr.Value();
          attrs.emplace_back(attr.GetName().GetString(), val, val.Is8Bit());
        }
        if (track_ranges) {
          for (const auto& range : tokenizer.attributes_ranges().attributes()) {
            ranges.emplace_back(range.name_range.start, range.name_range.end,
                                range.value_range.start, range.value_range.end);
          }
        }
      }
      if (track_ranges) {
        tokenizer.attributes_ranges().Clear();
      }
      tokenizer.ClearToken();
      if (type == HTMLToken::kEndOfFile) {
        break;
      }
    }
    return std::make_pair(std::move(attrs), std::move(ranges));
  };

  std::vector<std::vector<String>> cases = {
      {str_8bit},
      {StringView(str_8bit, 0, 120).ToString(),
       StringView(str_8bit, 120).ToString()},
      {str_16bit},
      {StringView(str_16bit, 0, 110).ToString(),
       StringView(str_16bit, 110).ToString()},
      // Edge cases: no attributes, empty quoted values, EOF without closing
      // quote, and CRLF split across chunk boundaries.
      {String("<div></div>")},
      {String("<div a=\"\" b=''></div>")},
      {String(("<div a=\"" + long_a).c_str())},
      {String(("<div a=\"" + long_a + "\r").c_str()),
       String(("\n" + long_b + "\">").c_str())},
  };
  // SIMD stride boundary lengths (15, 16, 17, 18).
  for (size_t len : {15u, 16u, 17u, 18u}) {
    std::string payload(len, 'x');
    cases.push_back(
        {String(("<div a=\"" + payload + "\" b='" + payload + "'>").c_str())});
    std::string payload_16 =
        "<div a=\"" + payload + "😀\" b='" + payload + "🎉'>";
    cases.push_back({String::FromUtf8(base::as_byte_span(payload_16))});
  }

  for (const auto& chunks : cases) {
    for (bool track_ranges : {false, true}) {
      auto baseline = [&]() {
        base::test::ScopedFeatureList disabled;
        disabled.InitAndDisableFeature(features::kHTMLTokenizerSimdScan);
        return tokenize_attrs(chunks, track_ranges);
      }();
      for (const std::string& attr_param : {"true", "false"}) {
        base::test::ScopedFeatureList enabled;
        enabled.InitAndEnableFeatureWithParameters(
            features::kHTMLTokenizerSimdScan,
            {{"attribute_value", attr_param}, {"min_length", "16"}});
        EXPECT_EQ(baseline, tokenize_attrs(chunks, track_ranges));
      }
    }
  }
}

}  // namespace blink
