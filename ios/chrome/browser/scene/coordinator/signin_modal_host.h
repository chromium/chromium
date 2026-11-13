// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_H_
#define IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/scene_sign_in_commands.h"

class Browser;
@protocol SigninModalHostDelegate;

// Host for the scene-level sign-in flow. This is the handler for the
// `SceneSignInCommands`, which show/hide the sign-in UI on top of the scene.
@interface SigninModalHost : NSObject <SceneSignInCommands>

// Provides the scene UI state needed to present the sign-in UI.
@property(nonatomic, weak) id<SigninModalHostDelegate> delegate;

// Initializes the host with the `regularBrowser` and `incognitoBrowser`.
- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// This object becomes the handler for the different command protocols it is
// hosting for, on the regular, inactive and incognito browsers.
- (void)startHostingCommandProtocols;

// Starts handling the commands of the new `incognitoBrowser`. Must be called
// after `-disconnectIncognitoBrowser`, while hosting the command protocols.
- (void)connectIncognitoBrowser:(Browser*)incognitoBrowser;

// Stops handling the commands of the current incognito browser, before it is
// destroyed.
- (void)disconnectIncognitoBrowser;

// The base view controller on which the sign-in UI will be presented.
- (void)setBaseViewControllerForModals:(UIViewController*)baseViewController;

// Dismisses the currently presented sign-in UI, with or without `animated`.
- (void)clearPresentedStateAnimated:(BOOL)animated;

// This object stops being the handler for the different command protocols it is
// hosting for.
- (void)stopHostingCommandProtocols;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SIGNIN_MODAL_HOST_H_
