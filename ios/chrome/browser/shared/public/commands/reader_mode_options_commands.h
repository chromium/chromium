// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_OPTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_OPTIONS_COMMANDS_H_

// Commands protocol to show/hide the Reader mode options UI.
@protocol ReaderModeOptionsCommands <NSObject>

// Shows the Reader mode options UI.
- (void)showReaderModeOptions;

// Hides the Reader mode options UI.
- (void)hideReaderModeOptions;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READER_MODE_OPTIONS_COMMANDS_H_
