// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_

@class ShareHighlightCommand;
@class ActivityServiceShareURLCommand;

@protocol ActivityServiceCommands <NSObject>

// Stops the existing SharingCoordinator and creates a new one. This is used
// when a sharing coordinator is already started, but the user taps again on the
// share button.
- (void)stopAndStartSharingCoordinator;

// Shows the share sheet for the current page.
- (void)sharePage;

// Shows the share sheet for a link to the Chrome App in the App Store.
- (void)shareChromeApp;

// Shows the share sheet for the page and currently highlighted text.
- (void)shareHighlight:(ShareHighlightCommand*)command;

// Shows the share sheet for the URL sharing flow for the given command.
- (void)shareURLFromContextMenu:(ActivityServiceShareURLCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_
