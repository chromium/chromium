// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_

@class ShareHighlightCommand;

@protocol ActivityServiceCommands <NSObject>

// Shows the share sheet for the current page.
- (void)sharePage;

// Shows the share sheet for a link to the Chrome App in the App Store.
- (void)shareChromeApp;

// Shows the share sheet for the page and currently highlighted text.
- (void)shareHighlight:(ShareHighlightCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_COMMANDS_H_
