// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/text_fragments_utils.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// These values correspond to the members that the JavaScript implementation is
// expecting.
const char kPrefixKey[] = "prefix";
const char kTextStartKey[] = "textStart";
const char kTextEndKey[] = "textEnd";
const char kSuffixKey[] = "suffix";

}  // namespace

namespace web {

typedef PlatformTest TextFragmentsUtilsTest;

TEST_F(TextFragmentsUtilsTest, ParseTextFragments) {
  GURL url_with_fragment(
      "https://www.example.com/#idFrag:~:text=text%201&text=text%202");
  base::Value result = ParseTextFragments(url_with_fragment);
  ASSERT_EQ(2u, result.GetList().size());
  EXPECT_EQ("text 1", result.GetList()[0].FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("text 2", result.GetList()[1].FindKey(kTextStartKey)->GetString());

  GURL url_no_fragment("www.example.com");
  base::Value empty_result = ParseTextFragments(url_no_fragment);
  EXPECT_TRUE(empty_result.is_none());
}

TEST_F(TextFragmentsUtilsTest, ExtractTextFragments) {
  std::vector<std::string> expected = {"test1", "test2", "test3"};
  // Ensure presence/absence of a trailing & doesn't break anything
  EXPECT_EQ(expected,
            ExtractTextFragments("#id:~:text=test1&text=test2&text=test3"));
  EXPECT_EQ(expected,
            ExtractTextFragments("#id:~:text=test1&text=test2&text=test3&"));

  // Test that empty tokens (&& or &text=&) are discarded
  EXPECT_EQ(expected, ExtractTextFragments(
                          "#id:~:text=test1&&text=test2&text=&text=test3"));

  expected.clear();
  EXPECT_EQ(expected, ExtractTextFragments("#idButNoTextFragmentsHere"));
  EXPECT_EQ(expected, ExtractTextFragments(""));
}

TEST_F(TextFragmentsUtilsTest, TextFragmentToValue) {
  // Success cases
  std::string fragment = "start";
  base::Value result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "prefix-,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  fragment = "prefix-,start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  fragment = "prefix-,start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  // Trailing comma doesn't break otherwise valid fragment
  fragment = "start,";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  // Failure Cases
  fragment = "";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "some,really-,malformed,-thing,with,too,many,commas";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "start,prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());
}

}  // namespace web
