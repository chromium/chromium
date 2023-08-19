// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/string_util.h"

#import <UIKit/UIKit.h>

#import "base/ios/ns_range.h"
#import "base/test/gtest_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using StringUtilTest = PlatformTest;

TEST_F(StringUtilTest, AttributedStringFromStringWithLink) {
  struct TestCase {
    NSString* input;
    NSDictionary* textAttributes;
    NSDictionary* linkAttributes;
    NSString* expectedString;
    NSRange expectedTextRange;
    NSRange expectedLinkRange;
  };

  const TestCase kAllTestCases[] = {
      TestCase{@"Text with valid BEGIN_LINK link END_LINK and spaces.", @{},
               @{NSLinkAttributeName : @"google.com"},
               @"Text with valid link and spaces.", NSRange{0, 16},
               NSRange{16, 4}},
      TestCase{
          @"Text with valid BEGIN_LINK link END_LINK and spaces.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{}, @"Text with valid link and spaces.", NSRange{0, 32},
          NSRange{0, 32}},
      TestCase{
          @"Text with valid BEGIN_LINK link END_LINK and spaces.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
          @"Text with valid link and spaces.",
          NSRange{0, 16},
          NSRange{16, 4},
      },
      TestCase{
          @"Text with valid BEGIN_LINKlinkEND_LINK and no spaces.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
          @"Text with valid link and no spaces.",
          NSRange{0, 16},
          NSRange{16, 4},
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const NSAttributedString* result = AttributedStringFromStringWithLink(
        test_case.input, test_case.textAttributes, test_case.linkAttributes);
    EXPECT_NSEQ(result.string, test_case.expectedString);

    // Text at index 0 has text attributes applied until the link location.
    NSRange textRange;
    NSDictionary* resultTextAttributes = [result attributesAtIndex:0
                                                    effectiveRange:&textRange];
    EXPECT_TRUE(NSEqualRanges(test_case.expectedTextRange, textRange));
    EXPECT_NSEQ(test_case.textAttributes, resultTextAttributes);

    // Text at index `expectedRange.location` has link attributes applied.
    NSRange linkRange;
    NSDictionary* resultLinkAttributes =
        [result attributesAtIndex:test_case.expectedLinkRange.location
                   effectiveRange:&linkRange];
    EXPECT_TRUE(NSEqualRanges(test_case.expectedLinkRange, linkRange));

    NSMutableDictionary* combinedAttributes =
        [[NSMutableDictionary alloc] init];
    [combinedAttributes addEntriesFromDictionary:test_case.textAttributes];
    [combinedAttributes addEntriesFromDictionary:test_case.linkAttributes];
    EXPECT_NSEQ(combinedAttributes, resultLinkAttributes);
  }
}

TEST_F(StringUtilTest, AttributedStringFromStringWithLinkFailures) {
  struct TestCase {
    NSString* input;
    NSDictionary* textAttributes;
    NSDictionary* linkAttributes;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text without link.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
      },
      TestCase{
          @"Text with BEGIN_LINK and no end link.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
      },
      TestCase{
          @"Text with no begin link and END_LINK.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
      },
      TestCase{
          @"Text with END_LINK before BEGIN_LINK.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"google.com"},
      },

  };

  for (const TestCase& test_case : kAllTestCases) {
    EXPECT_CHECK_DEATH(AttributedStringFromStringWithLink(
        test_case.input, test_case.textAttributes, test_case.linkAttributes));
  }
}

TEST_F(StringUtilTest, AttributedStringFromStringWithLinkWithEmptyLink) {
  struct TestCase {
    NSString* input;
    NSDictionary* textAttributes;
    NSDictionary* linkAttributes;
    NSString* expectedString;
  };
  const TestCase test_case = TestCase {
    @"Text with empty link BEGIN_LINK END_LINK.",
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
        @{NSLinkAttributeName : @"google.com"}, @"Text with empty link .",
  };

  const NSAttributedString* result = AttributedStringFromStringWithLink(
      test_case.input, test_case.textAttributes, test_case.linkAttributes);
  EXPECT_NSEQ(result.string, test_case.expectedString);

  // Text attributes apply to the full range of the result string.
  NSRange textRange;
  NSDictionary* resultTextAttributes = [result attributesAtIndex:0
                                                  effectiveRange:&textRange];
  EXPECT_TRUE(NSEqualRanges(NSMakeRange(0, test_case.expectedString.length),
                            textRange));
  EXPECT_NSEQ(test_case.textAttributes, resultTextAttributes);
}

TEST_F(StringUtilTest, ParseStringWithLinks) {
  struct TestCase {
    NSString* input;
    StringWithTags expected;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text without link.",
          StringWithTags{
              @"Text without link.",
              {},
          },
      },
      TestCase{
          @"Text with empty link BEGIN_LINK END_LINK.",
          StringWithTags{
              @"Text with empty link .",
              {NSRange{21, 0}},
          },
      },
      TestCase{
          @"Text with BEGIN_LINK and no end link.",
          StringWithTags{
              @"Text with BEGIN_LINK and no end link.",
              {},
          },
      },
      TestCase{
          @"Text with no begin link and END_LINK.",
          StringWithTags{
              @"Text with no begin link and END_LINK.",
              {},
          },
      },
      TestCase{@"Text with END_LINK before BEGIN_LINK.",
               StringWithTags{
                   @"Text with END_LINK before BEGIN_LINK.",
                   {},
               }},
      TestCase{
          @"Text with valid BEGIN_LINK link END_LINK and spaces.",
          StringWithTags{
              @"Text with valid link and spaces.",
              {NSRange{16, 4}},
          },
      },
      TestCase{
          @"Text with valid BEGIN_LINKlinkEND_LINK and no spaces.",
          StringWithTags{
              @"Text with valid link and no spaces.",
              {NSRange{16, 4}},
          },
      },
      TestCase{
          @"Text with multiple tags: BEGIN_LINKlink1END_LINK, "
          @"BEGIN_LINKlink2END_LINK and BEGIN_LINKlink3END_LINK.",
          StringWithTags{
              @"Text with multiple tags: link1, link2 and link3.",
              {NSRange{25, 5}, NSRange{32, 5}, NSRange{42, 5}},
          },
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const StringWithTags result = ParseStringWithLinks(test_case.input);
    EXPECT_NSEQ(result.string, test_case.expected.string);
    ASSERT_EQ(result.ranges.size(), test_case.expected.ranges.size());
    for (size_t i = 0; i < test_case.expected.ranges.size(); i++) {
      EXPECT_EQ(result.ranges[i], test_case.expected.ranges[i]);
    }
  }
}

