// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for BWG session events. Keep up to date with GCR's SessionDelegate.
@protocol BWGSessionDelegate

// Called when a new session is created.
- (void)newSessionCreatedWithClientID:(NSString*)clientID
                             serverID:(NSString*)serverID;

// Called when the UI is shown.
- (void)UIDidAppearWithClientID:(NSString*)clientID
                       serverID:(NSString*)serverID;

// Called when the UI is hidden.
- (void)UIDidDisappearWithClientID:(NSString*)clientID
                          serverID:(NSString*)serverID;

// Called when a response is received.
- (void)responseReceivedWithClientID:(NSString*)clientID
                            serverID:(NSString*)serverID;

// Called when the user taps the BWG settings button from within the BWG UI.
- (void)didTapBWGSettingsButton;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SESSION_DELEGATE_H_
