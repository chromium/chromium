// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_RESPONDER_CHAINING_H_
#define IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_RESPONDER_CHAINING_H_

#import <UIKit/UIKit.h>

// Protocol for objects (such as views or view controllers) that can insert
// themselves into the UIResponder chain before a specified next responder.
@protocol ResponderChaining <NSObject>

// Inserts the receiver into the responder chain before `nextResponder`.
- (void)respondBeforeResponder:(UIResponder*)nextResponder;

@end

#endif  // IOS_CHROME_BROWSER_KEYBOARD_UI_BUNDLED_RESPONDER_CHAINING_H_
