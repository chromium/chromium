// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"

@class OmniboxTextViewIOS;

/// Delegate for adjusting the text view height when the content changes.
@protocol OmniboxTextViewHeightDelegate

/// Informs that the content has changed, whether it's caused by the user or the
/// system.
- (void)textViewContentChanged:(OmniboxTextViewIOS*)textView;

@end

/// UITextView subclass to allow for adjusting borders.
/// A textview with a pre-edit state, inline autocomplete and additional text.
/// - Pre-edit: the state when the text is "selected" and will erase upon
/// typing. Unlike a normal iOS selection, no selection handles are displayed.
/// - Inline autocomplete: optional autocomplete text following the caret.
/// - Additional text: optional text after user and autocomplete text.
@interface OmniboxTextViewIOS : UITextView <OmniboxTextInput>

@property(nonatomic, weak) id<OmniboxTextViewHeightDelegate> heightDelegate;

/// The placeholder label. It must be added as a sibling to this view before
/// being set here because it must not be inside of the scroll view of
/// UITextView. The logic to constrain the placeholder is handled by this class.
@property(nonatomic, weak) UILabel* placeholderLabel;

/// Initialize the omnibox with the given `frame`, `textColor`, and `tintColor`.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor
          presentationContext:(OmniboxPresentationContext)presentationContext;

/// Returns the user text.
- (NSAttributedString*)attributedUserText;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_VIEW_IOS_H_
