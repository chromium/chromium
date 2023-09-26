// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"
#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_unittest_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fixture to test BubbleViewController.
class BubbleViewControllerTest : public PlatformTest {
 public:
  BubbleViewControllerTest()
      : text_(@"Text"),
        titleText_(@"Title"),
        image_([[UIImage alloc] init]),
        arrowDirection_(BubbleArrowDirectionUp),
        alignment_(BubbleAlignmentTopOrLeading) {}

 protected:
  // Text for the bubble view.
  NSString* text_;
  // Title for the bubble view.
  NSString* titleText_;
  // Image for the bubble view.
  UIImage* image_;
  // The direction that the bubble's arrow points.
  const BubbleArrowDirection arrowDirection_;
  // The alignment of the bubble's arrow relative to the rest of the bubble.
  const BubbleAlignment alignment_;

  // Tests that `bubbleViewController`'s bubbleView contains the expected
  // subviews.
  void ExpectBubbleViewContent(BubbleViewController* bubbleViewController,
                               BOOL expectCloseButton,
                               BOOL expectTitle,
                               BOOL expectImage,
                               BOOL expectSnoozeButton) {
    BubbleView* bubbleView =
        base::apple::ObjCCastStrict<BubbleView>(bubbleViewController.view);
    EXPECT_TRUE(bubbleView);
    UIView* closeButton = GetCloseButtonFromBubbleView(bubbleView);
    UIView* titleView = GetTitleLabelFromBubbleView(bubbleView);
    UIView* imageView = GetImageViewFromBubbleView(bubbleView);
    UIView* snoozeButton = GetSnoozeButtonFromBubbleView(bubbleView);
    EXPECT_EQ(expectCloseButton, static_cast<bool>(closeButton));
    EXPECT_EQ(expectTitle, static_cast<bool>(titleView));
    EXPECT_EQ(expectImage, static_cast<bool>(imageView));
    EXPECT_EQ(expectSnoozeButton, static_cast<bool>(snoozeButton));
  }
};

// Tests that with BubbleViewTypeDefault, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeDefaultContent) {
  BubbleViewController* bubbleViewController =
      [[BubbleViewController alloc] initWithText:text_
                                           title:titleText_
                                           image:image_
                                  arrowDirection:arrowDirection_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeDefault
                                        delegate:nil];
  ExpectBubbleViewContent(bubbleViewController, false, false, false, false);
}

// Tests that with BubbleViewTypeWithClose, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeWithCloseContent) {
  BubbleViewController* bubbleViewController =
      [[BubbleViewController alloc] initWithText:text_
                                           title:titleText_
                                           image:image_
                                  arrowDirection:arrowDirection_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeWithClose
                                        delegate:nil];
  ExpectBubbleViewContent(bubbleViewController, true, false, false, false);
}

// Tests that with BubbleViewTypeRich, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeRichContent) {
  BubbleViewController* bubbleViewController =
      [[BubbleViewController alloc] initWithText:text_
                                           title:titleText_
                                           image:image_
                                  arrowDirection:arrowDirection_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRich
                                        delegate:nil];
  ExpectBubbleViewContent(bubbleViewController, true, true, true, false);
}

// Tests that with BubbleViewTypeRichWithSnooze, bubble view contains the
// expected subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeRichWithSnoozeContent) {
  BubbleViewController* bubbleViewController =
      [[BubbleViewController alloc] initWithText:text_
                                           title:titleText_
                                           image:image_
                                  arrowDirection:arrowDirection_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRichWithSnooze
                                        delegate:nil];
  ExpectBubbleViewContent(bubbleViewController, true, true, true, true);
}
