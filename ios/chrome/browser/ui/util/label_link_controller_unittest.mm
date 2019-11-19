// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/label_link_controller.h"

#include "ios/chrome/browser/ui/util/text_region_mapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A simple text region mapper that just returns the mapped bounds for
// any range.
@interface SimpleTextRegionMapper : NSObject<TextRegionMapper>
@end

@implementation SimpleTextRegionMapper {
  CGRect _bounds;
}

- (instancetype)initWithAttributedString:(NSAttributedString*)string
                                  bounds:(CGRect)bounds {
  if ((self = [super init])) {
    _bounds = bounds;
  }
  return self;
}

- (NSArray*)rectsForRange:(NSRange)range {
  return @[ [NSValue valueWithCGRect:_bounds] ];
}

@end

#define EXPECT_UICOLOR_EQ(A, B) EXPECT_NSEQ([A description], [B description])

namespace {

class LabelLinkControllerTest : public PlatformTest {
 protected:
  LabelLinkControllerTest() {
    label_container_ = [[UIView alloc] initWithFrame:CGRectZero];
    label_ = [[UILabel alloc] initWithFrame:CGRectZero];
    [label_container_ addSubview:label_];
  }

  void setLabelAttrString(NSString* str) {
    str_ = [[NSAttributedString alloc] initWithString:str];
    [label_ setAttributedText:str_];
  }

  void setLabelAttrStringWithAttr(NSString* str, NSString* attr, id value) {
    str_ = [[NSAttributedString alloc] initWithString:str
                                           attributes:@{attr : value}];
    [label_ setAttributedText:str_];
  }

  UIView* label_container_;
  UILabel* label_;
  NSAttributedString* str_;
};

TEST_F(LabelLinkControllerTest, TapTest) {
  setLabelAttrString(@"link tap test");
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(5, 3);  // "tap".

  GURL url = GURL("http://www.google.com");
  __block NSInteger taps = 0;
  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_
                                          action:^(const GURL& tappedUrl) {
                                            EXPECT_EQ(tappedUrl, url);
                                            taps++;
                                          }];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  [llc addLinkWithRange:linkRange url:url];
  NSArray* rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  CGRect rect = [rects[0] CGRectValue];

  CGPoint tapPoint = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));
  [llc tapLabelAtPoint:tapPoint];
  EXPECT_EQ(1, taps);
}

TEST_F(LabelLinkControllerTest, LinkColorTest) {
  setLabelAttrStringWithAttr(@"link color test", NSForegroundColorAttributeName,
                             [UIColor blueColor]);
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(5, 5);  // "color".

  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  [llc addLinkWithRange:linkRange url:GURL("http://www.google.com")];

  // That shouldn't have changed the label's attributes.
  NSDictionary* attrs =
      [[label_ attributedText] attributesAtIndex:linkRange.location
                                  effectiveRange:nullptr];
  EXPECT_UICOLOR_EQ(attrs[NSForegroundColorAttributeName], [UIColor blueColor]);

  [llc setLinkColor:[UIColor redColor]];
  // That should only change the attributes at the link location
  attrs = [[label_ attributedText] attributesAtIndex:linkRange.location
                                      effectiveRange:nullptr];
  EXPECT_UICOLOR_EQ(attrs[NSForegroundColorAttributeName], [UIColor redColor]);

  attrs = [[label_ attributedText] attributesAtIndex:0 effectiveRange:nullptr];
  EXPECT_UICOLOR_EQ(attrs[NSForegroundColorAttributeName], [UIColor blueColor]);

  [llc setLinkColor:[UIColor yellowColor]];
  // There shouldn't be a red foreground color attribute any more.
  attrs = [[label_ attributedText] attributesAtIndex:linkRange.location
                                      effectiveRange:nullptr];
  EXPECT_UICOLOR_EQ(attrs[NSForegroundColorAttributeName],
                    [UIColor yellowColor]);
}

