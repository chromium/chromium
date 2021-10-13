// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

#include <gtest/gtest.h>

#define EXPECT_SELECTORS_EQ(a, b)    \
  EXPECT_EQ(a.Type(), b.Type());     \
  EXPECT_EQ(a.Start(), b.Start());   \
  EXPECT_EQ(a.End(), b.End());       \
  EXPECT_EQ(a.Prefix(), b.Prefix()); \
  EXPECT_EQ(a.Suffix(), b.Suffix());

namespace blink {

static const TextFragmentSelector kInvalidSelector(
    TextFragmentSelector::kInvalid);

TEST(TextFragmentSelectorTest, ExactText) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "", "",
                                "");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, ExactTextWithPrefix) {
  TextFragmentSelector selector = TextFragmentSelector::Create("prefix-,test");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "",
                                "prefix", "");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, ExactTextWithSuffix) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "", "",
                                "suffix");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, ExactTextWithContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kExact, "test", "",
                                "prefix", "suffix");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, TextRange) {
  TextFragmentSelector selector = TextFragmentSelector::Create("test,page");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "", "");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, TextRangeWithPrefix) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,page");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "prefix", "");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, TextRangeWithSuffix) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("test,page,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "", "suffix");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, TextRangeWithContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix-,test,page,-suffix");
  TextFragmentSelector expected(TextFragmentSelector::kRange, "test", "page",
                                "prefix", "suffix");
  EXPECT_SELECTORS_EQ(selector, expected);
}

TEST(TextFragmentSelectorTest, InvalidContext) {
  TextFragmentSelector selector =
      TextFragmentSelector::Create("prefix,test,page,suffix");
  EXPECT_SELECTORS_EQ(selector, kInvalidSelector);
}

TEST(TextFragmentSelectorTest, TooManyParameters) {
  TextFragmentSelector selector = TextFragmentSelector::Create(
      "prefix-,exact text, that has commas, which are not percent "
      "encoded,-suffix");
  EXPECT_SELECTORS_EQ(selector, kInvalidSelector);
}

TEST(TextFragmentSelectorTest, Empty) {
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create(""), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("-"), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("-,"), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create(",-"), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("-,-"), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create(","), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create(",,"), kInvalidSelector);
}

TEST(TextFragmentSelectorTest, NoMatchTextWithPrefix) {
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("prefix-"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("prefix-,"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("text,prefix-"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("text,prefix-,text"),
                      kInvalidSelector);
}

TEST(TextFragmentSelectorTest, NoMatchTextWithSuffix) {
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("text,-"), kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("-suffix"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create(",-suffix"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("-suffix,"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("text,-suffix,"),
                      kInvalidSelector);
}

TEST(TextFragmentSelectorTest, NoMatchTextWithPrefixAndSuffix) {
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("prefix-,-suffix"),
                      kInvalidSelector);
  EXPECT_SELECTORS_EQ(TextFragmentSelector::Create("prefix-,-suffix,invalid"),
                      kInvalidSelector);
}

}  // namespace blink
