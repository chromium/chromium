// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

namespace web {
class WebState;
}

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol OmniboxFocuser;
@protocol FakeboxFocuser;
@protocol SnackbarCommands;
@protocol NewTabPageControllerDelegate;

// Coordinator handling the NTP.
@interface NewTabPageCoordinator
    : ChromeCoordinator <LogoAnimationControllerOwnerOwner>

// Initializes this Coordinator with its |browserState|.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// ViewController associated with this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// The web state list to pass to ContentSuggestionsCoordinator.
@property(nonatomic, assign) web::WebState* webState;
// The toolbar delegate to pass to ContentSuggestionsCoordinator.
@property(nonatomic, weak) id<NewTabPageControllerDelegate> toolbarDelegate;
// The dispatcher to pass to ContentSuggestionsCoordinator.
@property(nonatomic, weak) id<ApplicationCommands,
                              BrowserCommands,
                              OmniboxFocuser,
                              FakeboxFocuser,
                              SnackbarCommands>
    dispatcher;

// Returns |YES| if the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;

// Dismisses all modals owned by the NTP.
- (void)dismissModals;

// Exposes content inset of contentSuggestions collectionView to ensure all of
// content is visible under the bottom toolbar.
@property(nonatomic) UIEdgeInsets contentInset;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// The content offset of the scroll view.
- (CGPoint)contentOffset;

// Reloads the content of the NewTabPage.
- (void)reload;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COORDINATOR_H_
