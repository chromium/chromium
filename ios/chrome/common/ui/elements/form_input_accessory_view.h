// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

@class FormInputAccessoryView;
@class FormInputAccessoryViewTextData;

// Informs the receiver of actions in the accessory view.
@protocol FormInputAccessoryViewDelegate
- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender;
- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender;
- (void)formInputAccessoryViewDidTapCloseButton:(FormInputAccessoryView*)sender;
- (FormInputAccessoryViewTextData*)textDataforFormInputAccessoryView:
    (FormInputAccessoryView*)sender;
- (void)fromInputAccessoryViewDidTapOmniboxTypingShield:
    (FormInputAccessoryView*)sender;
@optional
// This method is called when the manual fill button is tapped.
// Must be implemented when the view contains the button (i.e. setUp called with
// non nil "manualFillSymbol").
- (void)formInputAccessoryViewDidTapManualFillButton:
    (FormInputAccessoryView*)sender;

// This method is called when the password manual fill button is tapped.
// Must be implemented when the view contains the button (i.e. setUp called with
// non nil "passwordManualFillSymbol").
- (void)formInputAccessoryViewDidTapPasswordManualFillButton:
    (FormInputAccessoryView*)sender;

// This method is called when the credit card manual fill button is tapped.
// Must be implemented when the view contains the button (i.e. setUp called with
// non nil "creditCardManualFillButton").
- (void)formInputAccessoryViewDidTapCreditCardManualFillButton:
    (FormInputAccessoryView*)sender;

// This method is called when the address manual fill button is tapped.
// Must be implemented when the view contains the button (i.e. setUp called with
// non nil "addressManualFillSymbol").
- (void)formInputAccessoryViewDidTapAddressManualFillButton:
    (FormInputAccessoryView*)sender;

@end

extern NSString* const kFormInputAccessoryViewAccessibilityID;
extern NSString* const
    kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID;

// Subview of the accessory view for web forms. Shows a custom view with form
// navigation controls above the keyboard. Enables input clicks by way of the
// playInputClick method.
@interface FormInputAccessoryView : UIView <UIInputViewAudioFeedback>

// The previous button if the view was set up with a navigation delegate and no
// "manualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* previousButton;

// The next button if the view was set up with a navigation delegate and no
// "manualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* nextButton;

// The expand button if the view was set up with a navigation delegate and a
// "manualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* manualFillButton;

// The password button if the view was set up with a navigation delegate and a
// "passwordManualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* passwordManualFillButton;

// The credit card button if the view was set up with a navigation delegate and
// a "creditCardManualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* creditCardManualFillButton;

// The address button if the view was set up with a navigation delegate and a
// "addressManualFillSymbol". Nil otherwise.
@property(nonatomic, readonly, weak) UIButton* addressManualFillButton;

// The leading view.
@property(nonatomic, readonly, weak) UIView* leadingView;

// The trailing view. Can be nil.
@property(nonatomic, readonly, weak) UIView* trailingView;

// Sets up the view with the given `leadingView`. Navigation controls are shown
// on the trailing side and use `delegate` for actions.
- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate;

// Sets up the view with the given `leadingView`. Navigation controls are shown
// on the trailing side and use `delegate` for actions.
// This initializer modifies multiple UI elements:
// - The manual fill buttons are added, using *manualFillSymbol as their images.
// - The previous and next buttons are removed.
// - The accessory height is increased.
// - The background color is set to grey.
// If `closeButtonSymbol` is nil, the close button will use the default text.
// Otherwise, it will use closeButtonSymbol as the image instead.
- (void)setUpWithLeadingView:(UIView*)leadingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol;

// Sets up the view with the given `leadingView`. Navigation controls are
// replaced with `customTrailingView`.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView;

// Sets the height of the omnibox typing shield. Set a height of 0 to hide the
// typing shield. The omnibox typing shield is a transparent view on the top
// edge of the input accessory view for the collapsed bottom omnibox
// (crbug.com/1490601).
- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
