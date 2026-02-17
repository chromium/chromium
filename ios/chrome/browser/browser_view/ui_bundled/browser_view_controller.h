// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state_changed_callback.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_consumer.h"
#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_presenter.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_presentation_environment.h"
#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/browser/web/model/web_state_container_view_provider.h"

@class BookmarksCoordinator;
@class BrowserContentViewController;
@protocol BrowserCoordinatorCommands;
@protocol BWGCommands;
@protocol DefaultPromoNonModalPresentationDelegate;
@protocol FindInPageCommands;
class FullscreenController;
@protocol HelpCommands;
@protocol IncognitoReauthCommands;
@class KeyCommandsProvider;
@class LayoutGuideCenter;
@class NewTabPageCoordinator;
@protocol PopupMenuCommands;
@class PopupMenuCoordinator;
@class SafeAreaProvider;
@protocol SceneCommands;
@class SideSwipeCoordinator;
class SnapshotBrowserAgent;
class TabUsageRecorderBrowserAgent;
@protocol TextZoomCommands;
@class ToolbarAccessoryPresenter;
@protocol ToolbarCommands;
@class ToolbarCoordinator;
class UrlLoadingBrowserAgent;
@protocol VoiceSearchController;

typedef struct {
  ToolbarAccessoryPresenter* toolbarAccessoryPresenter;
  PopupMenuCoordinator* popupMenuCoordinator;
  NewTabPageCoordinator* ntpCoordinator;
  ToolbarCoordinator* toolbarCoordinator;
  SideSwipeCoordinator* sideSwipeCoordinator;
  BookmarksCoordinator* bookmarksCoordinator;
  raw_ptr<FullscreenController> fullscreenController;
  id<BrowserCoordinatorCommands> browserCoordinatorHandler;
  id<TextZoomCommands> textZoomHandler;
  id<HelpCommands> helpHandler;
  id<PopupMenuCommands> popupMenuCommandsHandler;
  id<SceneCommands> sceneHandler;
  id<ToolbarCommands> toolbarHandler;
  id<FindInPageCommands> findInPageCommandsHandler;
  id<BWGCommands> geminiHandler;
  LayoutGuideCenter* layoutGuideCenter;
  BOOL isOffTheRecord;
  raw_ptr<UrlLoadingBrowserAgent> urlLoadingBrowserAgent;
  id<VoiceSearchController> voiceSearchController;
  raw_ptr<TabUsageRecorderBrowserAgent> tabUsageRecorderBrowserAgent;
  raw_ptr<SnapshotBrowserAgent> snapshotBrowserAgent;
  base::WeakPtr<WebStateList> webStateList;
  SafeAreaProvider* safeAreaProvider;
} BrowserViewControllerDependencies;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <BrowserCommands,
                        BrowserLayoutConsumer,
                        ContextualSheetPresenter,
                        IncognitoReauthConsumer,
                        LensOverlayPresentationEnvironment,
                        TabConsumer,
                        OmniboxFocusDelegate,
                        OmniboxPopupPresenterDelegate,
                        ToolbarHeightDelegate,
                        WebStateContainerViewProvider>

// Initializes a new BVC.
// `browserContentViewController` is the container object this BVC will exist
// inside.
// TODO(crbug.com/41475381): Remove references to model objects from this class.
- (instancetype)initWithBrowserContentViewController:
                    (BrowserContentViewController*)browserContentViewController
                                 keyCommandsProvider:
                                     (KeyCommandsProvider*)keyCommandsProvider
                                        dependencies:
                                            (BrowserViewControllerDependencies)
                                                dependencies
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;

// Whether web usage is enabled for the WebStates in `self.browser`.
@property(nonatomic) BOOL webUsageEnabled;

// Presentation delegate for the non-modal default browser promo.
@property(nonatomic, weak) id<DefaultPromoNonModalPresentationDelegate>
    nonModalPromoPresentationDelegate;

// Command handler for Gemini commands.
@property(nonatomic, weak) id<BWGCommands> geminiHandler;

// Callback that will be invoked when the browser view visibility changed.
@property(nonatomic, assign) const BrowserViewVisibilityStateChangedCallback&
    browserViewVisibilityStateChangedCallback;

// Opens a new tab as if originating from `originPoint` and `focusOmnibox`.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener;

// Adds `tabAddedCompletion` to the completion block (if any) that will be run
// the next time a tab is added to the Browser this object was initialized
// with.
- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion;

// Shows the voice search UI.
- (void)startVoiceSearch;

// Dismisses all presented views, excluding the omnibox if `dismissOmnibox` is
// NO, then calls `completion`. if `dismissPresentedViewController` is NO, the
// view controller presented by the BVC will not be dismissed.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox
           dismissPresentedViewController:(BOOL)dismissPresentedViewController;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_
