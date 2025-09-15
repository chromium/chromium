// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_FIELD_IOS_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_FIELD_IOS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"

/// UITextField subclass to allow for adjusting borders.
/// A textfield with a pre-edit state, inline autocomplete and additional text.
/// - Pre-edit: the state when the text is "selected" and will erase upon
/// typing. Unlike a normal iOS selection, no selection handles are displayed.
/// - Inline autocomplete: optional autocomplete text following the caret.
/// - Additional text: optional text after user and autocomplete text.
@interface OmniboxTextFieldIOS : UITextField <OmniboxTextInput>

/// Initialize the omnibox with the given `frame`, `textColor`, and `tintColor`.
- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor
          presentationContext:(OmniboxPresentationContext)presentationContext;

/// Initialize the omnibox with the given `frame`.
- (instancetype)initWithFrame:(CGRect)frame
          presentationContext:(OmniboxPresentationContext)presentationContext;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_OMNIBOX_TEXT_FIELD_IOS_H_
