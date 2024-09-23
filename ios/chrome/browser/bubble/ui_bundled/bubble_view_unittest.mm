// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_unittest_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface BubbleViewDelegateTest : NSObject <BubbleViewDelegate>

- (instancetype)init;

@property(nonatomic) int tapCounter;

@end

@implementation BubbleViewDelegateTest

- (instancetype)init {
  self = [super init];
  if (self) {
    _tapCounter = 0;
  }
  return self;
}

- (void)didTapCloseButton {
  _tapCounter += 1;
}

- (void)didTapSnoozeButton {
  _tapCounter += 1;
}

@end

// Fixture to test BubbleView.
class BubbleViewTest : public PlatformTest {
 public:
  BubbleViewTest()
      : max_size_(CGSizeMake(500.0f, 500.0f)),
        arrow_direction_(BubbleArrowDirectionUp),
        alignment_(BubbleAlignmentCenter),
        alignment_offset_(35.0f),
        short_text_(@"I"),
        long_text_(@"Lorem ipsum dolor sit amet, consectetur adipiscing elit."),
        text_alignment_(NSTextAlignmentNatural) {}

 protected:
  // The maximum size of the bubble.
  const CGSize max_size_;
  // The direction that the bubble's arrow points.
  const BubbleArrowDirection arrow_direction_;
  // The alignment of the bubble's arrow relative to the rest of the bubble.
  const BubbleAlignment alignment_;
  // Distance between the arrow's centerX and the (leading or trailing) edge of
  // the bubble, depending on the BubbleAlignment. If BubbleAlignment is center,
  // then `alignmentOffset` is ignored.
  const CGFloat alignment_offset_;
  // Text that is shorter than the minimum line width.
  NSString* short_text_;
  // Text that is longer than the maximum line width. It should wrap onto
  // multiple lines.
  NSString* long_text_;
  // Text alignment for bubble's title, text and snooze button.
  NSTextAlignment text_alignment_;
};

// Test `sizeThatFits` given short text.
TEST_F(BubbleViewTest, BubbleSizeShortText) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:short_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_];
  CGSize bubble_size = [bubble sizeThatFits:max_size_];
  // Since the label is shorter than the minimum line width, expect the bubble
  // to be the minimum width and accommodate one line of text.
  EXPECT_NEAR(68.0f, bubble_size.width, 1.0f);
  EXPECT_NEAR(67.0f, bubble_size.height, 1.0f);
}

// Test that the accessibility label matches the display text.
TEST_F(BubbleViewTest, Accessibility) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  // Add the bubble view to the view hierarchy.
  [superview addSubview:bubble];
  EXPECT_NSEQ(long_text_, bubble.accessibilityValue);
}

// Tests that the close button is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, CloseButtonIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:text_alignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* close_button = GetCloseButtonFromBubbleView(bubble);
  ASSERT_FALSE(close_button);
}

// Tests the close button action and its presence.
TEST_F(BubbleViewTest, CloseButtonActionAndPresent) {
  BubbleViewDelegateTest* delegate = [[BubbleViewDelegateTest alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:YES
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:text_alignment_
                                               delegate:delegate];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* close_button = GetCloseButtonFromBubbleView(bubble);
  ASSERT_TRUE(close_button);
  // Tests close button action.
  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate.tapCounter, 1);
}

// Tests that the title is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, TitleIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* title_label = GetTitleLabelFromBubbleView(bubble);
  ASSERT_FALSE(title_label);
}

// Tests that the title is present and correct.
TEST_F(BubbleViewTest, TitleIsPresentAndCorrect) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:short_text_
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:text_alignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* title_label = GetTitleLabelFromBubbleView(bubble);
  ASSERT_TRUE(title_label);
  ASSERT_EQ(title_label.text, short_text_);
}

// Tests that the title is aligned correctly.
TEST_F(BubbleViewTest, TitleIsAligned) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:short_text_
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:NSTextAlignmentNatural
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* title_label = GetTitleLabelFromBubbleView(bubble);
  ASSERT_TRUE(title_label);
  ASSERT_EQ(title_label.textAlignment, NSTextAlignmentNatural);
}

// Tests that the image is not showed when the image is empty.
TEST_F(BubbleViewTest, ImageIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIImageView* image_view = GetImageViewFromBubbleView(bubble);
  ASSERT_FALSE(image_view);
}

// Tests that the image is present and correct.
TEST_F(BubbleViewTest, ImageIsPresentAndCorrect) {
  UIImage* testImage = [[UIImage alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:testImage
                                      showsSnoozeButton:NO
                                          textAlignment:text_alignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIImageView* image_view = GetImageViewFromBubbleView(bubble);
  ASSERT_TRUE(image_view);
  EXPECT_EQ(image_view.image, testImage);
}

// Tests that the snooze button is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, SnoozeButtonIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:text_alignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* snooze_button = GetSnoozeButtonFromBubbleView(bubble);
  ASSERT_FALSE(snooze_button);
}

// Tests the snooze button action and its presence.
TEST_F(BubbleViewTest, SnoozeButtonActionAndPresent) {
  BubbleViewDelegateTest* delegate = [[BubbleViewDelegateTest alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:YES
                                          textAlignment:text_alignment_
                                               delegate:delegate];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* snooze_button = GetSnoozeButtonFromBubbleView(bubble);
  ASSERT_TRUE(snooze_button);
  // Tests snooze button action.
  [snooze_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate.tapCounter, 1);
}

// Tests the arrow view is aligned properly with BubbleAlignmentTopOrLeading.
TEST_F(BubbleViewTest, ArrowViewLeadingAligned) {
  BubbleView* bubble =
      [[BubbleView alloc] initWithText:long_text_
                        arrowDirection:arrow_direction_
                             alignment:BubbleAlignmentTopOrLeading];
  bubble.alignmentOffset = alignment_offset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrow_view = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrow_view);
  // The center of the arrow must be at a distance of `alignment_offset_` to the
  // leading edge of the bubble.
  EXPECT_EQ(CGRectGetMidX(arrow_view.frame), alignment_offset_);
}

// Tests the arrow view is aligned properly with BubbleAlignmentCenter.
TEST_F(BubbleViewTest, ArrowViewCenterAligned) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:long_text_
                                         arrowDirection:arrow_direction_
                                              alignment:BubbleAlignmentCenter];
  bubble.alignmentOffset = alignment_offset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrow_view = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrow_view);
  // `alignmentOffset` should be ignored with BubbleAlignmentCenter.
  EXPECT_EQ(CGRectGetMidX(arrow_view.frame), CGRectGetMidX(bubble.frame));
}

// Tests the arrow view is aligned properly with
// BubbleAlignmentBottomOrTrailing.
TEST_F(BubbleViewTest, ArrowViewTrailingAligned) {
  BubbleView* bubble =
      [[BubbleView alloc] initWithText:long_text_
                        arrowDirection:arrow_direction_
                             alignment:BubbleAlignmentBottomOrTrailing];
  bubble.alignmentOffset = alignment_offset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrow_view = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrow_view);
  // The center of the arrow must be at a distance of `alignment_offset_` to the
  // trailing edge of the bubble.
  EXPECT_EQ(CGRectGetMidX(arrow_view.frame),
            bubble.frame.size.width - alignment_offset_);
}
