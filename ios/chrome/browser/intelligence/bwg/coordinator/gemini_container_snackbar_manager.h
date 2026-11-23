// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_SNACKBAR_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_SNACKBAR_MANAGER_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class Browser;
@class GeminiStartupState;
@protocol SnackbarCommands;

// Manages Chrome-presented snackbars for the Gemini container flow.
@interface GeminiContainerSnackbarManager : NSObject

// Handler for displaying and dismissing snackbars.
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

// Initializes the snackbar manager with the browser (used to build identity
// snackbars) and authentication service.
- (instancetype)initWithBrowser:(Browser*)browser
          authenticationService:(AuthenticationService*)authenticationService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Presents the mismatched-account identity snackbar for `startupState` when
// applicable.
- (void)showSnackbarIfAccountMismatchedForStartupState:
    (GeminiStartupState*)startupState;

// Disconnects raw pointers and handlers before deallocation.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_SNACKBAR_MANAGER_H_
