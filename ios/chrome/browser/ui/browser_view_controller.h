// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_

#import <MessageUI/MessageUI.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/settings/sync_utils/sync_presenter.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class BrowserViewControllerDependencyFactory;
class GURL;
@protocol OmniboxFocuser;
@protocol PopupMenuCommands;
@protocol FakeboxFocuser;
@protocol SnackbarCommands;
@class Tab;
@class TabModel;
@protocol TabStripFoldAnimation;
@protocol ToolbarCommands;

namespace ios {
class ChromeBrowserState;
}

// The top-level view controller for the browser UI. Manages other controllers
// which implement the interface.
@interface BrowserViewController
    : UIViewController<LogoAnimationControllerOwnerOwner,
                       SyncPresenter,
                       ToolbarCoordinatorDelegate,
                       UrlLoader>

// Initializes a new BVC from its nib. |model| must not be nil. The
// webUsageSuspended property for this BVC will be based on |model|, and future
// changes to |model|'s suspension state should be made through this BVC
// instead of directly on the model.
- (instancetype)
          initWithTabModel:(TabModel*)model
              browserState:(ios::ChromeBrowserState*)browserState
         dependencyFactory:(BrowserViewControllerDependencyFactory*)factory
applicationCommandEndpoint:(id<ApplicationCommands>)applicationCommandEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, readonly) id<ApplicationCommands,
                                  BrowserCommands,
                                  OmniboxFocuser,
                                  PopupMenuCommands,
                                  FakeboxFocuser,
                                  SnackbarCommands,
                                  ToolbarCommands,
                                  UrlLoader>
    dispatcher;

// The top-level browser container view.
@property(nonatomic, strong, readonly) UIView* contentArea;

// Invisible button used to dismiss the keyboard.
@property(nonatomic, strong) UIButton* typingShield;

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Returns whether or not text to speech is playing.
@property(nonatomic, assign, readonly, getter=isPlayingTTS) BOOL playingTTS;

// Returns the TabModel passed to the initializer.
@property(nonatomic, weak, readonly) TabModel* tabModel;

// Returns the ios::ChromeBrowserState passed to the initializer.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;

// Whether the receiver is currently the primary BVC.
- (void)setPrimary:(BOOL)primary;

// Called when the typing shield is tapped.
- (void)shieldWasTapped:(id)sender;

// Called when the user explicitly opens the tab switcher.
- (void)userEnteredTabSwitcher;

// Presents either in-product help bubbles if the the user is in a valid state
// to see one of them. At most one bubble will be shown. If the feature
// engagement tracker determines it is not valid to see one of the bubbles, that
// bubble will not be shown.
- (void)presentBubblesIfEligible;

// Called when the browser state provided to this instance is being destroyed.
// At this point the browser will no longer ever be active, and will likely be
// deallocated soon.
- (void)browserStateDestroyed;

// Opens a new tab as if originating from |originPoint| and |focusOmnibox|.
- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox;

// Add a new tab with the given url, appends it to the end of the model,
// and makes it the selected tab. The selected tab is returned.
- (Tab*)addSelectedTabWithURL:(const GURL&)url
                   transition:(ui::PageTransition)transition;

// Add a new tab with the given url, at the given |position|,
// and makes it the selected tab. The selected tab is returned.
// If |position| == NSNotFound the tab will be added at the end of the stack.
- (Tab*)addSelectedTabWithURL:(const GURL&)url
                      atIndex:(NSUInteger)position
                   transition:(ui::PageTransition)transition;

// Add a new tab with the given url, at the given |position|,
// and makes it the selected tab. The selected tab is returned.
// If |position| == NSNotFound the tab will be added at the end of the stack.
// |tabAddedCompletion| is called after the tab is added (if not nil).
- (Tab*)addSelectedTabWithURL:(const GURL&)url
                      atIndex:(NSUInteger)position
                   transition:(ui::PageTransition)transition
           tabAddedCompletion:(ProceduralBlock)tabAddedCompletion;

// Informs the BVC that a new foreground tab is about to be opened. This is
// intended to be called before setWebUsageSuspended:NO in cases where a new tab
// is about to appear in order to allow the BVC to avoid doing unnecessary work
// related to showing the previously selected tab.
- (void)expectNewForegroundTab;

// Shows the voice search UI.
- (void)startVoiceSearch;

// Dismisses all presented views, excluding the omnibox if |dismissOmnibox| is
// NO, then calls |completion|.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Returns a tab strip placeholder view created from the current state of the
// tab strip. It is used to animate the transition from the browser view
// controller to the tab switcher.
- (UIView<TabStripFoldAnimation>*)tabStripPlaceholderView;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_H_
