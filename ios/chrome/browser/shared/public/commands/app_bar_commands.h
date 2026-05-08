// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APP_BAR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APP_BAR_COMMANDS_H_

#import <Foundation/Foundation.h>

// Protocol to handle commands for the App Bar.
@protocol AppBarCommands <NSObject>

// Shows the blue-ish background with a circular gradient on the App Bar.
// If `centered` is YES, the gradient is centered. Otherwise, it is left-bottom
// aligned.
- (void)showIPHBackgroundWithCentering:(BOOL)centered;

// Hides the blue-ish background on the App Bar.
- (void)hideIPHBackground;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APP_BAR_COMMANDS_H_
