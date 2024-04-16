// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_

#import <UIKit/UIKit.h>

@class OmniboxTextFieldIOS;

@protocol OmniboxTextFieldDelegate<UITextFieldDelegate>

@optional
// Called when the OmniboxTextFieldIOS performs a copy operation.
- (void)onCopy;

// Called before the OmniboxTextFieldIOS performs a paste operation.
- (void)willPaste;

// Called when the backspace button is tapped in the OmniboxTextFieldIOS.
- (void)onDeleteBackward;

// Called when the UIPasteControl in the omnibox's keyboard accessory is shown.
// Returns whether or not the paste control should be enabled.
- (BOOL)canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders;

// Called when the UIPasteControl in the omnibox's keyboard accessory is tapped.
- (void)pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders;

/// Called when the user accepts autocomplete text in `textField`.
- (void)textFieldDidAcceptAutocomplete:(OmniboxTextFieldIOS*)textField;

/// Called when the additional text has been removed due to a user action in
/// `textField`.
- (void)textFieldDidRemoveAdditionalText:(OmniboxTextFieldIOS*)textField;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_
