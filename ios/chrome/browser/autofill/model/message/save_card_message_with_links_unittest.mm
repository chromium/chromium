// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"

#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using SaveCardMessageWithLinksTest = PlatformTest;

// Tests that converting an empty vector returns an empty array.
TEST_F(SaveCardMessageWithLinksTest, ConvertFromEmpty) {
  std::vector<autofill::LegalMessageLine> lines;
  NSArray<SaveCardMessageWithLinks*>* result =
      [SaveCardMessageWithLinks convertFrom:lines];

  EXPECT_EQ(0u, result.count);
}

// Tests converting a single legal message line without links.
TEST_F(SaveCardMessageWithLinksTest, ConvertFromSingleLineWithoutLinks) {
  std::vector<autofill::LegalMessageLine> lines = {
      autofill::TestLegalMessageLine("Simple legal message without links.")};
  NSArray<SaveCardMessageWithLinks*>* result =
      [SaveCardMessageWithLinks convertFrom:lines];

  ASSERT_EQ(1u, result.count);
  SaveCardMessageWithLinks* line = result[0];
  EXPECT_NSEQ(@"Simple legal message without links.", line.messageText);
  EXPECT_EQ(0u, line.linkRanges.count);
  EXPECT_TRUE(line.linkURLs.empty());
}

// Tests converting a legal message line with multiple link ranges.
TEST_F(SaveCardMessageWithLinksTest, ConvertFromMultipleLinks) {
  std::vector<autofill::LegalMessageLine> lines = {
      autofill::TestLegalMessageLine(
          "By continuing you agree to {0} and {1}.",
          {autofill::TestLegalMessageLine::Link(0, 10,
                                                "https://example.com/terms"),
           autofill::TestLegalMessageLine::Link(
               15, 23, "https://example.com/privacy")})};
  NSArray<SaveCardMessageWithLinks*>* result =
      [SaveCardMessageWithLinks convertFrom:lines];

  ASSERT_EQ(1u, result.count);
  SaveCardMessageWithLinks* line = result[0];
  EXPECT_NSEQ(@"By continuing you agree to {0} and {1}.", line.messageText);
  ASSERT_EQ(2u, line.linkRanges.count);
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, 10)], line.linkRanges[0]);
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(15, 8)], line.linkRanges[1]);
  ASSERT_EQ(2u, line.linkURLs.size());
  EXPECT_EQ(GURL("https://example.com/terms"), line.linkURLs[0]);
  EXPECT_EQ(GURL("https://example.com/privacy"), line.linkURLs[1]);
}

// Tests converting multiple legal message lines.
TEST_F(SaveCardMessageWithLinksTest, ConvertFromMultipleLines) {
  std::vector<autofill::LegalMessageLine> lines = {
      autofill::TestLegalMessageLine("Line 1 with {0}.",
                                     {autofill::TestLegalMessageLine::Link(
                                         0, 6, "https://example.com/1")}),
      autofill::TestLegalMessageLine("Line 2 with {0}.",
                                     {autofill::TestLegalMessageLine::Link(
                                         0, 6, "https://example.com/2")})};
  NSArray<SaveCardMessageWithLinks*>* result =
      [SaveCardMessageWithLinks convertFrom:lines];

  ASSERT_EQ(2u, result.count);
  EXPECT_NSEQ(@"Line 1 with {0}.", result[0].messageText);
  ASSERT_EQ(1u, result[0].linkRanges.count);
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, 6)],
              result[0].linkRanges[0]);
  ASSERT_EQ(1u, result[0].linkURLs.size());
  EXPECT_EQ(GURL("https://example.com/1"), result[0].linkURLs[0]);

  EXPECT_NSEQ(@"Line 2 with {0}.", result[1].messageText);
  ASSERT_EQ(1u, result[1].linkRanges.count);
  EXPECT_NSEQ([NSValue valueWithRange:NSMakeRange(0, 6)],
              result[1].linkRanges[0]);
  ASSERT_EQ(1u, result[1].linkURLs.size());
  EXPECT_EQ(GURL("https://example.com/2"), result[1].linkURLs[0]);
}

}  // namespace
