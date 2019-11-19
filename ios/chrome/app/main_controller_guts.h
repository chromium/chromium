// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

@class BrowserViewController;
@class HistoryCoordinator;
@class SigninInteractionCoordinator;
@class TabGridCoordinator;
@protocol BrowserInterfaceProvider;
@protocol TabSwitcher;
class AppUrlLoadingService;

// TODO(crbug.com/1012697): Remove this protocol when SceneController is
// operational. Move the private internals back into MainController, and pass
// ownership of Scene-related objects to SceneController.
@protocol MainControllerGuts <SettingsNavigationControllerDelegate,
                              UserFeedbackDataSource>

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The application level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) AppUrlLoadingService* appURLLoadingService;

// The tab switcher command and the voice search commands can be sent by views
// that reside in a different UIWindow leading to the fact that the exclusive
// touch property will be ineffective and a command for processing both
// commands may be sent in the same run of the runloop leading to
// inconsistencies. Those two boolean indicate if one of those commands have
// been processed in the last 200ms in order to only allow processing one at
// a time.
// TODO(crbug.com/560296):  Provide a general solution for handling mutually
// exclusive chrome commands sent at nearly the same time.
@property(nonatomic, assign) BOOL isProcessingTabSwitcherCommand;
@property(nonatomic, assign) BOOL isProcessingVoiceSearchCommand;
// The SigninInteractionCoordinator to present Sign In UI. It is created the
// first time Sign In UI is needed to be presented and should not be destroyed
// while the UI is presented.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

- (BOOL)isTabSwitcherActive;

- (id<TabSwitcher>)tabSwitcher;
- (ios::ChromeBrowserState*)mainBrowserState;
- (ios::ChromeBrowserState*)currentBrowserState;
- (BrowserViewController*)currentBVC;
- (BrowserViewController*)mainBVC;
- (BrowserViewController*)otrBVC;
- (TabGridCoordinator*)mainCoordinator;
- (id<BrowserInterfaceProvider>)interfaceProvider;
- (void)startVoiceSearchInCurrentBVC;

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;
- (void)closeSettingsAnimated:(BOOL)animated
                   completion:(ProceduralBlock)completion;

- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion;
- (void)showTabSwitcher;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