TEST_F(StringUtilTest, ParseStringWithTag) {
  struct TestCase {
    NSString* input;
    StringWithTag expected;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text without tag.",
          StringWithTag{
              @"Text without tag.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{
          @"Text with empty tag BEGIN_TAG END_TAG.",
          StringWithTag{
              @"Text with empty tag  .",
              NSRange{20, 1},
          },
      },
      TestCase{
          @"Text with BEGIN_TAG and no end tag.",
          StringWithTag{
              @"Text with BEGIN_TAG and no end tag.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{
          @"Text with no begin tag and END_TAG.",
          StringWithTag{
              @"Text with no begin tag and END_TAG.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{@"Text with END_TAG before BEGIN_TAG.",
               StringWithTag{
                   @"Text with END_TAG before BEGIN_TAG.",
                   NSRange{NSNotFound, 0},
               }},
      TestCase{
          @"Text with valid BEGIN_TAG tag END_TAG and spaces.",
          StringWithTag{
              @"Text with valid  tag  and spaces.",
              NSRange{16, 5},
          },
      },
      TestCase{
          @"Text with valid BEGIN_TAGtagEND_TAG and no spaces.",
          StringWithTag{
              @"Text with valid tag and no spaces.",
              NSRange{16, 3},
          },
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const StringWithTag result =
        ParseStringWithTag(test_case.input, @"BEGIN_TAG", @"END_TAG");
    EXPECT_NSEQ(result.string, test_case.expected.string);
    EXPECT_EQ(result.range, test_case.expected.range);
  }
}

TEST_F(StringUtilTest, ParseStringWithTags) {
  struct TestCase {
    NSString* input;
    StringWithTags expected;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text without tag.",
          StringWithTags{
              @"Text without tag.",
              {},
          },
      },
      TestCase{
          @"Text with empty tag BEGIN_TAG END_TAG.",
          StringWithTags{
              @"Text with empty tag  .",
              {NSRange{20, 1}},
          },
      },
      TestCase{
          @"Text with BEGIN_TAG and no end tag.",
          StringWithTags{
              @"Text with BEGIN_TAG and no end tag.",
              {},
          },
      },
      TestCase{
          @"Text with no begin tag and END_TAG.",
          StringWithTags{
              @"Text with no begin tag and END_TAG.",
              {},
          },
      },
      TestCase{@"Text with END_TAG before BEGIN_TAG.",
               StringWithTags{
                   @"Text with END_TAG before BEGIN_TAG.",
                   {},
               }},
      TestCase{
          @"Text with valid BEGIN_TAG tag END_TAG and spaces.",
          StringWithTags{
              @"Text with valid  tag  and spaces.",
              {NSRange{16, 5}},
          },
      },
      TestCase{
          @"Text with valid BEGIN_TAGtagEND_TAG and no spaces.",
          StringWithTags{
              @"Text with valid tag and no spaces.",
              {NSRange{16, 3}},
          },
      },
      TestCase{
          @"Text with multiple tags: BEGIN_TAGtag1END_TAG, "
          @"BEGIN_TAGtag2END_TAG and BEGIN_TAGtag3END_TAG.",
          StringWithTags{
              @"Text with multiple tags: tag1, tag2 and tag3.",
              {NSRange{25, 4}, NSRange{31, 4}, NSRange{40, 4}},
          },
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const StringWithTags result =
        ParseStringWithTags(test_case.input, @"BEGIN_TAG", @"END_TAG");
    EXPECT_NSEQ(result.string, test_case.expected.string);
    ASSERT_EQ(result.ranges.size(), test_case.expected.ranges.size());
    for (size_t i = 0; i < test_case.expected.ranges.size(); i++) {
      EXPECT_EQ(result.ranges[i], test_case.expected.ranges[i]);
    }
  }
}

// Verifies when it should return CGRectNull and when it shouldn't.
TEST_F(StringUtilTest, TextViewLinkBound) {
  UITextView* text_view = CreateUITextViewWithTextKit1();
  text_view.text = @"Some text.";

  // Returns CGRectNull for empty NSRange.
  EXPECT_TRUE(CGRectEqualToRect(
      TextViewLinkBound(text_view, NSMakeRange(-1, -1)), CGRectNull));

  // Returns CGRectNull for a range out of bound.
  EXPECT_TRUE(CGRectEqualToRect(
      TextViewLinkBound(text_view, NSMakeRange(20, 5)), CGRectNull));

  // Returns a CGRect diffent from CGRectNull when there is a range in bound.
  EXPECT_FALSE(CGRectEqualToRect(
      TextViewLinkBound(text_view, NSMakeRange(0, 5)), CGRectNull));
}
}  // namespace
