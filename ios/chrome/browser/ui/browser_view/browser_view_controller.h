// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_state_container_view_provider.h"

@protocol ApplicationCommands;
class Browser;
@class BookmarksCoordinator;
@class BrowserContainerViewController;
@protocol BrowserCoordinatorCommands;
@class BubblePresenter;
@protocol CRWResponderInputView;
@class DefaultBrowserPromoNonModalScheduler;
@protocol DefaultPromoNonModalPresentationDelegate;
@protocol FindInPageCommands;
class FullscreenController;
@protocol HelpCommands;
@class KeyCommandsProvider;
@class NewTabPageCoordinator;
@class LensCoordinator;
@protocol OmniboxCommands;
@protocol PopupMenuCommands;
@class PopupMenuCoordinator;
// TODO(crbug.com/1328039): Remove all use of the prerender service from BVC
@protocol PopupMenuUIUpdating;
class PrerenderService;
@class PrimaryToolbarCoordinator;
@class SecondaryToolbarCoordinator;
@class SideSwipeController;
@protocol SnackbarCommands;
@class TabStripCoordinator;
@class TabStripLegacyCoordinator;
@protocol TextZoomCommands;
@class ToolbarAccessoryPresenter;
@protocol ToolbarCommands;
@protocol IncognitoReauthCommands;
@protocol LoadQueryCommands;

// TODO(crbug.com/1328039): Remove all use of the prerender service from BVC
typedef struct {
  PrerenderService* prerenderService;
  BubblePresenter* bubblePresenter;
  ToolbarAccessoryPresenter* toolbarAccessoryPresenter;
  PopupMenuCoordinator* popupMenuCoordinator;
  NewTabPageCoordinator* ntpCoordinator;
  LensCoordinator* lensCoordinator;
  PrimaryToolbarCoordinator* primaryToolbarCoordinator;
  SecondaryToolbarCoordinator* secondaryToolbarCoordinator;
  TabStripCoordinator* tabStripCoordinator;
  TabStripLegacyCoordinator* legacyTabStripCoordinator;
  SideSwipeController* sideSwipeController;
  BookmarksCoordinator* bookmarksCoordinator;
  FullscreenController* fullscreenController;
  id<TextZoomCommands> textZoomHandler;
  id<HelpCommands> helpHandler;
  id<PopupMenuCommands> popupMenuCommandsHandler;
  id<SnackbarCommands> snackbarCommandsHandler;
  id<ApplicationCommands> applicationCommandsHandler;
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler;
  id<FindInPageCommands> findInPageCommandsHandler;
  id<ToolbarCommands> toolbarCommandsHandler;
  id<LoadQueryCommands> loadQueryCommandsHandler;
  id<OmniboxCommands> omniboxCommandsHandler;
  BOOL isOffTheRecord;
} BrowserViewControllerDependencies;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <FindBarPresentationDelegate,
                        IncognitoReauthConsumer,
                        LogoAnimationControllerOwnerOwner,
                        OmniboxFocusDelegate,
                        OmniboxPopupPresenterDelegate,
                        ThumbStripSupporting,
                        WebStateContainerViewProvider,
                        BrowserCommands>

// Initializes a new BVC.
// `browser` is the browser whose tabs this BVC will display.
// `browserContainerViewController` is the container object this BVC will exist
// inside.
// TODO(crbug.com/992582): Remove references to model objects -- including
//   `browser` -- from this class.
- (instancetype)initWithBrowser:(Browser*)browser
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
               keyCommandsProvider:(KeyCommandsProvider*)keyCommandsProvider
                      dependencies:
                          (BrowserViewControllerDependencies)dependencies
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;

// TODO(crbug.com/1329104): Move voice search controller/coordinator to
// BrowserCoordinator, remove this as a public property. Returns whether or not
// text to speech is playing.
@property(nonatomic, assign, readonly, getter=isPlayingTTS) BOOL playingTTS;

// The container used for infobar banner overlays.
@property(nonatomic, strong)
    UIViewController* infobarBannerOverlayContainerViewController;

// The container used for infobar modal overlays.
@property(nonatomic, strong)
    UIViewController* infobarModalOverlayContainerViewController;

// Scheduler for the non-modal default browser promo.
// TODO(crbug.com/1204120): The BVC should not need this model-level object.
@property(nonatomic, weak)
    DefaultBrowserPromoNonModalScheduler* nonModalPromoScheduler;

// Presentation delegate for the non-modal default browser promo.
@property(nonatomic, weak) id<DefaultPromoNonModalPresentationDelegate>
    nonModalPromoPresentationDelegate;

// Whether the receiver is currently the primary BVC.
- (void)setPrimary:(BOOL)primary;

// Called when the user explicitly opens the tab switcher.
- (void)userEnteredTabSwitcher;

// Opens a new tab as if originating from `originPoint` and `focusOmnibox`.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener;

// Adds `tabAddedCompletion` to the completion block (if any) that will be run
// the next time a tab is added to the Browser this object was initialized
// with.
- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion;

// Informs the BVC that a new foreground tab is about to be opened. This is
// intended to be called before setWebUsageSuspended:NO in cases where a new tab
// is about to appear in order to allow the BVC to avoid doing unnecessary work
// related to showing the previously selected tab.
// TODO(crbug.com/1329109): Move this to a browser agent or web event mediator.
- (void)expectNewForegroundTab;

// Shows the voice search UI.
- (void)startVoiceSearch;

// Displays or refreshes the current tab.
// TODO:(crbug.com/1385847): Remove this when BVC is refactored to not know
// about model layer objects such as webstates.
- (void)displayCurrentTab;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
