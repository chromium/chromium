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
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_state_container_view_provider.h"

class Browser;
@class BookmarkInteractionController;
@class BrowserContainerViewController;
@class BubblePresenter;
@class CommandDispatcher;
@protocol CRWResponderInputView;
@class DefaultBrowserPromoNonModalScheduler;
@protocol DefaultPromoNonModalPresentationDelegate;
// TODO(crbug.com/1331229): Remove all use of the download manager coordinator
// from BVC
@class DownloadManagerCoordinator;
class FullscreenController;
@protocol HelpCommands;
@class KeyCommandsProvider;
@class NewTabPageCoordinator;
@class LensCoordinator;
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
@protocol IncognitoReauthCommands;

// TODO(crbug.com/1328039): Remove all use of the prerender service from BVC
// TODO(crbug.com/1331229): Remove all use of the download manager coordinator
// from BVC
typedef struct {
  PrerenderService* prerenderService;
  BubblePresenter* bubblePresenter;
  ToolbarAccessoryPresenter* toolbarAccessoryPresenter;
  PopupMenuCoordinator* popupMenuCoordinator;
  DownloadManagerCoordinator* downloadManagerCoordinator;
  NewTabPageCoordinator* ntpCoordinator;
  LensCoordinator* lensCoordinator;
  PrimaryToolbarCoordinator* primaryToolbarCoordinator;
  SecondaryToolbarCoordinator* secondaryToolbarCoordinator;
  TabStripCoordinator* tabStripCoordinator;
  TabStripLegacyCoordinator* legacyTabStripCoordinator;
  SideSwipeController* sideSwipeController;
  BookmarkInteractionController* bookmarkInteractionController;
  FullscreenController* fullscreenController;
  id<TextZoomCommands> textZoomHandler;
  id<HelpCommands> helpHandler;
  id<PopupMenuCommands> popupMenuCommandsHandler;
  id<SnackbarCommands> snackbarCommandsHandler;
} BrowserViewControllerDependencies;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <FindBarPresentationDelegate,
                        IncognitoReauthConsumer,
                        LogoAnimationControllerOwnerOwner,
                        OmniboxPopupPresenterDelegate,
                        ThumbStripSupporting,
                        ToolbarCoordinatorDelegate,
                        WebStateContainerViewProvider,
                        BrowserCommands>

// Initializes a new BVC.
// `browser` is the browser whose tabs this BVC will display.
// `browserContainerViewController` is the container object this BVC will exist
// inside.
// `dispatcher` is the dispatcher instance this BVC will use.
// TODO(crbug.com/992582): Remove references to model objects -- including
//   `browser` and `dispatcher` -- from this class.
- (instancetype)initWithBrowser:(Browser*)browser
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
                        dispatcher:(CommandDispatcher*)dispatcher
               keyCommandsProvider:(KeyCommandsProvider*)keyCommandsProvider
                      dependencies:
                          (BrowserViewControllerDependencies)dependencies
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Command dispatcher.
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
