// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
@protocol BrowserCommands <NSObject>

// Dismisses the soft keyboard (if open).
- (void)dismissSoftKeyboard;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COMMANDS_H_
