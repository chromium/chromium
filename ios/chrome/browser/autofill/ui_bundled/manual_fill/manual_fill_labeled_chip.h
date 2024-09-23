// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_LABELED_CHIP_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_LABELED_CHIP_H_

#import <UIKit/UIKit.h>

// Class to hold/arrange the UIButton(s) and UILabel for a segment of payment
// card info within a ManualFillCardCell.
@interface ManualFillLabeledChip : UIStackView

// Creates an ManulFillLabeledChip of 1 UIButton with the given selector and
// target.
- (instancetype)initSingleChipWithTarget:(id)target
                                selector:(SEL)action NS_DESIGNATED_INITIALIZER;

// Creates an ManulFillLabeledChip of 2 UIButtons (separated by a label with the
// text '/') with the given selector and target.
- (instancetype)initExpirationDateChipWithTarget:(id)target
                                   monthSelector:(SEL)monthAction
                                    yearSelector:(SEL)yearAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the text for the label and buttons.
- (void)setLabelText:(NSString*)text
        buttonTitles:(NSArray<NSString*>*)buttonTitles;

// Clears the text from the label and buttons.
- (void)prepareForReuse;

// Returns a single button for labeled single chip
- (UIButton*)singleButton;

// Returns the expiration month button for a labeled expiration date chip.
- (UIButton*)expirationMonthButton;

// Returns the expiration year button for a labeled expiration date chip.
- (UIButton*)expirationYearButton;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_LABELED_CHIP_H_
