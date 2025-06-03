// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"

// Enum specifying the logo to be used for the image above the title of the
// bottomsheet.
typedef NS_ENUM(NSInteger, AboveTitleImageLogoType) {
  // Represents no logo. This should not be used.
  kNoLogo = 0,

  // Used for local save.
  kChromeLogo,

  // Used for upload save.
  kGooglePayLogo
};

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

// Sets legal message to be displayed in the under title view of the
// bottomsheet.
- (void)setLegalMessages:(NSArray<SaveCardMessageWithLinks*>*)legalMessages;

// Sets card information to be displayed in the under
// title view of the bottomsheet.
- (void)setCardNameAndLastFourDigits:(NSString*)label
                  withCardExpiryDate:(NSString*)subLabel
                         andCardIcon:(UIImage*)issuerIcon
           andCardAccessibilityLabel:(NSString*)accessibilityLabel;

// Updates bottomsheet to show card upload is in progress and sets accessibility
// label for the accept button to indicate loading.
- (void)showLoadingStateWithAccessibilityLabel:(NSString*)accessibilityLabel;

// Updates bottomsheet to show card upload is successful.
- (void)showConfirmationState;

@end

// Data source protocol to provide data on demand.
@protocol SaveCardBottomSheetDataSource <NSObject>

// Provides logo type based on which the image to be displayed above the title
// of the bottomsheet can be set.
@property(nonatomic, readonly) AboveTitleImageLogoType logoType;

// Provides accessibility label for the logoType.
@property(nonatomic, readonly) NSString* logoAccessibilityLabel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_CONSUMER_H_
