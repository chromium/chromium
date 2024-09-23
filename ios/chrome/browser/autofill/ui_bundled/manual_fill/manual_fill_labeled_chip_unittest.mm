// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_labeled_chip.h"

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ManualFillLabeledChipiOSTest = PlatformTest;

namespace {

NSString* TOP_LABEL_TEXT = @"Label Text";
NSString* BOTTOM_BUTTON_TEXT_0 = @"Button Text";
NSString* BOTTOM_LABEL_TEXT_1 = @"/";
NSString* BOTTOM_BUTTON_TEXT_2 = @"Next Button Text";
int TOP_LABEL_INDEX = 0;
int BOTTOM_BUTTONS_INDEX = 1;
int BOTTOM_BUTTON_0_INDEX = 0;
int BOTTOM_LABEL_1_INDEX = 1;
int BOTTOM_BUTTON_2_INDEX = 2;

// Returns the title of the given `button`.
NSString* ButtonTitle(UIButton* button) {
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    UIButtonConfiguration* button_configuration = button.configuration;
    return button_configuration.attributedTitle.string;
  }

  return button.currentTitle;
}

}  // namespace

// Tests that a labeled chip is successfully created.
TEST_F(ManualFillLabeledChipiOSTest, Creation_SingleChip) {
  // Create labeled chip.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];

  // Confirm there are 2 subviews: a UILabel and a UIButton.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_EQ(chipSubviews.count, 2u);
  EXPECT_TRUE([chipSubviews[TOP_LABEL_INDEX] isKindOfClass:[UILabel class]]);
  EXPECT_TRUE(
      [chipSubviews[BOTTOM_BUTTONS_INDEX] isKindOfClass:[UIButton class]]);
}

// Tests that a labeled chip for an expiration date is successfully created.
TEST_F(ManualFillLabeledChipiOSTest, Creation_ExpirationDateChip) {
  // Create labeled chip with a UIButton, a UILabel and another UIButton.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initExpirationDateChipWithTarget:0
                                                        monthSelector:0
                                                         yearSelector:0];

  // Confirm there are 2 subviews: UILabel and UIStackView.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_EQ(chipSubviews.count, 2u);
  EXPECT_TRUE([chipSubviews[TOP_LABEL_INDEX] isKindOfClass:[UILabel class]]);
  EXPECT_TRUE(
      [chipSubviews[BOTTOM_BUTTONS_INDEX] isKindOfClass:[UIStackView class]]);

  // Confirm the bottom UIStackView only holds 3 subviews: a UIButton, a UILabel
  // and another UIButton.
  NSArray<UIView*>* buttonStackViewSubviews =
      ((UIStackView*)chipSubviews[BOTTOM_BUTTONS_INDEX]).arrangedSubviews;
  EXPECT_EQ(buttonStackViewSubviews.count, 3u);
  EXPECT_TRUE([buttonStackViewSubviews[BOTTOM_BUTTON_0_INDEX]
      isKindOfClass:[UIButton class]]);
  EXPECT_TRUE([buttonStackViewSubviews[BOTTOM_LABEL_1_INDEX]
      isKindOfClass:[UILabel class]]);
  EXPECT_TRUE([buttonStackViewSubviews[BOTTOM_BUTTON_2_INDEX]
      isKindOfClass:[UIButton class]]);
}

// Tests that a labeled chip is successfully populated with text.
TEST_F(ManualFillLabeledChipiOSTest, SetText_SingleChip) {
  // Create the labeled chip and populate it with text.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];
  [labeledChip setLabelText:TOP_LABEL_TEXT
               buttonTitles:@[ BOTTOM_BUTTON_TEXT_0 ]];

  // Confirm the label has the correct text.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_NSEQ(((UILabel*)chipSubviews[TOP_LABEL_INDEX]).text, TOP_LABEL_TEXT);

  // Confirm the button has the correct text.
  EXPECT_EQ(ButtonTitle((UIButton*)chipSubviews[BOTTOM_BUTTONS_INDEX]),
            BOTTOM_BUTTON_TEXT_0);
}

