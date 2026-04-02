// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_

#import <Foundation/Foundation.h>

// Protocol for commands that control the fullscreen state.
@protocol FullscreenCommands

// Enters fullscreen mode.
- (void)enterFullscreenWithAnimation:(BOOL)animated;

// Exits fullscreen mode.
- (void)exitFullscreenWithAnimation:(BOOL)animated;

// Disables fullscreen. Increments the disabled counter.
- (void)disableFullscreenAnimated:(BOOL)animated;

// Re-enables fullscreen. Decrements the disabled counter.
- (void)reenableFullscreen;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FULLSCREEN_COMMANDS_H_
