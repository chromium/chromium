// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Consumer interface for updating the save card bottomsheet UI.
// TODO(crbug.com/406311602): Declare methods to set action button texts and to
// show loading and confirmation.
@protocol SaveCardBottomSheetConsumer <NSObject>

// Sets the image to be displayed above the title of the bottomsheet.
- (void)setAboveTitleImage:(UIImage*)logoImage;

// Sets the accessibility description of the image above the title of the
// bottomsheet.
- (void)setAboveTitleImageDescription:(NSString*)description;

// Sets the main title of the bottomsheet.
- (void)setTitle:(NSString*)title;

// Sets the explanatory text below the main title of the bottomsheet.
- (void)setSubtitle:(NSString*)subtitle;

// Sets text for the button to accept saving the card in the bottomsheet.
- (void)setAcceptActionText:(NSString*)acceptActionText;

// Sets text for the button to dismiss the bottomsheet.
- (void)setCancelActionText:(NSString*)cancelActionText;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_
