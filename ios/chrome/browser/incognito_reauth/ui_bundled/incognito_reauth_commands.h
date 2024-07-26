// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_COMMANDS_H_
#define IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_COMMANDS_H_

#import <UIKit/UIKit.h>

// Commands related to incognito authentication.
// Should only be registered in a per-scene dispatcher, never in the global app
// dispatcher.
@protocol IncognitoReauthCommands

// Requests authentication and marks the scene as authenticated until the next
// scene foregrounding.
// The authentication will require user interaction. To know when it changes, a
// IncognitoReauthObserver callback will be called.
- (void)authenticateIncognitoContent;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_REAUTH_UI_BUNDLED_INCOGNITO_REAUTH_COMMANDS_H_
