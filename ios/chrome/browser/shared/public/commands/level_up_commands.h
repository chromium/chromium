// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LEVEL_UP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LEVEL_UP_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to the Level Up feature.
@protocol LevelUpCommands

// Shows the Level Up experience.
- (void)showLevelUp;

// Dismisses the Level Up experience.
- (void)dismissLevelUp;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LEVEL_UP_COMMANDS_H_
