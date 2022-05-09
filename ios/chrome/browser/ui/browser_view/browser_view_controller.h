// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/find_bar/find_bar_coordinator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_consumer.h"
#import "ios/chrome/browser/ui/ntp/logo_animation_controller.h"
#import "ios/chrome/browser/ui/page_info/requirements/page_info_presentation.h"
#import "ios/chrome/browser/ui/print/print_controller.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_supporting.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/web/web_navigation_ntp_delegate.h"
#import "ios/chrome/browser/web/web_state_container_view_provider.h"

@protocol ActivityServicePositioner;
class Browser;
@class BrowserContainerViewController;
@class BrowserViewControllerDependencyFactory;
@class CommandDispatcher;
@protocol CRWResponderInputView;
@class DefaultBrowserPromoNonModalScheduler;
@protocol DefaultPromoNonModalPresentationDelegate;
@class ToolbarAccessoryPresenter;
@protocol IncognitoReauthCommands;

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController <FindBarPresentationDelegate,
                        IncognitoReauthConsumer,
                        LogoAnimationControllerOwnerOwner,
                        PageInfoPresentation,
                        PrintControllerDelegate,
                        SigninPresenter,
                        SyncPresenter,
                        ThumbStripSupporting,
                        ToolbarCoordinatorDelegate,
                        WebNavigationNTPDelegate,
                        WebStateContainerViewProvider>

// Initializes a new BVC.
// |browser| is the browser whose tabs this BVC will display.
// |factory| is the dependency factory created for this BVC instance.
// |browserContainerViewController| is the container object this BVC will exist
// inside.
// |dispatcher| is the dispatcher instance this BVC will use.
// TODO(crbug.com/992582): Remove references to model objects -- including
//   |browser| and |dispatcher| -- from this class.
- (instancetype)initWithBrowser:(Browser*)browser
                 dependencyFactory:
                     (BrowserViewControllerDependencyFactory*)factory
    browserContainerViewController:
        (BrowserContainerViewController*)browserContainerViewController
                        dispatcher:(CommandDispatcher*)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Command dispatcher.
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;

// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;

// Returns whether or not text to speech is playing.
@property(nonatomic, assign, readonly, getter=isPlayingTTS) BOOL playingTTS;

// The container used for infobar banner overlays.
@property(nonatomic, strong)
    UIViewController* infobarBannerOverlayContainerViewController;

// The container used for infobar modal overlays.
@property(nonatomic, strong)
    UIViewController* infobarModalOverlayContainerViewController;

// The sad tab view controller. Only used to add the sad tab view (if any) to
// snapshots.
// TODO(crbug.com/1272491): Refactor snapshotting to remove the need for this
// property.
@property(nonatomic, strong) UIViewController* sadTabViewController;

// Presenter used to display accessories over the toolbar (e.g. Find In Page).
@property(nonatomic, strong)
    ToolbarAccessoryPresenter* toolbarAccessoryPresenter;

// Positioner for activity services attached to the toolbar.
@property(nonatomic, readonly) id<ActivityServicePositioner>
    activityServicePositioner;

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

// Opens a new tab as if originating from |originPoint| and |focusOmnibox|.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox
                    inheritOpener:(BOOL)inheritOpener;

// Adds |tabAddedCompletion| to the completion block (if any) that will be run
// the next time a tab is added to the Browser this object was initialized
// with.
- (void)appendTabAddedCompletion:(ProceduralBlock)tabAddedCompletion;

// Informs the BVC that a new foreground tab is about to be opened. This is
// intended to be called before setWebUsageSuspended:NO in cases where a new tab
// is about to appear in order to allow the BVC to avoid doing unnecessary work
// related to showing the previously selected tab.
- (void)expectNewForegroundTab;

// Shows the voice search UI.
- (void)startVoiceSearch;

// Returns the number of tabs with the NTP open.
- (int)liveNTPCount;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_H_
