// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_RESTORE_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_RESTORE_HELPER_H_

#import <Foundation/Foundation.h>

class Browser;
class ChromeBrowserState;

// Helper class for handling session restoration after a crash.
@interface CrashRestoreHelper : NSObject

- (instancetype)initWithBrowser:(Browser*)browser;

// Saves the session information stored on disk in temporary files and will
// then delete those from their default location. This will ensure that the
// user will then start from scratch, while allowing restoring their old
// sessions. This method has to be called before the browser is created, or the
// session information will be overwritten.
// Returns |YES| if the deletetion and backup was successful.
+ (BOOL)moveAsideSessionInformationForBrowserState:
    (ChromeBrowserState*)browserState;

// Shows an infobar on the currently active tab of the browser. This infobar
// lets the user restore its session after a crash.
- (void)showRestorePrompt;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_RESTORE_HELPER_H_
