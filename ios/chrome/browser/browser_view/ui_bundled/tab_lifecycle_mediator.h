// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_TAB_LIFECYCLE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_TAB_LIFECYCLE_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol AppLauncherTabHelperBrowserPresentationProvider;
@class CommandDispatcher;
@protocol DownloadManagerTabHelperDelegate;
@protocol PasswordControllerDelegate;
@class PrintCoordinator;
@protocol RepostFormTabHelperDelegate;
@class SadTabCoordinator;
@protocol SnapshotGeneratorDelegate;
class TabInsertionBrowserAgent;
class WebStateList;
@protocol NetExportTabHelperDelegate;
@protocol NewTabPageTabHelperDelegate;
@protocol OverscrollActionsControllerDelegate;
@protocol EditMenuBuilder;
class Browser;

// Mediator that handles the setup of tab helpers that require UI-layer
// dependencies not available when AttachTabHelpers() is called.
// The required dependencies are injected into the mediator instance as
// properties, and are generally expected not to change during the mediator's
// lifetime. The mediator keeps only weak references to injected dependencies.
@interface TabLifecycleMediator : NSObject

@property(nonatomic, weak) id<DownloadManagerTabHelperDelegate>
    downloadManagerTabHelperDelegate;
@property(nonatomic, weak) UIViewController* baseViewController;
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;
@property(nonatomic, weak) id<NetExportTabHelperDelegate> tabHelperDelegate;
@property(nonatomic, weak) id<NewTabPageTabHelperDelegate> NTPTabHelperDelegate;
@property(nonatomic, weak) PrintCoordinator* printCoordinator;
@property(nonatomic, weak) id<RepostFormTabHelperDelegate> repostFormDelegate;
@property(nonatomic, assign) TabInsertionBrowserAgent* tabInsertionBrowserAgent;
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollActionsDelegate;
@property(nonatomic, weak) id<PasswordControllerDelegate>
    passwordControllerDelegate;
@property(nonatomic, weak) id<SnapshotGeneratorDelegate>
    snapshotGeneratorDelegate;
@property(nonatomic, weak) id<AppLauncherTabHelperBrowserPresentationProvider>
    appLauncherBrowserPresentationProvider;
@property(nonatomic, weak) id<EditMenuBuilder> editMenuBuilder;

// Creates an instance of the mediator. Delegates will be installed into all
// existing web states in the `browser` web state list. While the mediator is
// alive, delegates will be added and removed from web states when they are
// inserted into or removed from the web state list.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects all delegates set by the mediator on any web states in its
// web state list. After `disconnect` is called, the mediator will not add
// delegates to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_TAB_LIFECYCLE_MEDIATOR_H_
