// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol OmniboxTextFieldDelegate<UITextFieldDelegate>

@optional
// Called when the OmniboxTextFieldIOS performs a copy operation.
- (void)onCopy;

// Called before the OmniboxTextFieldIOS performs a paste operation.
- (void)willPaste;

// Called when the backspace button is tapped in the OmniboxTextFieldIOS.
- (void)onDeleteBackward;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_TEXT_FIELD_DELEGATE_H_
