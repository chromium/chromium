// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_FOCUS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_FOCUS_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol receiving notification when the omnibox is focused/unfocused.
@protocol OmniboxFocusDelegate

// Called when the omnibox gains keyboard focus.
- (void)omniboxDidBecomeFirstResponder;
// Called when the omnibox loses keyboard focus.
- (void)omniboxDidResignFirstResponder;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_FOCUS_DELEGATE_H_
