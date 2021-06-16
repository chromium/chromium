// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/manual_text_framer.h"

#import <MaterialComponents/MaterialTypography.h>

#include "base/mac/foundation_util.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/util/core_text_util.h"
#import "ios/chrome/browser/ui/util/text_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
void ExpectNearPoint(CGFloat value1, CGFloat value2) {
  EXPECT_NEAR(value1, value2, 1);
}

class ManualTextFramerTest : public PlatformTest {
 protected:
  ManualTextFramerTest() {
    attributes_ = [[NSMutableDictionary alloc] init];
    string_ = [[NSMutableAttributedString alloc] init];
  }

  NSString* text() { return [string_ string]; }
  NSRange text_range() { return NSMakeRange(0, [string_ length]); }
  id<TextFrame> text_frame() { return static_cast<id<TextFrame>>(text_frame_); }

  void SetText(NSString* text) {
    DCHECK(text.length);
    [[string_ mutableString] setString:text];
  }

  void FrameTextInBounds(CGRect bounds) {
    ManualTextFramer* manual_framer =
        [[ManualTextFramer alloc] initWithString:string_ inBounds:bounds];
    [manual_framer frameText];
    id frame = [manual_framer textFrame];
    text_frame_ = frame;
  }

  UIFont* TypographyFontWithSize(CGFloat size) {
    return [[MDCTypography fontLoader] regularFontOfSize:size];
  }

  NSParagraphStyle* CreateParagraphStyle(CGFloat line_height,
                                         NSTextAlignment alignment,
                                         NSLineBreakMode line_break_mode) {
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    style.alignment = alignment;
    style.lineBreakMode = line_break_mode;
    style.minimumLineHeight = line_height;
    style.maximumLineHeight = line_height;
    return style;
  }

  NSMutableDictionary* attributes() { return attributes_; }

  void ApplyAttributesForRange(NSRange range) {
    [string_ setAttributes:attributes_ range:range];
  }

  void CheckForLineCountAndFramedRange(NSUInteger line_count,
                                       NSRange framed_range) {
    EXPECT_EQ(line_count, text_frame().lines.count);
    EXPECT_EQ(framed_range.location, text_frame().framedRange.location);
    EXPECT_EQ(framed_range.length, text_frame().framedRange.length);
  }

  NSMutableDictionary* attributes_;
  NSMutableAttributedString* string_;
  id<TextFrame> text_frame_;
};

// Tests that newline characters cause an attributed string to be laid out on
// multiple lines.
TEST_F(ManualTextFramerTest, NewlineTest) {
  SetText(@"line one\nline two\nline three");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 500);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(3, text_range());
}

// Tests that strings with no spaces fail correctly.
TEST_F(ManualTextFramerTest, NoSpacesText) {
  // "St. Mary's church in the hollow of the white hazel near the the rapid
  // whirlpool of Llantysilio of the red cave."
  SetText(
      @"Llanfair­pwllgwyngyll­gogery­chwyrn­drobwll­llan­tysilio­gogo­goch");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(16.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 200, 60);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(0, NSMakeRange(0, 0));
}

// Tests that unbreakable spaces are accounted for.
TEST_F(ManualTextFramerTest, UnbreakableSpace) {
  SetText(@"This is a long text with\u00A0unbreakable\u00A0spaces");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(16.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 200, 60);
  FrameTextInBounds(bounds);
  ASSERT_EQ(2UL, text_frame().lines.count);
  FramedLine* line = text_frame().lines[1];
  EXPECT_TRUE(NSEqualRanges(NSMakeRange(20, 23), line.stringRange));
}

// Tests that multiple newlines are accounted for.  Only the first three
// newlines should be added to |lines_|.
TEST_F(ManualTextFramerTest, MultipleNewlineTest) {
  SetText(@"\n\n\ntext");
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 60);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(3, NSMakeRange(0, 3));
}

// Tests that the framed range for text that will be rendered with ligatures is
// corresponds with the actual range of the text.
TEST_F(ManualTextFramerTest, LigatureTest) {
  SetText(@"fffi");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 20);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(1, text_range());
}

// Tests that ManualTextFramer correctly frames Å
TEST_F(ManualTextFramerTest, DiacriticTest) {
  SetText(@"A\u030A");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 20);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(1, text_range());
}

// String text, attributes, and bounds are chosen to match the "Terms of
// Service" text in WelcomeToChromeView, as the text is not properly framed by
// CTFrameSetter. http://crbug.com/537212
TEST_F(ManualTextFramerTest, TOSTextTest) {
  CGRect bounds = CGRectMake(0, 0, 300.0, 40.0);
  NSString* const kTOSLinkText = @"Terms of Service";
  NSString* const kTOSText =
      @"By using this application, you agree to Chrome’s Terms of Service.";
  SetText(kTOSText);
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentCenter, NSLineBreakByTruncatingTail);
  attributes()[NSForegroundColorAttributeName] = [UIColor blackColor];
  ApplyAttributesForRange(text_range());
  attributes()[NSForegroundColorAttributeName] = [UIColor blueColor];
  ApplyAttributesForRange([kTOSText rangeOfString:kTOSLinkText]);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(2, text_range());
}

// Tests that the origin of a left-aligned single line is correct.
TEST_F(ManualTextFramerTest, SimpleOriginTest) {
  SetText(@"test");
  UIFont* font = TypographyFontWithSize(14.0);
  attributes()[NSFontAttributeName] = font;
  CGFloat line_height = 20.0;
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      line_height, NSTextAlignmentLeft, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 21);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(1, text_range());
  FramedLine* line = [text_frame().lines firstObject];
  EXPECT_EQ(0, line.origin.x);
  ExpectNearPoint(CGRectGetHeight(bounds) - line_height - font.descender,
                  line.origin.y);
}

