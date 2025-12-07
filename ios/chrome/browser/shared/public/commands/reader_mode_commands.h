// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"

// Commands protocol to show/hide the Reader mode UI.
@protocol ReaderModeCommands <NSObject>

// Shows the Reader mode UI.
- (void)showReaderModeFromAccessPoint:(ReaderModeAccessPoint)accessPoint;

// Hides the Reader mode UI.
- (void)hideReaderMode;

// Shows a blur overlay for Reader mode.
- (void)showReaderModeBlurOverlay:(ProceduralBlock)completion;

// Hides the blur overlay for Reader mode.
- (void)hideReaderModeBlurOverlay;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_
