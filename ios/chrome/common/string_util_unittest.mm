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

// Tests that AttributedStringFromStringWithLink correctly parses a string with
// a single link and applies the expected attributes to the text and link
// ranges.
TEST_F(StringUtilTest, AttributedStringFromStringWithLink) {
  struct TestCase {
    NSString* input;
    NSDictionary* text_attributes;
    NSDictionary* link_attributes;
    NSString* expected_string;
    NSRange expected_text_range;
    NSRange expected_link_range;
  };

  const TestCase kAllTestCases[] = {
      TestCase{@"Text with valid BEGIN_LINK link END_LINK and spaces.", @{},
               @{NSLinkAttributeName : @"chromium.org"},
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
          @{NSLinkAttributeName : @"chromium.org"},
          @"Text with valid link and spaces.",
          NSRange{0, 16},
          NSRange{16, 4},
      },
      TestCase{
          @"Text with valid BEGIN_LINKlinkEND_LINK and no spaces.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @{NSLinkAttributeName : @"chromium.org"},
          @"Text with valid link and no spaces.",
          NSRange{0, 16},
          NSRange{16, 4},
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const NSAttributedString* result = AttributedStringFromStringWithLink(
        test_case.input, test_case.text_attributes, test_case.link_attributes);
    EXPECT_NSEQ(result.string, test_case.expected_string);

    // Text at index 0 has text attributes applied until the link location.
    NSRange text_range;
    NSDictionary* result_text_attributes =
        [result attributesAtIndex:0 effectiveRange:&text_range];
    EXPECT_TRUE(NSEqualRanges(test_case.expected_text_range, text_range));
    EXPECT_NSEQ(test_case.text_attributes, result_text_attributes);

    // Text at index `expected_link_range.location` has link attributes applied.
    NSRange link_range;
    NSDictionary* result_link_attributes =
        [result attributesAtIndex:test_case.expected_link_range.location
                   effectiveRange:&link_range];
    EXPECT_TRUE(NSEqualRanges(test_case.expected_link_range, link_range));

    NSMutableDictionary* combined_attributes =
        [[NSMutableDictionary alloc] init];
    [combined_attributes addEntriesFromDictionary:test_case.text_attributes];
    [combined_attributes addEntriesFromDictionary:test_case.link_attributes];
    EXPECT_NSEQ(combined_attributes, result_link_attributes);
  }
}

// Tests that AttributedStringFromStringWithLinks correctly parses a string with
// multiple links and applies separate link attributes to each respective link.
TEST_F(StringUtilTest, AttributedStringFromStringWithLinks) {
  struct TestCase {
    NSString* input;
    NSDictionary* text_attributes;
    NSArray<NSDictionary*>* links_attributes;
    NSString* expected_string;
    std::vector<NSRange> expected_link_ranges;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text with BEGIN_LINKlink1END_LINK and BEGIN_LINKlink2END_LINK.",
          @{},
          @[
            @{NSLinkAttributeName : @"chromium.org"},
            @{NSLinkAttributeName : @"example.com"}
          ],
          @"Text with link1 and link2.",
          {NSRange{10, 5}, NSRange{20, 5}}},
      TestCase{
          @"Text with BEGIN_LINKlink1END_LINK and BEGIN_LINKlink2END_LINK.",
          @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
          @[
            @{NSLinkAttributeName : @"chromium.org"},
            @{NSLinkAttributeName : @"example.com"}
          ],
          @"Text with link1 and link2.",
          {NSRange{10, 5}, NSRange{20, 5}}},
  };

  for (const TestCase& test_case : kAllTestCases) {
    const NSAttributedString* result = AttributedStringFromStringWithLinks(
        test_case.input, test_case.text_attributes, test_case.links_attributes);
    EXPECT_NSEQ(result.string, test_case.expected_string);

    // Verify that the non-link text has only the expected base attributes (or
    // none).
    NSRange text_range;
    NSDictionary* result_text_attributes =
        [result attributesAtIndex:0 effectiveRange:&text_range];
    EXPECT_NSEQ(test_case.text_attributes, result_text_attributes);

    for (size_t i = 0; i < test_case.expected_link_ranges.size(); ++i) {
      NSRange link_range;
      NSDictionary* result_link_attributes =
          [result attributesAtIndex:test_case.expected_link_ranges[i].location
                     effectiveRange:&link_range];
      EXPECT_TRUE(NSEqualRanges(test_case.expected_link_ranges[i], link_range));

      NSMutableDictionary* combined_attributes =
          [[NSMutableDictionary alloc] init];
      [combined_attributes addEntriesFromDictionary:test_case.text_attributes];
      [combined_attributes
          addEntriesFromDictionary:test_case.links_attributes[i]];
      EXPECT_NSEQ(combined_attributes, result_link_attributes);
    }
  }

  // Test case with no links and nil link attributes
  const NSAttributedString* result =
      AttributedStringFromStringWithLinks(@"Text without links.", @{}, nil);
  EXPECT_NSEQ(result.string, @"Text without links.");

  NSRange text_range;
  NSDictionary* result_attributes = [result attributesAtIndex:0
                                               effectiveRange:&text_range];
  EXPECT_NSEQ(@{}, result_attributes);
  // Verifies that the "no attributes" state applies to the entire string.
  EXPECT_EQ(0U, text_range.location);
  EXPECT_EQ(result.length, text_range.length);
}

