// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_COMMANDS_H_

#import <UIKit/UIKit.h>

// Command protocol for events for the Default Browser module.
@protocol DefaultBrowserCommands

// Called when the Default Browser tile is tapped.
- (void)didTapDefaultBrowserPromo;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_COMMANDS_H_
