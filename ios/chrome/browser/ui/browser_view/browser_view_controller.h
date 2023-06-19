// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/ui/browser_view/tab_consumer.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_height_delegate.h"
#import "ios/chrome/browser/web/web_state_container_view_provider.h"

@protocol ApplicationCommands;
@class BookmarksCoordinator;
@class BrowserContainerViewController;
@class BubblePresenter;
@protocol CRWResponderInputView;
@protocol DefaultPromoNonModalPresentationDelegate;
@protocol FindInPageCommands;
class FullscreenController;
@protocol HelpCommands;
@class KeyCommandsProvider;
@class NewTabPageCoordinator;
@class LensCoordinator;
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
class UrlLoadingNotifierBrowserAgent;
@protocol VoiceSearchController;
class WebStateUpdateBrowserAgent;

typedef struct {
  BubblePresenter* bubblePresenter;
  ToolbarAccessoryPresenter* toolbarAccessoryPresenter;
  PopupMenuCoordinator* popupMenuCoordinator;
  NewTabPageCoordinator* ntpCoordinator;
  LensCoordinator* lensCoordinator;
  ToolbarCoordinator* toolbarCoordinator;
  TabStripCoordinator* tabStripCoordinator;
  TabStripLegacyCoordinator* legacyTabStripCoordinator;
  SideSwipeMediator* sideSwipeMediator;
  BookmarksCoordinator* bookmarksCoordinator;
  FullscreenController* fullscreenController;
  id<TextZoomCommands> textZoomHandler;
  id<HelpCommands> helpHandler;
  id<PopupMenuCommands> popupMenuCommandsHandler;
  id<ApplicationCommands> applicationCommandsHandler;
  id<FindInPageCommands> findInPageCommandsHandler;
  LayoutGuideCenter* layoutGuideCenter;
  BOOL isOffTheRecord;
  PagePlaceholderBrowserAgent* pagePlaceholderBrowserAgent;
  UrlLoadingBrowserAgent* urlLoadingBrowserAgent;
  UrlLoadingNotifierBrowserAgent* urlLoadingNotifierBrowserAgent;
  id<VoiceSearchController> voiceSearchController;
  TabUsageRecorderBrowserAgent* tabUsageRecorderBrowserAgent;
  base::WeakPtr<WebStateList> webStateList;
  SafeAreaProvider* safeAreaProvider;
  WebStateUpdateBrowserAgent* webStateUpdateBrowserAgent;
} BrowserViewControllerDependencies;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <FindBarPresentationDelegate,
                        IncognitoReauthConsumer,
                        LogoAnimationControllerOwnerOwner,
                        TabConsumer,
                        OmniboxFocusDelegate,
                        OmniboxPopupPresenterDelegate,
                        ThumbStripSupporting,
                        ToolbarHeightDelegate,
                        WebStateContainerViewProvider,
                        BrowserCommands>

// Initializes a new BVC.
// `browserContainerViewController` is the container object this BVC will exist
// inside.
// TODO(crbug.com/992582): Remove references to model objects from this class.
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

// Shows the voice search UI.
- (void)startVoiceSearch;

// Displays or refreshes the current tab.
// TODO:(crbug.com/1385847): Remove this when BVC is refactored to not know
// about model layer objects such as webstates.
- (void)displayCurrentTab;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