TEST_F(LabelLinkControllerTest, LinkUnderlineTest) {
  setLabelAttrStringWithAttr(@"link underline test",
                             NSUnderlineStyleAttributeName,
                             @(NSUnderlineStyleSingle));
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(5, 9);  // "underline".

  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  [llc addLinkWithRange:linkRange url:GURL("http://www.google.com")];

  // That shouldn't have changed the label's attributes.
  NSDictionary* attrs =
      [[label_ attributedText] attributesAtIndex:linkRange.location
                                  effectiveRange:nullptr];
  EXPECT_NSEQ(attrs[NSUnderlineStyleAttributeName], @(NSUnderlineStyleSingle));

  [llc setLinkUnderlineStyle:NSUnderlineStyleDouble];
  // That should only change the attributes at the link location
  attrs = [[label_ attributedText] attributesAtIndex:linkRange.location
                                      effectiveRange:nullptr];
  EXPECT_NSEQ(attrs[NSUnderlineStyleAttributeName], @(NSUnderlineStyleDouble));

  attrs = [[label_ attributedText] attributesAtIndex:0 effectiveRange:nullptr];
  EXPECT_NSEQ(attrs[NSUnderlineStyleAttributeName], @(NSUnderlineStyleSingle));

  [llc setLinkUnderlineStyle:NSUnderlineStyleThick];
  // There shouldn't be a red foreground color attribute any more.
  attrs = [[label_ attributedText] attributesAtIndex:linkRange.location
                                      effectiveRange:nullptr];
  EXPECT_NSEQ(attrs[NSUnderlineStyleAttributeName], @(NSUnderlineStyleThick));

  [llc setLinkUnderlineStyle:NSUnderlineStyleNone];
  // Should see the underlying underline style.
  attrs = [[label_ attributedText] attributesAtIndex:linkRange.location
                                      effectiveRange:nullptr];
  EXPECT_NSEQ(attrs[NSUnderlineStyleAttributeName], @(NSUnderlineStyleSingle));
}

TEST_F(LabelLinkControllerTest, BoundsChangeTest) {
  setLabelAttrString(@"bounds change test");
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(7, 6);  // "change".

  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  GURL url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];

  NSArray* rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  NSValue* rect = rects[0];
  EXPECT_TRUE(CGRectContainsRect([rect CGRectValue], [label_ bounds]));

  // Change the label bounds, expect the links to be recomputed.
  // (SimpleTextRegionMapper just maps all ranges to the label bounds).
  CGRect newFrame = CGRectMake(0, 0, 200, 200);
  [label_ setFrame:newFrame];
  rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  NSValue* newRect = rects[0];
  EXPECT_NSNE(rect, newRect);
  EXPECT_TRUE(CGRectContainsRect([newRect CGRectValue], newFrame));
}

TEST_F(LabelLinkControllerTest, AttributedTextChangeTest) {
  setLabelAttrString(@"attributed text change test");
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(16, 6);  // "change".

  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  GURL url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];

  NSArray* rects = [llc tapRectsForURL:url];
  EXPECT_EQ(1UL, [rects count]);

  setLabelAttrString(@"crazy new attributed text");
  // Expect all links to be gone.
  rects = [llc tapRectsForURL:url];
  EXPECT_EQ(0UL, [rects count]);
}

TEST_F(LabelLinkControllerTest, TextChangeTest) {
  [label_ setText:@"text change test"];
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(5, 6);  // "change".

  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  [llc setTextMapperClass:[SimpleTextRegionMapper class]];
  GURL url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];

  NSArray* rects = [llc tapRectsForURL:url];
  EXPECT_EQ(1UL, [rects count]);

  [label_ setText:@"crazy new text"];
  // Expect all links to be gone.
  rects = [llc tapRectsForURL:url];
  EXPECT_EQ(0UL, [rects count]);
}

TEST_F(LabelLinkControllerTest, LabelStyleInitTest) {
  [label_ setText:@"style init test"];
  [label_ sizeToFit];
  NSRange linkRange = NSMakeRange(6, 5);  // "init".

  // Don't use an injected text mapper for this test.
  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  GURL url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];

  NSArray* rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);

  CGRect linkRect = [rects[0] CGRectValue];

  // Make a new label and controller with a very large font size, and
  // compute the same tap rect. It should be different from the rect computed
  // above.
  label_ = [[UILabel alloc] initWithFrame:CGRectZero];
  [label_ setText:@"style init test"];
  [label_ setFont:[UIFont systemFontOfSize:40]];
  [label_ sizeToFit];
  llc = [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];
  rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  CGRect newLinkRect = [rects[0] CGRectValue];

  EXPECT_NSNE(NSStringFromCGRect(linkRect), NSStringFromCGRect(newLinkRect));
}