// Tests that a labeled chip for an expiration date is successfully populated
// with text.
TEST_F(ManualFillLabeledChipiOSTest, SetText_ExpirationDateChip) {
  // Create labeled chip with a UIButton, a UILabel and another UIButton.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initExpirationDateChipWithTarget:0
                                                        monthSelector:0
                                                         yearSelector:0];
  [labeledChip setLabelText:TOP_LABEL_TEXT
               buttonTitles:@[ BOTTOM_BUTTON_TEXT_0, BOTTOM_BUTTON_TEXT_2 ]];

  // Confirm the top label has the correct text.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_NSEQ(((UILabel*)chipSubviews[TOP_LABEL_INDEX]).text, TOP_LABEL_TEXT);

  // Confirm the bottom button, label and other button have the correct text.
  NSArray<UIView*>* buttonStackViewSubviews =
      ((UIStackView*)chipSubviews[BOTTOM_BUTTONS_INDEX]).arrangedSubviews;
  EXPECT_EQ(
      ButtonTitle((UIButton*)buttonStackViewSubviews[BOTTOM_BUTTON_0_INDEX]),
      BOTTOM_BUTTON_TEXT_0);
  EXPECT_EQ(((UILabel*)buttonStackViewSubviews[BOTTOM_LABEL_1_INDEX]).text,
            BOTTOM_LABEL_TEXT_1);
  EXPECT_EQ(
      ButtonTitle((UIButton*)buttonStackViewSubviews[BOTTOM_BUTTON_2_INDEX]),
      BOTTOM_BUTTON_TEXT_2);
}

// Tests that a labeled chip has its text successfully cleared for reuse.
TEST_F(ManualFillLabeledChipiOSTest, PrepareForReuse_SingleChip) {
  // Create the labeled chip and populate it with text.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initSingleChipWithTarget:0 selector:0];
  [labeledChip setLabelText:TOP_LABEL_TEXT
               buttonTitles:@[ BOTTOM_BUTTON_TEXT_0 ]];

  // Call prepareForReuse to clear the text from the just-populated views.
  [labeledChip prepareForReuse];

  // Confirm the label has the correct text.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_NSEQ(((UILabel*)chipSubviews[TOP_LABEL_INDEX]).text, @"");

  // Confirm the button has the correct text.
  EXPECT_EQ(ButtonTitle((UIButton*)chipSubviews[BOTTOM_BUTTONS_INDEX]), @"");
}

// Tests that a labeled chip for an expiration date is successfully cleared for
// reuse.
TEST_F(ManualFillLabeledChipiOSTest, PrepareForReuse_ExpirationDateChip) {
  // Create labeled chip with a UIButton, a UILabel and another UIButton.
  ManualFillLabeledChip* labeledChip =
      [[ManualFillLabeledChip alloc] initExpirationDateChipWithTarget:0
                                                        monthSelector:0
                                                         yearSelector:0];
  [labeledChip setLabelText:TOP_LABEL_TEXT
               buttonTitles:@[ BOTTOM_BUTTON_TEXT_0, BOTTOM_BUTTON_TEXT_2 ]];

  // Call prepareForReuse to clear the text from the just-populated views.
  [labeledChip prepareForReuse];

  // Confirm the top label has the correct text.
  NSArray<UIView*>* chipSubviews = labeledChip.arrangedSubviews;
  EXPECT_NSEQ(((UILabel*)chipSubviews[TOP_LABEL_INDEX]).text, @"");

  // Confirm the bottom button, label and other button have the correct text.
  NSArray<UIView*>* buttonStackViewSubviews =
      ((UIStackView*)chipSubviews[BOTTOM_BUTTONS_INDEX]).arrangedSubviews;
  EXPECT_EQ(
      ButtonTitle((UIButton*)buttonStackViewSubviews[BOTTOM_BUTTON_0_INDEX]),
      @"");
  EXPECT_EQ(
      ButtonTitle((UIButton*)buttonStackViewSubviews[BOTTOM_BUTTON_2_INDEX]),
      @"");
}
