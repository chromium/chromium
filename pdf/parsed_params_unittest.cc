// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/parsed_params.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

namespace {

constexpr char kDummyOriginalUrl[] = "https://test.com/dummy.pdf";
constexpr char kDummySrcUrl[] = "chrome-extension://dummy-source-url";

constexpr SkColor kNewBackgroundColor = SkColorSetARGB(0xFF, 0x52, 0x56, 0x59);

// `kNewBackgroundColor` as a decimal number in string format.
constexpr char kNewBackgroundColorStr[] = "4283586137";

// Creates a `blink::WebPluginParams` without any URL attributes, namely "src"
// and "original-url". The return value only contains valid "background-color"
// and "full-frame" attributes.
blink::WebPluginParams CreateWebPluginParamsWithoutUrl() {
  blink::WebPluginParams params;
  params.attribute_names.push_back(blink::WebString("background-color"));
  params.attribute_values.push_back(blink::WebString(kNewBackgroundColorStr));
  params.attribute_names.push_back(blink::WebString("full-frame"));
  params.attribute_values.push_back(blink::WebString(""));
  return params;
}

// Creates a `blink::WebPluginParams` with only the URL attributes: "src" and
// "original-url".
blink::WebPluginParams CreateWebPluginParamsWithUrls() {
  blink::WebPluginParams params;
  params.attribute_names.push_back(blink::WebString("original-url"));
  params.attribute_values.push_back(blink::WebString(kDummyOriginalUrl));
  params.attribute_names.push_back(blink::WebString("src"));
  params.attribute_values.push_back(blink::WebString(kDummySrcUrl));
  return params;
}

}  // namespace

TEST(ParsedParamsTest, ParseValidWebPluginParams) {
  blink::WebPluginParams params = CreateWebPluginParamsWithoutUrl();
  params.attribute_names.push_back(blink::WebString("original-url"));
  params.attribute_values.push_back(blink::WebString(kDummyOriginalUrl));
  params.attribute_names.push_back(blink::WebString("src"));
  params.attribute_values.push_back(blink::WebString(kDummySrcUrl));

  absl::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kDummyOriginalUrl, result->original_url);
  EXPECT_EQ(kDummySrcUrl, result->src_url);
  ASSERT_TRUE(result->background_color.has_value());
  EXPECT_EQ(kNewBackgroundColor, result->background_color.value());
  EXPECT_TRUE(result->full_frame);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithoutSourceUrl) {
  blink::WebPluginParams params = CreateWebPluginParamsWithoutUrl();
  params.attribute_names.push_back(blink::WebString("original-url"));
  params.attribute_values.push_back(blink::WebString(kDummyOriginalUrl));

  // Expect the `ParsedParams` to be invalid due to missing the source URL.
  absl::optional<ParsedParams> result = ParseWebPluginParams(params);
  EXPECT_FALSE(result.has_value());
}

TEST(ParseParsedParamsTest, ParseWebPluginParamsWithoutOriginalUrl) {
  blink::WebPluginParams params = CreateWebPluginParamsWithoutUrl();
  params.attribute_names.push_back(blink::WebString("src"));
  params.attribute_values.push_back(blink::WebString(kDummySrcUrl));

  // Expect the `ParsedParams` to be valid and `original_url` to be the same as
  // `src_url`.
  absl::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kDummySrcUrl, result->original_url);
  EXPECT_EQ(kDummySrcUrl, result->src_url);
  ASSERT_TRUE(result->background_color.has_value());
  EXPECT_EQ(kNewBackgroundColor, result->background_color.value());
  EXPECT_TRUE(result->full_frame);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithoutBackgroundColor) {
  blink::WebPluginParams params = CreateWebPluginParamsWithUrls();

  // The `ParsedParams` can still be valid without the background color
  // attribute.
  absl::optional<ParsedParams> result = ParseWebPluginParams(params);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kDummyOriginalUrl, result->original_url);
  EXPECT_EQ(kDummySrcUrl, result->src_url);
  EXPECT_FALSE(result->background_color.has_value());
  EXPECT_FALSE(result->full_frame);
}

TEST(ParsedParamsTest, ParseWebPluginParamsWithInvalidBackgroundColor) {
  blink::WebPluginParams params = CreateWebPluginParamsWithUrls();
  params.attribute_names.push_back(blink::WebString("background-color"));
  params.attribute_values.push_back(blink::WebString("red"));

  // Expect the `ParsedParams` to be invalid because the background color's
  // attribute value is in the wrong format.
  absl::optional<ParsedParams> result = ParseWebPluginParams(params);
  EXPECT_FALSE(result.has_value());
}

}  // namespace chrome_pdf
