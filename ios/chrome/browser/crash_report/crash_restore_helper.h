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

// Returns YES if a backup file for sessionID can be found on disk.
+ (BOOL)isBackedUpSessionID:(NSString*)sessionID;

// Saves the session information stored on disk for sessions with |sessionIDs|
// in temporary files and will then delete those from their default location.
// This will ensure that the user will then start from scratch, while allowing
// restoring their old sessions. This method has to be called before the browser
// is created, or the session information will be overwritten.
// |sessionIDs| can be nil when multiple windows are not supported, and in that
// case only the default session will be moved.
// Returns |YES| if the  at least one session deletion was successful.
+ (BOOL)moveAsideSessions:(NSSet<NSString*>*)sessionIDs
          forBrowserState:(ChromeBrowserState*)browserState;

// Move the session information for Legacy non multiwindow supported OS.
// This method deletes the session from its default location, while
// allowing restoring it back later.
// Returns |YES| if the delettion and backup was successful.
+ (BOOL)moveAsideSessionInformationForBrowserState:
    (ChromeBrowserState*)browserState;

// Shows an infobar on the currently active tab of the browser. This infobar
// lets the user restore its session after a crash.
- (void)showRestorePrompt;

@end

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_RESTORE_HELPER_H_
