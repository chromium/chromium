// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

@class FormSuggestion;
@protocol FormSuggestionClient;
@class FormSuggestionView;
@class LayoutGuideCenter;

@protocol FormSuggestionViewDelegate <NSObject>

// User accepted a suggestion from FormSuggestionView. `index` indicates the
// position of the selected suggestion among the available suggestions.
- (void)formSuggestionView:(FormSuggestionView*)formSuggestionView
       didAcceptSuggestion:(FormSuggestion*)suggestion
                   atIndex:(NSInteger)index;

// The view received a long pull in the content direction. The delegate should
// probably unlock the trailing view and reset to a clean state.
- (void)formSuggestionViewShouldResetFromPull:
    (FormSuggestionView*)formSuggestionView;

@end

// A scrollable view for displaying user-selectable autofill form suggestions.
@interface FormSuggestionView : UIScrollView <UIInputViewAudioFeedback>

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

// Reset content insets back to zero and sets the delegate to nil. Used to stop
// hearing for the pull gesture to reset and unlock the trailing view.
- (void)resetContentInsetAndDelegateAnimated:(BOOL)animated;

// Animates the content insets so the trailing view is showed as the first
// thing.
- (void)lockTrailingView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_SUGGESTION_VIEW_H_
