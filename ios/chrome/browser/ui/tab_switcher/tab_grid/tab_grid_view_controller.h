// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_action_wrangler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_animation_layout_providing.h"

@protocol ApplicationCommands;
@protocol GridCommands;
@protocol PriceCardDataSource;
@protocol GridShareableItemsProvider;
class GURL;
@protocol InactiveTabsInfoConsumer;
@protocol IncognitoReauthCommands;
@protocol IncognitoReauthConsumer;
@class LayoutGuideCenter;
@protocol PopupMenuCommands;
@protocol RecentTabsConsumer;
@class RecentTabsTableViewController;
@class TabGridBottomToolbar;
@protocol TabCollectionCommands;
@protocol TabCollectionConsumer;
@protocol TabCollectionDragDropHandler;
@protocol TabGridConsumer;
@protocol TabContextMenuProvider;
@protocol TabGridMutator;
@protocol TabGridToolbarsCommandsWrangler;
@class TabGridTopToolbar;
@class TabGridViewController;

// Configurations for tab grid pages.
enum class TabGridPageConfiguration {
  // All pages are enabled.
  kAllPagesEnabled = 0,
  // Only the incognito page is disabled.
  kIncognitoPageDisabled = 1,
  // Only incognito page is enabled.
  kIncognitoPageOnly = 2,
};

// Delegate protocol for an object that can handle presenting ("opening") tabs
// from the tab grid.
@protocol TabPresentationDelegate <NSObject>
// Show the active tab in `page`, presented on top of the tab grid.  The
// omnibox will be focused after the animation if `focusOmnibox` is YES.
- (void)showActiveTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox;
@end

@protocol TabGridViewControllerDelegate <NSObject>

// Asks the delegate for the page that should currently be active.
- (TabGridPage)activePageForTabGridViewController:
    (TabGridViewController*)tabGridViewController;

// Notifies the delegate that the tab grid was dismissed via the
// ViewRevealingAnimatee.
- (void)tabGridViewControllerDidDismiss:
    (TabGridViewController*)tabGridViewController;

// Opens a link when the user clicks on the in-text link.
- (void)openLinkWithURL:(const GURL&)URL;

// Asks the delegate to open history modal with results filtered by
// `searchText`.
- (void)showHistoryFilteredBySearchText:(NSString*)searchText;

// Asks the delegate to open a new tab page with a web search for `searchText`.
- (void)openSearchResultsPageForSearchText:(NSString*)searchText;

// Asks the delegate to show the inactive tabs.
- (void)showInactiveTabs;

@end

// View controller representing a tab switcher. The tab switcher has an
// incognito tab grid, regular tab grid, and remote tabs.
@interface TabGridViewController
    : UIViewController <IncognitoReauthObserver,
                        KeyCommandActions,
                        TabGridConsumer,
                        LegacyGridTransitionAnimationLayoutProviding,
                        TabGridPaging,
                        TabGridToolbarsActionWrangler,
                        UISearchBarDelegate>

@property(nonatomic, weak) id<ApplicationCommands> handler;
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;
@property(nonatomic, weak) IncognitoReauthSceneAgent* reauthAgent;
// Handlers for popup menu commands for the regular and incognito states.
@property(nonatomic, weak) id<PopupMenuCommands> regularPopupMenuHandler;
@property(nonatomic, weak) id<PopupMenuCommands> incognitoPopupMenuHandler;

// Delegate for this view controller to handle presenting tab UI.
@property(nonatomic, weak) id<TabPresentationDelegate> tabPresentationDelegate;

@property(nonatomic, weak) id<TabGridViewControllerDelegate> delegate;

// Mutator to apply all user change in the model.
@property(nonatomic, weak) id<TabGridMutator> mutator;

// Consumers send updates from the model layer to the UI layer.
@property(nonatomic, readonly)
    id<TabCollectionConsumer, InactiveTabsInfoConsumer>
        regularTabsConsumer;
@property(nonatomic, readonly)
    id<TabCollectionConsumer, IncognitoReauthConsumer>
        incognitoTabsConsumer;
@property(nonatomic, readonly) id<RecentTabsConsumer> remoteTabsConsumer;
@property(nonatomic, readonly) id<TabCollectionConsumer> pinnedTabsConsumer;

// Delegates send updates from the UI layer to the model layer.
@property(nonatomic, weak) id<GridCommands> regularTabsDelegate;
@property(nonatomic, weak) id<GridCommands> inactiveTabsDelegate;
@property(nonatomic, weak) id<GridCommands> incognitoTabsDelegate;
@property(nonatomic, weak) id<TabCollectionCommands> pinnedTabsDelegate;

// Handles drag and drop interactions that require the model layer.
@property(nonatomic, weak) id<TabCollectionDragDropHandler>
    regularTabsDragDropHandler;
@property(nonatomic, weak) id<TabCollectionDragDropHandler>
    incognitoTabsDragDropHandler;
@property(nonatomic, weak) id<TabCollectionDragDropHandler>
    pinnedTabsDragDropHandler;

// Data source for acquiring data which power the PriceCardView
@property(nonatomic, weak) id<PriceCardDataSource> priceCardDataSource;

@property(nonatomic, weak) id<GridShareableItemsProvider>
    regularTabsShareableItemsProvider;
@property(nonatomic, weak) id<GridShareableItemsProvider>
    incognitoTabsShareableItemsProvider;

// Readwrite override of the UIViewController property. This object will ignore
// the value supplied by UIViewController.
@property(nonatomic, weak, readwrite)
    UIViewController* childViewControllerForStatusBarStyle;

// The view controller for remote tabs.
// TODO(crbug.com/845192) : This was only exposed in the public interface so
// that TabGridViewController does not need to know about model objects. The
// model objects used in this view controller should be factored out.
@property(nonatomic, strong)
    RecentTabsTableViewController* remoteTabsViewController;

// Provides the context menu for the tabs on the grid.
@property(nonatomic, weak) id<TabContextMenuProvider>
    regularTabsContextMenuProvider;
@property(nonatomic, weak) id<TabContextMenuProvider>
    incognitoTabsContextMenuProvider;

// The view controller that shows below the tab grid as a bottom message. Note
// that setting this value immediately adds it to the view hierarchy.
@property(nonatomic, strong) UIViewController* regularTabsBottomMessage;

// The layout guide center to use to refer to the bottom toolbar.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Top and bottom toolbars. Those must be set before -viewDidLoad is called.
@property(nonatomic, strong) TabGridTopToolbar* topToolbar;
@property(nonatomic, strong) TabGridBottomToolbar* bottomToolbar;

// Whether the primary signed-in account is subject to parental controls.
@property(nonatomic, assign) BOOL isSubjectToParentalControls;

// Temporary handler for sending commands to the toolbar.
// TODO(crbug.com/1456659): Remove this.
@property(nonatomic, weak) id<TabGridToolbarsCommandsWrangler>
    toolbarCommandsWrangler;

// Init with tab grid view configuration, which decides which sub view
// controller should be added.
- (instancetype)initWithPageConfiguration:
    (TabGridPageConfiguration)tabGridPageConfiguration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Tells the receiver to prepare for its appearance by pre-requesting any
// resources it needs from data sources. This should be called before any
// transitions are triggered.
- (void)prepareForAppearance;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentDidAppear;
- (void)contentWillDisappearAnimated:(BOOL)animated;

// Dismisses any modal UI which may be presented.
- (void)dismissModals;

// Sets both the current page and page control's selected page to `page`.
// Animation is used if `animated` is YES.
- (void)setCurrentPageAndPageControl:(TabGridPage)page animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
