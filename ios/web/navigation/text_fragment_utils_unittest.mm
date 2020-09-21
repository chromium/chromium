// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/text_fragment_utils.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

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

typedef PlatformTest TextFragmentUtilsTest;

TEST_F(TextFragmentUtilsTest, AreTextFragmentsAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kScrollToTextIOS);

  std::unique_ptr<TestWebState> web_state = std::make_unique<TestWebState>();
  TestWebState* web_state_ptr = web_state.get();
  FakeNavigationContext context;
  context.SetWebState(std::move(web_state));

  // Working case: no opener, has user gesture, not same document
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(false);
  EXPECT_TRUE(AreTextFragmentsAllowed(&context));

  // Blocking case #1: WebState has an opener
  web_state_ptr->SetHasOpener(true);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(false);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));

  // Blocking case #2: No user gesture
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(false);
  context.SetIsSameDocument(false);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));

  // Blocking case #3: Same-document navigation
  web_state_ptr->SetHasOpener(false);
  context.SetHasUserGesture(true);
  context.SetIsSameDocument(true);
  EXPECT_FALSE(AreTextFragmentsAllowed(&context));
}

TEST_F(TextFragmentUtilsTest, ParseTextFragments) {
  GURL url_with_fragment(
      "https://www.example.com/#idFrag:~:text=text%201&text=text%202");
  base::Value result = internal::ParseTextFragments(url_with_fragment);
  ASSERT_EQ(2u, result.GetList().size());
  EXPECT_EQ("text 1", result.GetList()[0].FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("text 2", result.GetList()[1].FindKey(kTextStartKey)->GetString());

  GURL url_no_fragment("www.example.com");
  base::Value empty_result = internal::ParseTextFragments(url_no_fragment);
  EXPECT_TRUE(empty_result.is_none());
}

TEST_F(TextFragmentUtilsTest, ExtractTextFragments) {
  std::vector<std::string> expected = {"test1", "test2", "test3"};
  // Ensure presence/absence of a trailing & doesn't break anything
  EXPECT_EQ(expected, internal::ExtractTextFragments(
                          "#id:~:text=test1&text=test2&text=test3"));
  EXPECT_EQ(expected, internal::ExtractTextFragments(
                          "#id:~:text=test1&text=test2&text=test3&"));

  // Test that empty tokens (&& or &text=&) are discarded
  EXPECT_EQ(expected, internal::ExtractTextFragments(
                          "#id:~:text=test1&&text=test2&text=&text=test3"));

  expected = {};
  EXPECT_EQ(expected,
            internal::ExtractTextFragments("#idButNoTextFragmentsHere"));
  EXPECT_EQ(expected, internal::ExtractTextFragments(""));
}

TEST_F(TextFragmentUtilsTest, TextFragmentToValue) {
  // Success cases
  std::string fragment = "start";
  base::Value result = internal::TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,end";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "prefix-,start";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  fragment = "prefix-,start,end";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  fragment = "start,end,-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  fragment = "prefix-,start,end,-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kSuffixKey)->GetString());

  // Trailing comma doesn't break otherwise valid fragment
  fragment = "start,";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kPrefixKey));
  EXPECT_EQ("start", result.FindKey(kTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kTextEndKey));
  EXPECT_FALSE(result.FindKey(kSuffixKey));

  // Failure Cases
  fragment = "";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "some,really-,malformed,-thing,with,too,many,commas";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "start,prefix-,-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix,start";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "-suffix";
  result = internal::TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());
}

}  // namespace web
