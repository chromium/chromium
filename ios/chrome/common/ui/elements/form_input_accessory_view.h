// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

// Large height for the keyboard accessory.
extern const CGFloat kLargeKeyboardAccessoryHeight;

@class FormInputAccessoryView;
@class FormInputAccessoryViewTextData;

// Enum for groups of subitems (UIButtons for now) in `FormInputAccessoryView`.
enum class FormInputAccessoryViewSubitemGroup {
  // Empty group, the default uninitialized value.
  kEmpty = 0,
  // The expand button.
  kExpandButton,
  // Manual fill buttons: the password button, the credit card button and the
  // address button.
  kManualFillButtons,
  // Navigation buttons: the previous button and the next button.
  kNavigationButtons
};

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

// The trailing view. Can be nil. It is the parent view of all manual fill
// buttons, and the close button when split view is not enabled.
@property(nonatomic, readonly, weak) UIView* trailingView;

// Sets up the view with the given `leadingView`. Navigation controls are shown
// on the trailing side and use `delegate` for actions.
- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate;

// Sets up the view with the given `leadingView`. Navigation controls are shown
// on the trailing side and use `delegate` for actions.
// This initializer modifies multiple UI elements:
// - The manual fill buttons are added, using the supplied symbols as their
// images.
// - The previous and next buttons are removed.
// - The accessory height is increased.
// - The background color is set to grey.
// If `closeButtonSymbol` is nil, the close button will use the default text.
// Otherwise, it will use `closeButtonSymbol` as the image instead.
// `splitViewEnabled` indicates whether two-bubble feature flag is enabled.
// `isTabletFormFactor` modifies the appearance of the manual fill button.
- (void)setUpWithLeadingView:(UIView*)leadingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol
              splitViewEnabled:(BOOL)splitViewEnabled
            isTabletFormFactor:(BOOL)isTabletFormFactor;

// Sets the height of the omnibox typing shield. Set a height of 0 to hide the
// typing shield. The omnibox typing shield is a transparent view on the top
// edge of the input accessory view for the collapsed bottom omnibox
// (crbug.com/1490601).
- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight;

// Sets whether the UI is in compact mode, so that the keyboard accessory can
// adapt to the compact size class if necessary.
- (void)setIsCompact:(BOOL)isCompact;

// Shows the group passed in, and hides other elements. If it is already the
// current group, then it simply returns.
- (void)showGroup:(FormInputAccessoryViewSubitemGroup)group;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
