// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_constants.h"
#import "ios/chrome/browser/ui/bubble/bubble_unittest_util.h"
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
      : maxSize_(CGSizeMake(500.0f, 500.0f)),
        arrowDirection_(BubbleArrowDirectionUp),
        alignment_(BubbleAlignmentCenter),
        alignmentOffset_(35.0f),
        shortText_(@"I"),
        longText_(@"Lorem ipsum dolor sit amet, consectetur adipiscing elit."),
        textAlignment_(NSTextAlignmentNatural) {}

 protected:
  // The maximum size of the bubble.
  const CGSize maxSize_;
  // The direction that the bubble's arrow points.
  const BubbleArrowDirection arrowDirection_;
  // The alignment of the bubble's arrow relative to the rest of the bubble.
  const BubbleAlignment alignment_;
  // Distance between the arrow's centerX and the (leading or trailing) edge of
  // the bubble, depending on the BubbleAlignment. If BubbleAlignment is center,
  // then `alignmentOffset` is ignored.
  const CGFloat alignmentOffset_;
  // Text that is shorter than the minimum line width.
  NSString* shortText_;
  // Text that is longer than the maximum line width. It should wrap onto
  // multiple lines.
  NSString* longText_;
  // Text alignment for bubble's title, text and snooze button.
  NSTextAlignment textAlignment_;
};

// Test `sizeThatFits` given short text.
TEST_F(BubbleViewTest, BubbleSizeShortText) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:shortText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_];
  CGSize bubbleSize = [bubble sizeThatFits:maxSize_];
  // Since the label is shorter than the minimum line width, expect the bubble
  // to be the minimum width and accommodate one line of text.
  EXPECT_NEAR(68.0f, bubbleSize.width, 1.0f);
  EXPECT_NEAR(67.0f, bubbleSize.height, 1.0f);
}

// Test that the accessibility label matches the display text.
TEST_F(BubbleViewTest, Accessibility) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  // Add the bubble view to the view hierarchy.
  [superview addSubview:bubble];
  EXPECT_NSEQ(longText_, bubble.accessibilityValue);
}

// Tests that the close button is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, CloseButtonIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:textAlignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* closeButton = GetCloseButtonFromBubbleView(bubble);
  ASSERT_FALSE(closeButton);
}

// Tests the close button action and its presence.
TEST_F(BubbleViewTest, CloseButtonActionAndPresent) {
  BubbleViewDelegateTest* delegate = [[BubbleViewDelegateTest alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:YES
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:textAlignment_
                                               delegate:delegate];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* closeButton = GetCloseButtonFromBubbleView(bubble);
  ASSERT_TRUE(closeButton);
  // Tests close button action.
  [closeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate.tapCounter, 1);
}

// Tests that the title is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, TitleIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* titleLabel = GetTitleLabelFromBubbleView(bubble);
  ASSERT_FALSE(titleLabel);
}

// Tests that the title is present and correct.
TEST_F(BubbleViewTest, TitleIsPresentAndCorrect) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:shortText_
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:textAlignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* titleLabel = GetTitleLabelFromBubbleView(bubble);
  ASSERT_TRUE(titleLabel);
  ASSERT_EQ(titleLabel.text, shortText_);
}

// Tests that the title is aligned correctly.
TEST_F(BubbleViewTest, TitleIsAligned) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:shortText_
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:NSTextAlignmentNatural
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UILabel* titleLabel = GetTitleLabelFromBubbleView(bubble);
  ASSERT_TRUE(titleLabel);
  ASSERT_EQ(titleLabel.textAlignment, NSTextAlignmentNatural);
}

// Tests that the image is not showed when the image is empty.
TEST_F(BubbleViewTest, ImageIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIImageView* imageView = GetImageViewFromBubbleView(bubble);
  ASSERT_FALSE(imageView);
}

// Tests that the image is present and correct.
TEST_F(BubbleViewTest, ImageIsPresentAndCorrect) {
  UIImage* testImage = [[UIImage alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:testImage
                                      showsSnoozeButton:NO
                                          textAlignment:textAlignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIImageView* imageView = GetImageViewFromBubbleView(bubble);
  ASSERT_TRUE(imageView);
  EXPECT_EQ(imageView.image, testImage);
}

// Tests that the snooze button is not showed when the option is set to hidden.
TEST_F(BubbleViewTest, SnoozeButtonIsNotPresent) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:NO
                                          textAlignment:textAlignment_
                                               delegate:nil];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* snoozeButton = GetSnoozeButtonFromBubbleView(bubble);
  ASSERT_FALSE(snoozeButton);
}

// Tests the snooze button action and its presence.
TEST_F(BubbleViewTest, SnoozeButtonActionAndPresent) {
  BubbleViewDelegateTest* delegate = [[BubbleViewDelegateTest alloc] init];
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:alignment_
                                       showsCloseButton:NO
                                                  title:nil
                                                  image:nil
                                      showsSnoozeButton:YES
                                          textAlignment:textAlignment_
                                               delegate:delegate];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  UIButton* snoozeButton = GetSnoozeButtonFromBubbleView(bubble);
  ASSERT_TRUE(snoozeButton);
  // Tests snooze button action.
  [snoozeButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(delegate.tapCounter, 1);
}

// Tests the arrow view is aligned properly with BubbleAlignmentTopOrLeading.
TEST_F(BubbleViewTest, ArrowViewLeadingAligned) {
  BubbleView* bubble =
      [[BubbleView alloc] initWithText:longText_
                        arrowDirection:arrowDirection_
                             alignment:BubbleAlignmentTopOrLeading];
  bubble.alignmentOffset = alignmentOffset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrowView = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrowView);
  // The center of the arrow must be at a distance of `alignmentOffset_` to the
  // leading edge of the bubble.
  EXPECT_EQ(CGRectGetMidX(arrowView.frame), alignmentOffset_);
}

// Tests the arrow view is aligned properly with BubbleAlignmentCenter.
TEST_F(BubbleViewTest, ArrowViewCenterAligned) {
  BubbleView* bubble = [[BubbleView alloc] initWithText:longText_
                                         arrowDirection:arrowDirection_
                                              alignment:BubbleAlignmentCenter];
  bubble.alignmentOffset = alignmentOffset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrowView = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrowView);
  // `alignmentOffset` should be ignored with BubbleAlignmentCenter.
  EXPECT_EQ(CGRectGetMidX(arrowView.frame), CGRectGetMidX(bubble.frame));
}

// Tests the arrow view is aligned properly with
// BubbleAlignmentBottomOrTrailing.
TEST_F(BubbleViewTest, ArrowViewTrailingAligned) {
  BubbleView* bubble =
      [[BubbleView alloc] initWithText:longText_
                        arrowDirection:arrowDirection_
                             alignment:BubbleAlignmentBottomOrTrailing];
  bubble.alignmentOffset = alignmentOffset_;
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:bubble];
  [bubble layoutIfNeeded];
  UIView* arrowView = GetArrowViewFromBubbleView(bubble);
  ASSERT_TRUE(arrowView);
  // The center of the arrow must be at a distance of `alignmentOffset_` to the
  // trailing edge of the bubble.
  EXPECT_EQ(CGRectGetMidX(arrowView.frame),
            bubble.frame.size.width - alignmentOffset_);
}
