// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/browser_view/ui_bundled/tab_consumer.h"
#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_presenter.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_coordinator.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_animation_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/ui/lens/lens_coordinator.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/web/model/web_state_container_view_provider.h"

@protocol ApplicationCommands;
@class BookmarksCoordinator;
@class BrowserContainerViewController;
@protocol BrowserViewVisibilityConsumer;
@protocol DefaultPromoNonModalPresentationDelegate;
@protocol FindInPageCommands;
class FullscreenController;
@protocol HelpCommands;
@class KeyCommandsProvider;
@class NewTabPageCoordinator;
@protocol OmniboxCommands;
class PagePlaceholderBrowserAgent;
@protocol PopupMenuCommands;
@class PopupMenuCoordinator;
@class SafeAreaProvider;
@class SideSwipeMediator;
@class TabStripCoordinator;
@class TabStripLegacyCoordinator;
class TabUsageRecorderBrowserAgent;
@protocol TextZoomCommands;
@class ToolbarAccessoryPresenter;
@class ToolbarCoordinator;
@protocol IncognitoReauthCommands;
@class LayoutGuideCenter;
@protocol LoadQueryCommands;
class UrlLoadingBrowserAgent;
@protocol VoiceSearchController;

typedef struct {
  ToolbarAccessoryPresenter* toolbarAccessoryPresenter;
  PopupMenuCoordinator* popupMenuCoordinator;
  NewTabPageCoordinator* ntpCoordinator;
  ToolbarCoordinator* toolbarCoordinator;
  TabStripCoordinator* tabStripCoordinator;
  TabStripLegacyCoordinator* legacyTabStripCoordinator;
  SideSwipeMediator* sideSwipeMediator;
  BookmarksCoordinator* bookmarksCoordinator;
  raw_ptr<FullscreenController> fullscreenController;
  id<TextZoomCommands> textZoomHandler;
  id<HelpCommands> helpHandler;
  id<PopupMenuCommands> popupMenuCommandsHandler;
  id<ApplicationCommands> applicationCommandsHandler;
  id<FindInPageCommands> findInPageCommandsHandler;
  LayoutGuideCenter* layoutGuideCenter;
  BOOL isOffTheRecord;
  raw_ptr<PagePlaceholderBrowserAgent> pagePlaceholderBrowserAgent;
  raw_ptr<UrlLoadingBrowserAgent> urlLoadingBrowserAgent;
  id<VoiceSearchController> voiceSearchController;
  raw_ptr<TabUsageRecorderBrowserAgent> tabUsageRecorderBrowserAgent;
  base::WeakPtr<WebStateList> webStateList;
  SafeAreaProvider* safeAreaProvider;
} BrowserViewControllerDependencies;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <BrowserCommands,
                        ContextualSheetPresenter,
                        FindBarPresentationDelegate,
                        IncognitoReauthConsumer,
                        LensPresentationDelegate,
                        LogoAnimationControllerOwnerOwner,
                        TabConsumer,
                        OmniboxFocusDelegate,
                        OmniboxPopupPresenterDelegate,
                        ToolbarHeightDelegate,
                        WebStateContainerViewProvider>

// Initializes a new BVC.
// `browserContainerViewController` is the container object this BVC will exist
// inside.
// TODO(crbug.com/41475381): Remove references to model objects from this class.
- (instancetype)
    initWithBrowserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
                       keyCommandsProvider:
                           (KeyCommandsProvider*)keyCommandsProvider
                              dependencies:(BrowserViewControllerDependencies)
                                               dependencies
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Consumer that gets notified of the visibility of the browser view.
@property(nonatomic, weak) id<BrowserViewVisibilityConsumer>
    browserViewVisibilityConsumer;

// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;

// Whether web usage is enabled for the WebStates in `self.browser`.
@property(nonatomic) BOOL webUsageEnabled;

// The container used for infobar banner overlays.
@property(nonatomic, strong)
    UIViewController* infobarBannerOverlayContainerViewController;

// The container used for infobar modal overlays.
@property(nonatomic, strong)
    UIViewController* infobarModalOverlayContainerViewController;

// Presentation delegate for the non-modal default browser promo.
@property(nonatomic, weak) id<DefaultPromoNonModalPresentationDelegate>
    nonModalPromoPresentationDelegate;

// Command handler for load query commands.
@property(nonatomic, weak) id<LoadQueryCommands> loadQueryCommandsHandler;

// Command handler for omnibox commands.
@property(nonatomic, weak) id<OmniboxCommands> omniboxCommandsHandler;

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

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_VIEW_CONTROLLER_H_
