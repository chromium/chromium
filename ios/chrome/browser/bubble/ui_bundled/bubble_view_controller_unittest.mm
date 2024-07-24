// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_unittest_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fixture to test BubbleViewController.
class BubbleViewControllerTest : public PlatformTest {
 public:
  BubbleViewControllerTest()
      : text_(@"Text"),
        title_text_(@"Title"),
        image_([[UIImage alloc] init]),
        arrow_direction_(BubbleArrowDirectionUp),
        alignment_(BubbleAlignmentTopOrLeading) {}

 protected:
  // Text for the bubble view.
  NSString* text_;
  // Title for the bubble view.
  NSString* title_text_;
  // Image for the bubble view.
  UIImage* image_;
  // The direction that the bubble's arrow points.
  const BubbleArrowDirection arrow_direction_;
  // The alignment of the bubble's arrow relative to the rest of the bubble.
  const BubbleAlignment alignment_;

  // Tests that `bubble_view_controller`'s bubbleView contains the expected
  // subviews.
  void ExpectBubbleViewContent(BubbleViewController* bubble_view_controller,
                               BOOL expect_close_button,
                               BOOL expect_title,
                               BOOL expect_image,
                               BOOL expect_snooze_button) {
    BubbleView* bubbleView =
        base::apple::ObjCCastStrict<BubbleView>(bubble_view_controller.view);
    EXPECT_TRUE(bubbleView);
    UIView* closeButton = GetCloseButtonFromBubbleView(bubbleView);
    UIView* titleView = GetTitleLabelFromBubbleView(bubbleView);
    UIView* imageView = GetImageViewFromBubbleView(bubbleView);
    UIView* snoozeButton = GetSnoozeButtonFromBubbleView(bubbleView);
    EXPECT_EQ(expect_close_button, static_cast<bool>(closeButton));
    EXPECT_EQ(expect_title, static_cast<bool>(titleView));
    EXPECT_EQ(expect_image, static_cast<bool>(imageView));
    EXPECT_EQ(expect_snooze_button, static_cast<bool>(snoozeButton));
  }

  base::test::ScopedFeatureList feature_list_;
};

// Tests that with BubbleViewTypeDefault, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeDefaultContent) {
  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeDefault
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, false, false, false, false);
}

// Tests that with BubbleViewTypeWithClose, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeWithCloseContent) {
  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeWithClose
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, true, false, false, false);
}

// Tests that with BubbleViewTypeRich, bubble view contains the expected
// subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeRichContent) {
  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRich
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, false, true, true, false);
}

// Tests that with BubbleViewTypeRichWithSnooze, bubble view contains the
// expected subviews.
TEST_F(BubbleViewControllerTest, BubbleTypeRichWithSnoozeContent) {
  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRichWithSnooze
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, false, true, true, true);
}

// Tests that with kRichBubbleWithoutImage enabled, BubbleViewTypeRich's bubble
// view contains the expected subviews (no image).
TEST_F(BubbleViewControllerTest, BubbleViewTypeRichWithoutImageContent) {
  feature_list_.InitWithFeatures({kRichBubbleWithoutImage}, {});

  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRich
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, false, true, false, false);
}

// Tests that with kRichBubbleWithoutImage enabled, BubbleTypeRichWithSnooze's
// bubble view contains the expected subviews (no image).
TEST_F(BubbleViewControllerTest, BubbleTypeRichWithSnoozeWithoutImageContent) {
  feature_list_.InitWithFeatures({kRichBubbleWithoutImage}, {});

  BubbleViewController* bubble_view_controller =
      [[BubbleViewController alloc] initWithText:text_
                                           title:title_text_
                                           image:image_
                                  arrowDirection:arrow_direction_
                                       alignment:alignment_
                                  bubbleViewType:BubbleViewTypeRichWithSnooze
                                        delegate:nil];
  ExpectBubbleViewContent(bubble_view_controller, false, true, false, true);
}
