// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/string_util.h"

#import <UIKit/UIKit.h>

#include "base/ios/ns_range.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using StringUtilTest = PlatformTest;

TEST_F(StringUtilTest, ParseStringWithLink) {
  struct TestCase {
    NSString* input;
    StringWithTag expected;
  };

  const TestCase kAllTestCases[] = {
      TestCase{
          @"Text without link.",
          StringWithTag{
              @"Text without link.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{
          @"Text with empty link BEGIN_LINK END_LINK.",
          StringWithTag{
              @"Text with empty link .",
              NSRange{21, 0},
          },
      },
      TestCase{
          @"Text with BEGIN_LINK and no end link.",
          StringWithTag{
              @"Text with BEGIN_LINK and no end link.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{
          @"Text with no begin link and END_LINK.",
          StringWithTag{
              @"Text with no begin link and END_LINK.",
              NSRange{NSNotFound, 0},
          },
      },
      TestCase{@"Text with END_LINK before BEGIN_LINK.",
               StringWithTag{
                   @"Text with END_LINK before BEGIN_LINK.",
                   NSRange{NSNotFound, 0},
               }},
      TestCase{
          @"Text with valid BEGIN_LINK link END_LINK and spaces.",
          StringWithTag{
              @"Text with valid link and spaces.",
              NSRange{16, 4},
          },
      },
      TestCase{
          @"Text with valid BEGIN_LINKlinkEND_LINK and no spaces.",
          StringWithTag{
              @"Text with valid link and no spaces.",
              NSRange{16, 4},
          },
      },
  };

  for (const TestCase& test_case : kAllTestCases) {
    const StringWithTag result = ParseStringWithLink(test_case.input);
    EXPECT_NSEQ(result.string, test_case.expected.string);
    EXPECT_EQ(result.range, test_case.expected.range);
  }
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

TEST_F(StringUtilTest, CleanNSStringForDisplay) {
  NSArray* const all_test_data = @[
    @{
      @"input" : @"Clean String",
      @"remove_graphic_chars" : @NO,
      @"expected" : @"Clean String"
    },
    @{
      @"input" : @"  \t String with leading and trailing  whitespaces   ",
      @"remove_graphic_chars" : @NO,
      @"expected" : @"String with leading and trailing whitespaces"
    },
    @{
      @"input" : @"  \n\n\r String with \n\n\r\n\r newline characters \n\n\r",
      @"remove_graphic_chars" : @NO,
      @"expected" : @"String with newline characters"
    },
    @{
      @"input" : @"String with an   arrow ⟰ that remains intact",
      @"remove_graphic_chars" : @NO,
      @"expected" : @"String with an arrow ⟰ that remains intact"
    },
    @{
      @"input" : @"String with an   arrow ⟰ that gets cleaned up",
      @"remove_graphic_chars" : @YES,
      @"expected" : @"String with an arrow that gets cleaned up"
    },
  ];

  for (NSDictionary* test_data : all_test_data) {
    NSString* input_text = test_data[@"input"];
    NSString* expected_text = test_data[@"expected"];
    BOOL remove_graphic_chars = [test_data[@"remove_graphic_chars"] boolValue];

    EXPECT_NSEQ(expected_text,
                CleanNSStringForDisplay(input_text, remove_graphic_chars));
  }
}

TEST_F(StringUtilTest, SubstringOfWidth) {
  // returns nil for zero-length strings
  EXPECT_NSEQ(SubstringOfWidth(@"", @{}, 100, NO), nil);
  EXPECT_NSEQ(SubstringOfWidth(nil, @{}, 100, NO), nil);

  // This font should always exist
  UIFont* sys_font = [UIFont systemFontOfSize:18.0f];
  NSDictionary* attributes = @{NSFontAttributeName : sys_font};

  EXPECT_NSEQ(SubstringOfWidth(@"asdf", attributes, 100, NO), @"asdf");

  // construct some string of known lengths
  NSString* leading = @"some text";
  NSString* trailing = @"some other text";
  NSString* mid = @"some text for the method to do some actual work";
  NSString* long_string =
      [[leading stringByAppendingString:mid] stringByAppendingString:trailing];

  CGFloat leading_width = [leading sizeWithAttributes:attributes].width;
  CGFloat trailing_width = [trailing sizeWithAttributes:attributes].width;

  NSString* leading_calculated =
      SubstringOfWidth(long_string, attributes, leading_width, NO);
  EXPECT_NSEQ(leading, leading_calculated);

  NSString* trailing_calculated =
      SubstringOfWidth(long_string, attributes, trailing_width, YES);
  EXPECT_NSEQ(trailing, trailing_calculated);
}
}  // namespace
