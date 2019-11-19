// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

@class FormSuggestion;
@protocol FormSuggestionClient;
@class FormSuggestionView;

@protocol FormSuggestionViewDelegate <NSObject>

// The view received a long pull in the content direction. The delegate should
// probably unlock the trailing view and reset to a clean state.
- (void)formSuggestionViewShouldResetFromPull:
    (FormSuggestionView*)formSuggestionView;

@end

// A scrollable view for displaying user-selectable autofill form suggestions.
@interface FormSuggestionView : UIScrollView<UIInputViewAudioFeedback>

// The delegate for FormSuggestionView events.
@property(nonatomic, weak) id<FormSuggestionViewDelegate>
    formSuggestionViewDelegate;

// The current suggestions this view is showing.
@property(nonatomic, readonly) NSArray<FormSuggestion*>* suggestions;

// A view added at the end of the current suggestions.
@property(nonatomic, strong) UIView* trailingView;

// Updates with |client| and |suggestions|.
- (void)updateClient:(id<FormSuggestionClient>)client
         suggestions:(NSArray<FormSuggestion*>*)suggestions;

// Reset content insets back to zero and sets the delegate to nil. Used to stop
// hearing for the pull gesture to reset and unlock the trailing view.
- (void)resetContentInsetAndDelegateAnimated:(BOOL)animated;

// Animates the content insets so the trailing view is showed as the first
// thing.
- (void)lockTrailingView;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_VIEW_H_