TEST_F(LabelLinkControllerTest, LabelStylePropertyChangeTest) {
  [label_ setText:@"style change test"];
  // Choose a size large enough so that the full text can be laid out with both
  // fonts in this test.
  CGSize labelSize = CGSizeMake(400, 50);
  [label_ setFrame:{CGPointZero, labelSize}];

  NSRange linkRange = NSMakeRange(6, 6);  // "change".

  // Don't use an injected text mapper for this test.
  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  GURL url = GURL("http://www.google.com");
  [llc addLinkWithRange:linkRange url:url];

  NSArray* rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  NSValue* smallTextRect = rects[0];

  [label_ setFont:[UIFont systemFontOfSize:40]];
  // Expect all links to be changed.
  rects = [llc tapRectsForURL:url];
  ASSERT_EQ(1UL, [rects count]);
  EXPECT_NSNE(smallTextRect, rects[0]);
}

// Tests if the accessibility identifier is correctly set to the link button.
TEST_F(LabelLinkControllerTest, AccessibilityIdentifier) {
  [label_ setText:@"accessibility identifier"];
  // Choose a size large enough so that the full text can be laid out with both
  // fonts in this test.
  CGSize labelSize = CGSizeMake(400, 50);
  [label_ setFrame:{CGPointZero, labelSize}];

  NSRange linkRange = NSMakeRange(14, 4);  // "iden".

  // Don't use an injected text mapper for this test.
  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  GURL url = GURL("http://www.google.com");
  NSString* accessibilityID = @"GoogleLink";
  [llc addLinkWithRange:linkRange url:url accessibilityID:accessibilityID];

  UIView* linkButton = nil;
  NSMutableArray* views = [NSMutableArray arrayWithObject:label_.superview];
  while (views.count) {
    UIView* view = [views objectAtIndex:0];
    [views removeObjectAtIndex:0];
    [views addObjectsFromArray:view.subviews];
    if ([view.accessibilityIdentifier isEqualToString:accessibilityID]) {
      linkButton = view;
      break;
    }
  }
  EXPECT_NE(nil, linkButton);
}

TEST_F(LabelLinkControllerTest, LinkMaximumHeightTest) {
  NSMutableParagraphStyle* newStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  CGFloat lineHeight = 20;
  [newStyle setMinimumLineHeight:lineHeight];
  [newStyle setMaximumLineHeight:lineHeight];
  setLabelAttrStringWithAttr(@"first line test\nsecond line test.",
                             NSParagraphStyleAttributeName, newStyle);
  CGRect newFrame = CGRectMake(0, 0, 200, 100);
  [label_ setFrame:newFrame];
  [label_ setFont:[UIFont systemFontOfSize:14]];
  NSRange firstLinkRange = NSMakeRange(0, 5);    // "first".
  NSRange secondLinkRange = NSMakeRange(16, 6);  // "second".
  LabelLinkController* llc =
      [[LabelLinkController alloc] initWithLabel:label_ action:nullptr];
  GURL firsturl = GURL("http://www.google.com");
  GURL secondurl = GURL("http://www.cnn.com");

  // Test that a single link is expanded to 44.
  [llc addLinkWithRange:firstLinkRange url:firsturl];
  NSArray* rects = [llc buttonFrames];
  ASSERT_EQ(1UL, [rects count]);
  ASSERT_EQ(44.0, CGRectGetHeight([rects[0] CGRectValue]));

  // Test that multiple links overlap by only .25 of |lineHeight|.
  [llc addLinkWithRange:secondLinkRange url:secondurl];
  rects = [llc buttonFrames];
  ASSERT_EQ(2UL, [rects count]);
  CGRect intersection =
      CGRectIntersection([rects[0] CGRectValue], [rects[1] CGRectValue]);
  ASSERT_EQ(.25 * lineHeight, CGRectGetHeight(intersection));
}

}  // namespace
