// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACCOUNT_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACCOUNT_PICKER_COMMANDS_H_

// Commands to show/hide the account picker.
@protocol AccountPickerCommands <NSObject>

// Hides the account picker. If `animated` is YES then dismissal is animated.
- (void)hideAccountPickerAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACCOUNT_PICKER_COMMANDS_H_
