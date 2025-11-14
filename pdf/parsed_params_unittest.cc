// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/parsed_params.h"

#include <optional>
#include <string>
#include <vector>

#include "pdf/pdfium/pdfium_form_filler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

namespace {

using ::testing::AnyOf;

constexpr char kFakeSrcUrl[] = "chrome-extension://fake-source-url";

// Creates a `blink::WebPluginParams` with only required attributes.
blink::WebPluginParams CreateMinimalWebPluginParams() {
  blink::WebPluginParams params;
  params.attribute_names.push_back(blink::WebString("src"));
  params.attribute_values.push_back(blink::WebString(kFakeSrcUrl));
  return params;
}

}  // namespace

TEST(ParsedParamsTest, ParseWebPluginParamsMinimal) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(kFakeSrcUrl, result->src_url);
  EXPECT_EQ(kFakeSrcUrl, result->original_url);
  EXPECT_EQ("", result->top_level_url);
  EXPECT_FALSE(result->full_frame);
  EXPECT_EQ(SK_ColorTRANSPARENT, result->background_color);
  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kJavaScript, result->script_option);
  EXPECT_FALSE(result->use_skia);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithoutSourceUrl) {
  blink::WebPluginParams params;

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  EXPECT_FALSE(result.has_value());
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithOriginalUrl) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("original-url"));
  params.attribute_values.push_back(
      blink::WebString("https://example.com/original.pdf"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(kFakeSrcUrl, result->src_url);
  EXPECT_EQ("https://example.com/original.pdf", result->original_url);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithTopLevelUrl) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("top-level-url"));
  params.attribute_values.push_back(
      blink::WebString("https://example.net/top.html"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ("https://example.net/top.html", result->top_level_url);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithFullFrame) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("full-frame"));
  params.attribute_values.push_back(blink::WebString(""));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_TRUE(result->full_frame);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithFullFrameNonEmpty) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("full-frame"));
  params.attribute_values.push_back(blink::WebString("false"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_TRUE(result->full_frame);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithBackgroundColor) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("background-color"));
  params.attribute_values.push_back(blink::WebString("4283586137"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(4283586137, result->background_color);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithInvalidBackgroundColor) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("background-color"));
  params.attribute_values.push_back(blink::WebString("red"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  EXPECT_FALSE(result.has_value());
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithJavascriptAllow) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("javascript"));
  params.attribute_values.push_back(blink::WebString("allow"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_THAT(result->script_option,
              AnyOf(PDFiumFormFiller::ScriptOption::kJavaScript,
                    PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA));
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithJavascriptEmpty) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("javascript"));
  params.attribute_values.push_back(blink::WebString(""));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kNoJavaScript,
            result->script_option);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithJavascriptNonEmpty) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("javascript"));
  params.attribute_values.push_back(blink::WebString("true"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kNoJavaScript,
            result->script_option);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithHasUseSkia) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("use-skia"));
  params.attribute_values.push_back(blink::WebString(""));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_TRUE(result->use_skia);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithHasUseSkiaNonEmpty) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.push_back(blink::WebString("use-skia"));
  params.attribute_values.push_back(blink::WebString("false"));

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_TRUE(result->use_skia);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithHasAllowXfa) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.emplace_back("javascript");
  params.attribute_values.emplace_back("allow");
  params.attribute_names.emplace_back("allow-xfa-forms");
  params.attribute_values.emplace_back("");

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA,
            result->script_option);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithHasAllowXfaNonEmpty) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.emplace_back("javascript");
  params.attribute_values.emplace_back("allow");
  params.attribute_names.emplace_back("allow-xfa-forms");
  params.attribute_values.emplace_back("false");

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kJavaScriptAndXFA,
            result->script_option);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithHasAllowXfaNoJavaScript) {
  blink::WebPluginParams params = CreateMinimalWebPluginParams();
  params.attribute_names.emplace_back("javascript");
  params.attribute_values.emplace_back("");
  params.attribute_names.emplace_back("allow-xfa-forms");
  params.attribute_values.emplace_back("");

  std::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(PDFiumFormFiller::ScriptOption::kNoJavaScript,
            result->script_option);
}

}  // namespace chrome_pdf
