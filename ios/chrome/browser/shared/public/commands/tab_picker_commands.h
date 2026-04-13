// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_

#import <Foundation/Foundation.h>

@protocol TabPickerCommands <NSObject>

// Shows the tab picker UI.
- (void)showTabPicker;

// Hides the tab picker UI.
- (void)hideTabPicker;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_PICKER_COMMANDS_H_