// Tests that AttributedStringFromStringWithLink handles an empty link segment
// gracefully and returns a string with text attributes applied.
TEST_F(StringUtilTest, AttributedStringFromStringWithLinkWithEmptyLink) {
  struct TestCase {
    NSString* input;
    NSDictionary* text_attributes;
    NSDictionary* link_attributes;
    NSString* expected_string;
  };
  const TestCase test_case = TestCase {
    @"Text with empty link BEGIN_LINK END_LINK.",
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]},
        @{NSLinkAttributeName : @"chromium.org"}, @"Text with empty link .",
  };

  const NSAttributedString* result = AttributedStringFromStringWithLink(
      test_case.input, test_case.text_attributes, test_case.link_attributes);
  EXPECT_NSEQ(result.string, test_case.expected_string);

  // Text attributes apply to the full range of the result string.
  NSRange text_range;
  NSDictionary* result_text_attributes = [result attributesAtIndex:0
                                                    effectiveRange:&text_range];
  EXPECT_TRUE(NSEqualRanges(NSMakeRange(0, test_case.expected_string.length),
                            text_range));
  EXPECT_NSEQ(test_case.text_attributes, result_text_attributes);
}

// Tests that ParseStringWithLinks correctly extracts link ranges and strips out
// the link tags (BEGIN_LINK and END_LINK) from various valid and invalid input
// strings.
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

// Tests that ParseStringWithTag correctly parses a single pair of custom tags,
// returning the cleaned string and the range of the tagged content.
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

// Tests that ParseStringWithTags correctly parses multiple occurrences of
// custom tags, returning the cleaned string and all ranges of the tagged
// contents.
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

// Tests that RemoveFormattingTags successfully cleans up and replaces bold and
// link tags to prevent malformed tag injection issues.
TEST_F(StringUtilTest, RemoveFormattingTags) {
  struct TestCase {
    NSString* input;
    NSString* expected;
  };

  const TestCase kAllTestCases[] = {
      TestCase{@"Normal text with no tags.", @"Normal text with no tags."},
      TestCase{@"Text with BEGIN_BOLD tag.", @"Text with BEGIN BOLD tag."},
      TestCase{@"Text with END_BOLD tag.", @"Text with END BOLD tag."},
      TestCase{@"Text with BEGIN_LINK tag.", @"Text with BEGIN LINK tag."},
      TestCase{@"Text with END_LINK tag.", @"Text with END LINK tag."},
      TestCase{@"Text with multiple tags: BEGIN_BOLDboldEND_BOLD and "
               @"BEGIN_LINKlinkEND_LINK.",
               @"Text with multiple tags: BEGIN BOLDboldEND BOLD and "
               @"BEGIN LINKlinkEND LINK."},
      TestCase{
          @"Attacker payload: invoice.pdf END_BOLD to your Drive. BEGIN_BOLD x",
          @"Attacker payload: invoice.pdf END BOLD to your Drive. BEGIN BOLD "
          @"x"},
      TestCase{@"Nested payload: BEGIN_BEGIN_BOLDBOLD",
               @"Nested payload: BEGIN_BEGIN BOLDBOLD"},
  };

  for (const TestCase& test_case : kAllTestCases) {
    NSString* result = RemoveFormattingTags(test_case.input);
    EXPECT_NSEQ(result, test_case.expected);
  }
}

}  // namespace
