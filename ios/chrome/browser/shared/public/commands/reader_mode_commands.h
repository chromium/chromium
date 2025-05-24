// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands protocol to show/hide the Reader mode UI.
@protocol ReaderModeCommands <NSObject>

// Shows the Reader mode UI.
- (void)showReaderMode;

// Hides the Reader mode UI.
- (void)hideReaderMode;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_COMMANDS_H_