// Tests that lines that are laid out in RTL are right aligned.
TEST_F(ManualTextFramerTest, OriginRTLTest) {
  if (@available(iOS 15, *)) {
    // TODO(crbug.com/1220239): Fix for TextInput2 changes in iOS15.
    return;
  }
  SetText(@"\u0641\u064e\u0628\u064e\u0642\u064e\u064a\u0652\u062a\u064f\u0020"
          @"\u0645\u064f\u062a\u064e\u0627\u0628\u0650\u0639\u064e\u0627\u064b"
          @"\u0020\u0028\u0634\u064f\u063a\u0652\u0644\u0650\u064a\u0029\u0020"
          @"\u0644\u064e\u0639\u064e\u0644\u064e\u0643\u0650\u0020\u062a\u064e"
          @"\u062a\u064e\u0639\u064e\u0644\u0651\u064e\u0645\u064e\u0020\u0627"
          @"\u0644\u062d\u0650\u0631\u0652\u0635\u064e\u0020\u0639\u064e\u0644"
          @"\u064e\u0649\u0020\u0627\u0644\u0648\u064e\u0642\u0652\u062a\u0650"
          @"\u0020\u002e\u0020\u0641\u064e\u0627\u0644\u062d\u064e\u064a\u064e"
          @"\u0627\u0629\u064f");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  // The bounds width is chosen so that the RTL string above can be completely
  // laid out into three lines.
  CGRect bounds = CGRectMake(0, 0, 115.0, 60.0);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(3, text_range());
  for (FramedLine* line in text_frame().lines) {
    ExpectNearPoint(
        CGRectGetMaxX(bounds),
        line.origin.x + core_text_util::GetTrimmedLineWidth(line.line));
  }
}

TEST_F(ManualTextFramerTest, CJKLineBreakTest) {
  // Example from our strings. Framer will put “触摸搜索” on one line, and then
  // fail to frame the second.
  // clang-format off
  SetText(@"“触摸搜索”会将所选字词和当前页面（作为上下文）一起发送给 Google 搜索。"
          @"您可以在设置中停用此功能。");
  // clang-format on
  attributes()[NSFontAttributeName] = TypographyFontWithSize(16.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      16 * 1.15, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 300.0, 65.0);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(3, NSMakeRange(0, 53));

  // Example without any space-ish characters:
  SetText(@"会将所选字词和当前页面（作为上下文）一起发送给Google搜索。"
          @"您可以在设置中停用此功能。");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(16.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      16 * 1.15, NSTextAlignmentNatural, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(2, NSMakeRange(0, 45));
}

// Tests that paragraphs with NSTextAlignmentCenter are actually centered.
TEST_F(ManualTextFramerTest, CenterAlignedTest) {
  SetText(@"xxxx\nyyy\nwww");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentCenter, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 200.0, 60.0);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(3, text_range());
  for (FramedLine* line in text_frame().lines) {
    ExpectNearPoint(CGRectGetMidX(bounds) -
                        0.5 * core_text_util::GetTrimmedLineWidth(line.line),
                    line.origin.x);
  }
}

// Tests that words with a large line height will not be framed if they don't
// fit in the bounding height.
TEST_F(ManualTextFramerTest, LargeLineHeightTest) {
  SetText(@"the last word is very LARGE");
  attributes()[NSFontAttributeName] = TypographyFontWithSize(14.0);
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      20.0, NSTextAlignmentCenter, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(text_range());
  attributes()[NSParagraphStyleAttributeName] = CreateParagraphStyle(
      500.0, NSTextAlignmentCenter, NSLineBreakByWordWrapping);
  ApplyAttributesForRange(NSMakeRange(22, 5));  // "LARGE"
  CGRect bounds = CGRectMake(0, 0, 500, 20.0);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(1, NSMakeRange(0, 22));
}

// Tests a preexisting error condition in which a BiDi string containing Arabic
// is correctly laid out (crbug.com/584549).
TEST_F(ManualTextFramerTest, RTLTest) {
  SetText(@"\u0642\u062F\u0020\u064A\u0633\u062A\u062E\u062F\u0645\u0020\u0047"
           "\u006F\u006F\u0067\u006C\u0065\u0020\u0043\u0068\u0072\u006F\u006D"
           "\u0065\u0020\u062E\u062F\u0645\u0627\u062A\u0020\u0627\u0644\u0648"
           "\u064A\u0628\u0020\u0644\u062A\u062D\u0633\u064A\u0646\u0020\u062A"
           "\u062C\u0631\u0628\u0629\u0020\u0627\u0644\u062A\u0635\u0641\u062D"
           "\u002E\u0020\u0648\u064A\u0645\u0643\u0646\u0643\u0020\u0628\u0634"
           "\u0643\u0644\u0020\u0627\u062E\u062A\u064A\u0627\u0631\u064A\u0020"
           "\u062A\u0639\u0637\u064A\u0644\u0020\u0647\u0630\u0647\u0020\u0627"
           "\u0644\u062E\u062F\u0645\u0627\u062A\u002E");
  attributes()[NSFontAttributeName] = [UIFont systemFontOfSize:20.0];
  ApplyAttributesForRange(text_range());
  CGRect bounds = CGRectMake(0, 0, 500, 100);
  FrameTextInBounds(bounds);
  CheckForLineCountAndFramedRange(2, text_range());
}

}  // namespace
