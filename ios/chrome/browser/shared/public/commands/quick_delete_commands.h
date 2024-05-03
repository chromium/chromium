// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QUICK_DELETE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QUICK_DELETE_COMMANDS_H_

@class UIViewController;

// Commands related to Quick Delete.
@protocol QuickDeleteCommands

// Shows Quick Delete.
- (void)showQuickDelete;

// Stops Quick Delete.
- (void)stopQuickDelete;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_QUICK_DELETE_COMMANDS_H_
