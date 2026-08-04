// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/form_input_accessory/public/autofill_suggestion_context_menu_handler.h"

@class FormSuggestion;
@protocol FormSuggestionClient;
@class FormSuggestionView;
@class LayoutGuideCenter;

@protocol
    FormSuggestionViewDelegate <NSObject, AutofillSuggestionContextMenuHandler>

// User accepted a suggestion from FormSuggestionView. `index` indicates the
// position of the selected suggestion among the available suggestions.
- (void)formSuggestionView:(FormSuggestionView*)formSuggestionView
       didAcceptSuggestion:(FormSuggestion*)suggestion
                   atIndex:(NSInteger)index;

// Requests if the suggestion label should show its RP ID.
- (BOOL)formSuggestionView:(FormSuggestionView*)formSuggestionView
            shouldShowRPId:(NSString*)rpId;

// Requests the username for a passkey suggestion.
- (NSString*)formSuggestionView:(FormSuggestionView*)formSuggestionView
          usernameForSuggestion:(FormSuggestion*)suggestion;

@end

// A scrollable view for displaying user-selectable autofill form suggestions.
@interface FormSuggestionView : UIScrollView <UIInputViewAudioFeedback>

// Whether context menu interaction is enabled for suggestions. Defaults to NO,
// which disables the long-press context menu on suggestions.
@property(nonatomic, assign) BOOL isContextMenuEnabled;

// The delegate for FormSuggestionView events.
@property(nonatomic, weak) id<FormSuggestionViewDelegate>
    formSuggestionViewDelegate;

// The current suggestions this view is showing.
@property(nonatomic, readonly) NSArray<FormSuggestion*>* suggestions;

// A view added at the end of the current suggestions.
@property(nonatomic, strong) UIView* trailingView;

// The layout guide center to use to refer to the first suggestion label.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Updates with `suggestions`.
- (void)updateSuggestions:(NSArray<FormSuggestion*>*)suggestions
           showScrollHint:(BOOL)showScrollHint
    accessoryTrailingView:(UIView*)trailingView
               completion:(void (^)(BOOL finished))completion;

// Sets whether the UI is in compact mode, so that the keyboard accessory can
// adapt to the compact size class if necessary.
- (void)setIsCompact:(BOOL)isCompact;

// Resets content insets back to zero and sets the delegate to nil. Used to stop
// hearing for the pull gesture to reset and unlock the trailing view.
- (void)resetContentInsetAndDelegateAnimated:(BOOL)animated;

// Starts or stops the activity indicator and enables/disables user interaction.
- (void)setActivityIndicatorEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_UI_FORM_SUGGESTION_VIEW_H_
