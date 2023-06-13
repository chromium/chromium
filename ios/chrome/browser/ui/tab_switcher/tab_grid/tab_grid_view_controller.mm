// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

#import <objc/runtime.h>

#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/gestures/view_controller_trait_collection_observer.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller_ui_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/disabled_tab_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_collection_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/thumb_strip_plus_sign_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Not selected tabs opacity in thumbstrip.
const CGFloat kNotSelectedTabsOpacity = 0.8f;

// Types of configurations of this view controller.
typedef NS_ENUM(NSUInteger, TabGridConfiguration) {
  TabGridConfigurationBottomToolbar = 1,
  TabGridConfigurationFloatingButton,
};

// Key of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
const char kUMATabSwitcherPageChangeInteractionHistogram[] =
    "IOS.TabSwitcher.PageChangeInteraction";

// Values of the UMA IOS.TabSwitcher.PageChangeInteraction histogram.
enum class TabSwitcherPageChangeInteraction {
  kNone = 0,
  kScrollDrag = 1,
  kControlTap = 2,
  kControlDrag = 3,
  kItemDrag = 4,
  kMaxValue = kItemDrag,
};

// Computes the page from the offset and width of `scrollView`.
TabGridPage GetPageFromScrollView(UIScrollView* scrollView) {
  CGFloat pageWidth = scrollView.frame.size.width;
  CGFloat offset = scrollView.contentOffset.x;
  NSUInteger page = lround(offset / pageWidth);
  // Fence `page` to valid values; page values of 3 (rounded up from 2.5) are
  // possible, as are large int values if `pageWidth` is somehow very small.
  page = page < TabGridPageIncognitoTabs ? TabGridPageIncognitoTabs : page;
  page = page > TabGridPageRemoteTabs ? TabGridPageRemoteTabs : page;
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract `page` from the highest-
    // index TabGridPage value.
    return static_cast<TabGridPage>(TabGridPageRemoteTabs - page);
  }
  return static_cast<TabGridPage>(page);
}

NSUInteger GetPageIndexFromPage(TabGridPage page) {
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract `page` from the highest-
    // index TabGridPage value.
    return static_cast<NSUInteger>(TabGridPageRemoteTabs - page);
  }
  return static_cast<NSUInteger>(page);
}
}  // namespace

@interface TabGridViewController () <DisabledTabViewControllerDelegate,
                                     GridViewControllerDelegate,
                                     LayoutSwitcher,
                                     PinnedTabsViewControllerDelegate,
                                     RecentTabsTableViewControllerUIDelegate,
                                     SuggestedActionsDelegate,
                                     UIGestureRecognizerDelegate,
                                     UIScrollViewAccessibilityDelegate,
                                     UISearchBarDelegate>
// Whether the view is visible. Bookkeeping is based on
// `-contentWillAppearAnimated:` and
// `-contentWillDisappearAnimated methods. Note that the `Did` methods are not
// reliably called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL viewVisible;
// Child view controllers.
@property(nonatomic, strong) GridViewController* regularTabsViewController;
@property(nonatomic, strong) GridViewController* incognitoTabsViewController;
@property(nonatomic, strong) PinnedTabsViewController* pinnedTabsViewController;

// Disabled tab view controllers shown when a certain browser mode is disabled.
@property(nonatomic, strong)
    DisabledTabViewController* incognitoDisabledTabViewController;
@property(nonatomic, strong)
    DisabledTabViewController* regularDisabledTabViewController;
@property(nonatomic, strong)
    DisabledTabViewController* recentDisabledTabViewController;
// Array holding the child page view controllers.
@property(nonatomic, strong) NSArray<UIViewController*>* pageViewControllers;
// Other UI components.
@property(nonatomic, weak) UIScrollView* scrollView;
@property(nonatomic, weak) UIView* scrollContentView;
// Scrim view to be presented when the search box in focused with no text.
@property(nonatomic, strong) UIControl* scrimView;
@property(nonatomic, weak) TabGridTopToolbar* topToolbar;
@property(nonatomic, weak) TabGridBottomToolbar* bottomToolbar;
@property(nonatomic, assign) TabGridConfiguration configuration;
// Setting the current page doesn't scroll the scroll view; use
// -scrollToPage:animated: for that.
@property(nonatomic, assign) TabGridPage currentPage;
// The UIViewController corresponding with `currentPage`.
@property(nonatomic, readonly) UIViewController* currentPageViewController;
// The frame of `self.view` when it initially appeared.
@property(nonatomic, assign) CGRect initialFrame;
// Whether the scroll view is animating its content offset to the current page.
@property(nonatomic, assign, getter=isScrollViewAnimatingContentOffset)
    BOOL scrollViewAnimatingContentOffset;

// UIView whose background color changes to create a fade-in / fade-out effect
// when revealing / hiding the Thumb Strip.
@property(nonatomic, weak) UIView* foregroundView;
// Button with a plus sign that opens a new tab, located on the right side of
// the thumb strip, shown when the plus sign cell isn't visible.
@property(nonatomic, weak) ThumbStripPlusSignButton* plusSignButton;
// Bottom constraint for `plusSignButton`.
@property(nonatomic, weak) NSLayoutConstraint* plusSignButtonBottomConstraint;
// Constraints for the pinned tabs view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* pinnedTabsConstraints;
// Bottom constraint for the regular tabs bottom message view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* regularTabsBottomMessageConstraints;

// The current state of the tab grid when using the thumb strip.
@property(nonatomic, assign) ViewRevealState currentState;
// The configuration for tab grid pages.
@property(nonatomic, assign) TabGridPageConfiguration pageConfiguration;
// If the scrim view is being presented.
@property(nonatomic, assign) BOOL isScrimDisplayed;
// Wether there is a search being performed in the tab grid or not.
@property(nonatomic, assign) BOOL isPerformingSearch;
// Pan gesture for when the search results view is scrolled during the search
// mode.
@property(nonatomic, strong) UIPanGestureRecognizer* searchResultPanRecognizer;

@property(nonatomic, assign, getter=isDragSeesionInProgress)
    BOOL dragSeesionInProgress;

// YES if it is possible to undo the close all conditions.
@property(nonatomic, assign) BOOL undoCloseAllAvailable;

// The timestamp of the user entering the tab grid.
@property(nonatomic, assign) base::TimeTicks tabGridEnterTime;

@end

@implementation TabGridViewController {
  // Idle page status.
  // Tracks whether the user closed the tab switcher without doing any
  // meaningful action.
  BOOL _idleRegularTabGrid;
  BOOL _idleIncognitoTabGrid;
  BOOL _idleRecentTabs;

  TabGridPage _activePageWhenAppear;
}

// TabGridPaging property.
@synthesize activePage = _activePage;
@synthesize tabGridMode = _tabGridMode;

- (instancetype)initWithPageConfiguration:
    (TabGridPageConfiguration)tabGridPageConfiguration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageConfiguration = tabGridPageConfiguration;
    _dragSeesionInProgress = NO;

    switch (_pageConfiguration) {
      case TabGridPageConfiguration::kAllPagesEnabled:
        _incognitoTabsViewController = [[GridViewController alloc] init];
        _regularTabsViewController = [[GridViewController alloc] init];
        _remoteTabsViewController =
            [[RecentTabsTableViewController alloc] init];
        _pageViewControllers = @[
          _incognitoTabsViewController, _regularTabsViewController,
          _remoteTabsViewController
        ];
        break;
      case TabGridPageConfiguration::kIncognitoPageDisabled:
        _incognitoDisabledTabViewController = [[DisabledTabViewController alloc]
            initWithPage:TabGridPageIncognitoTabs];
        _regularTabsViewController = [[GridViewController alloc] init];
        _remoteTabsViewController =
            [[RecentTabsTableViewController alloc] init];
        _pageViewControllers = @[
          _incognitoDisabledTabViewController, _regularTabsViewController,
          _remoteTabsViewController
        ];
        break;
      case TabGridPageConfiguration::kIncognitoPageOnly:
        _incognitoTabsViewController = [[GridViewController alloc] init];
        _regularDisabledTabViewController = [[DisabledTabViewController alloc]
            initWithPage:TabGridPageRegularTabs];
        _recentDisabledTabViewController = [[DisabledTabViewController alloc]
            initWithPage:TabGridPageRemoteTabs];
        _pageViewControllers = @[
          _incognitoTabsViewController, _regularDisabledTabViewController,
          _recentDisabledTabViewController
        ];
        break;
    }

    if (IsPinnedTabsEnabled()) {
      _pinnedTabsViewController = [[PinnedTabsViewController alloc] init];
    }
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  [self setupScrollView];

  switch (_pageConfiguration) {
    case TabGridPageConfiguration::kAllPagesEnabled:
      [self setupIncognitoTabsViewController];
      [self setupRegularTabsViewController];
      [self setupRemoteTabsViewController];
      break;
    case TabGridPageConfiguration::kIncognitoPageDisabled:
      [self setupDisabledTabViewForPageType:TabGridPageIncognitoTabs];
      [self setupRegularTabsViewController];
      [self setupRemoteTabsViewController];
      break;
    case TabGridPageConfiguration::kIncognitoPageOnly:
      [self setupIncognitoTabsViewController];
      [self setupDisabledTabViewForPageType:TabGridPageRegularTabs];
      [self setupDisabledTabViewForPageType:TabGridPageRemoteTabs];
      break;
  }

  [self setupSearchUI];
  [self setupTopToolbar];
  [self setupBottomToolbar];
  [self setupEditButton];

  if (IsPinnedTabsEnabled()) {
    [self setupPinnedTabsViewController];
  }

  // Hide the toolbars and the floating button, so they can fade in the first
  // time there's a transition into this view controller.
  [self hideToolbars];

  // Mark the non-current page view controllers' contents as hidden for
  // VoiceOver.
  for (UIViewController* pageViewController in self.pageViewControllers) {
    if (pageViewController != self.currentPageViewController) {
      pageViewController.view.accessibilityElementsHidden = YES;
    }
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Modify Incognito and Regular Tabs Insets.
  [self setInsetForGridViews];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak TabGridViewController* weakSelf = self;
  auto animate = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [weakSelf animateTransition:context];
  };
  [coordinator animateAlongsideTransition:animate completion:nil];
}

- (void)animateTransition:
    (id<UIViewControllerTransitionCoordinatorContext>)context {
  // Sync the scroll view offset to the current page value. Since this is
  // invoked inside an animation block, the scrolling doesn't need to be
  // animated.
  [self scrollToPage:_currentPage animated:NO];
  [self configureViewControllerForCurrentSizeClassesAndPage];
  [self setInsetForRemoteTabs];
  [self setInsetForGridViews];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self.traitCollectionObserver viewController:self
                      traitCollectionDidChange:previousTraitCollection];
  if (IsPinnedTabsEnabled()) {
    [self updatePinnedTabsViewControllerConstraints];
  }
  [self updateRegularTabsBottomMessageConstraintsIfExists];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (scrollView.dragging || scrollView.decelerating) {
    // Only when user initiates scroll through dragging.
    CGFloat offsetWidth =
        self.scrollView.contentSize.width - self.scrollView.frame.size.width;
    CGFloat offset = scrollView.contentOffset.x / offsetWidth;
    // In RTL, flip the offset.
    if (UseRTLLayout())
      offset = 1.0 - offset;
    self.topToolbar.pageControl.sliderPosition = offset;

    TabGridPage page = GetPageFromScrollView(scrollView);
    if (page != self.currentPage) {
      self.currentPage = page;
      [self broadcastIncognitoContentVisibility];
      [self configureButtonsForActiveAndCurrentPage];
      // Records when the user drags the scrollView to switch pages.
      [self recordActionSwitchingToPage:_currentPage
                         withInteration:TabSwitcherPageChangeInteraction::
                                            kScrollDrag];
    }
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  // Disable the page control when the user drags on the scroll view since
  // tapping on the page control during scrolling can result in erratic
  // scrolling.
  self.topToolbar.pageControl.userInteractionEnabled = NO;
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // Re-enable the page control since the user isn't dragging anymore.
  self.topToolbar.pageControl.userInteractionEnabled = YES;
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // Update currentPage if scroll view has moved to a new page. Especially
  // important here for 3-finger accessibility swipes since it's not registered
  // as dragging in scrollViewDidScroll:
  TabGridPage page = GetPageFromScrollView(scrollView);
  if (page != self.currentPage) {
    self.currentPage = page;
    [self broadcastIncognitoContentVisibility];
    [self configureButtonsForActiveAndCurrentPage];
  }
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  TabGridPage currentPage = GetPageFromScrollView(scrollView);
  if (currentPage != self.currentPage && self.isDragSeesionInProgress) {
    // This happens when the user drags an item from one scroll view into
    // another.
    [self recordActionSwitchingToPage:currentPage
                       withInteration:TabSwitcherPageChangeInteraction::
                                          kItemDrag];
    [self.topToolbar.pageControl setSelectedPage:currentPage animated:YES];
  }
  self.currentPage = currentPage;
  self.scrollViewAnimatingContentOffset = NO;
  [self broadcastIncognitoContentVisibility];
  [self configureButtonsForActiveAndCurrentPage];
}

#pragma mark - UIScrollViewAccessibilityDelegate

- (NSString*)accessibilityScrollStatusForScrollView:(UIScrollView*)scrollView {
  // This reads the new page whenever the user scrolls in VoiceOver.
  int stringID;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      stringID = IDS_IOS_TAB_GRID_INCOGNITO_TABS_TITLE;
      break;
    case TabGridPageRegularTabs:
      stringID = IDS_IOS_TAB_GRID_REGULAR_TABS_TITLE;
      break;
    case TabGridPageRemoteTabs:
      stringID = IDS_IOS_TAB_GRID_REMOTE_TABS_TITLE;
      break;
  }
  return l10n_util::GetNSString(stringID);
}

#pragma mark - GridTransitionAnimationLayoutProviding properties

- (BOOL)isSelectedCellVisible {
  if (self.activePage != self.currentPage) {
    return NO;
  }

  return [self isSelectedCellVisibleForPage:self.activePage];
}

- (BOOL)shouldReparentSelectedCell:(GridAnimationDirection)animationDirection {
  switch (animationDirection) {
    // For contracting animation only selected pinned cells should be
    // reparented.
    case GridAnimationDirectionContracting:
      return [self isPinnedCellSelected];
    // For expanding animation any selected cell should be reparented.
    case GridAnimationDirectionExpanding:
      return YES;
  }
}

- (GridTransitionLayout*)transitionLayout:(TabGridPage)activePage {
  GridTransitionLayout* layout = [self transitionLayoutForPage:activePage];
  if (!layout) {
    return nil;
  }
  layout.frameChanged = !CGRectEqualToRect(self.view.frame, self.initialFrame);
  return layout;
}

- (UIView*)animationViewsContainer {
  return self.view;
}

- (UIView*)animationViewsContainerBottomView {
  return self.scrollView;
}

#pragma mark - Public Methods

- (void)prepareForAppearance {
  [[self gridViewControllerForPage:self.activePage] prepareForAppearance];
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  [self resetIdlePageStatus];
  self.viewVisible = YES;
  [self.topToolbar.pageControl setSelectedPage:self.currentPage animated:NO];
  _activePageWhenAppear = self.currentPage;
  [self configureViewControllerForCurrentSizeClassesAndPage];

  // The toolbars should be hidden (alpha 0.0) before the tab appears, so that
  // they can be animated in. They can't be set to 0.0 here, because if
  // `animated` is YES, this method is being called inside the animation block.
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForAppearance];
  } else {
    [self showToolbars];
  }
  [self broadcastIncognitoContentVisibility];

  [self.incognitoTabsViewController contentWillAppearAnimated:animated];
  [self.regularTabsViewController contentWillAppearAnimated:animated];
  [self.pinnedTabsViewController contentWillAppearAnimated:animated];

  self.remoteTabsViewController.session = self.view.window.windowScene.session;

  self.remoteTabsViewController.preventUpdates = NO;

  // Record when the tab switcher is presented.
  self.tabGridEnterTime = base::TimeTicks::Now();
}

- (void)contentDidAppear {
  self.initialFrame = self.view.frame;
  // Modify Remote Tabs Insets when page appears and during rotation.
  if (self.remoteTabsViewController) {
    [self setInsetForRemoteTabs];
  }

  // Let the active grid view know the initial appearance is done.
  [[self gridViewControllerForPage:self.activePage] contentDidAppear];
}

- (void)contentWillDisappearAnimated:(BOOL)animated {
  [self recordIdlePageStatus];

  self.undoCloseAllAvailable = NO;
  if (self.tabGridMode != TabGridModeSearch || !animated) {
    // Updating the mode reset the items on the grid, in that case of search
    // mode the animation to show the tab will start from the tab cell after the
    // reset instead of starting from the cell that triggered the navigation.
    self.tabGridMode = TabGridModeNormal;
  }
  [self.regularTabsDelegate discardSavedClosedItems];
  [self.inactiveTabsDelegate discardSavedClosedItems];
  // When the view disappears, the toolbar alpha should be set to 0; either as
  // part of the animation, or directly with -hideToolbars.
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForDisappearance];
  } else {
    [self hideToolbars];
  }

  self.viewVisible = NO;

  [self.incognitoTabsViewController contentWillDisappear];
  [self.regularTabsViewController contentWillDisappear];
  [self.pinnedTabsViewController contentWillDisappear];
  self.remoteTabsViewController.preventUpdates = YES;

  self.tabGridEnterTime = base::TimeTicks();
}

- (void)dismissModals {
  [self.regularTabsConsumer dismissModals];
  [self.pinnedTabsConsumer dismissModals];
  [self.incognitoTabsConsumer dismissModals];
  [self.remoteTabsViewController dismissModals];
}

- (void)setCurrentPageAndPageControl:(TabGridPage)page animated:(BOOL)animated {
  [self updatePageWithCurrentSearchTerms:page];

  if (self.topToolbar.pageControl.selectedPage != page)
    [self.topToolbar.pageControl setSelectedPage:page animated:animated];
  if (self.currentPage != page) {
    [self scrollToPage:page animated:animated];
  }
}

// Sets the current search terms on `page`. This allows the content to update
// while the page is still hidden before the page change animation begins.
- (void)updatePageWithCurrentSearchTerms:(TabGridPage)page {
  if (self.tabGridMode != TabGridModeSearch ||
      self.currentPage == TabGridPageIncognitoTabs) {
    // No need to update search term if not in search mode or currently on the
    // incognito page.
    return;
  }

  NSString* searchTerms = nil;
  if (self.currentPage == TabGridPageRegularTabs) {
    searchTerms = self.regularTabsViewController.searchText;
  } else {
    searchTerms = self.remoteTabsViewController.searchTerms;
  }

  if (page == TabGridPageRegularTabs) {
    // Search terms will be non-empty when switching pages. This is important
    // because `searchItemsWithText:` will show items from all windows. When no
    // search terms exist, `resetToAllItems` is used instead.
    DCHECK(searchTerms.length);
    self.regularTabsViewController.searchText = searchTerms;
    [self.regularTabsDelegate searchItemsWithText:searchTerms];
  } else {
    self.remoteTabsViewController.searchTerms = searchTerms;
  }
}

#pragma mark - Public Properties

- (id<TabCollectionConsumer>)regularTabsConsumer {
  return self.regularTabsViewController;
}

- (void)setPriceCardDataSource:(id<PriceCardDataSource>)priceCardDataSource {
  self.regularTabsViewController.priceCardDataSource = priceCardDataSource;
  _priceCardDataSource = priceCardDataSource;
}

- (id<TabCollectionConsumer>)pinnedTabsConsumer {
  return self.pinnedTabsViewController;
}

- (id<TabCollectionConsumer>)incognitoTabsConsumer {
  return self.incognitoTabsViewController;
}

- (id<RecentTabsConsumer>)remoteTabsConsumer {
  return self.remoteTabsViewController;
}

- (void)setRegularTabsShareableItemsProvider:
    (id<GridShareableItemsProvider>)provider {
  self.regularTabsViewController.shareableItemsProvider = provider;
  _regularTabsShareableItemsProvider = provider;
}

- (void)setIncognitoTabsShareableItemsProvider:
    (id<GridShareableItemsProvider>)provider {
  self.incognitoTabsViewController.shareableItemsProvider = provider;
  _incognitoTabsShareableItemsProvider = provider;
}

- (void)setReauthHandler:(id<IncognitoReauthCommands>)reauthHandler {
  if (_reauthHandler == reauthHandler)
    return;
  _reauthHandler = reauthHandler;
  self.incognitoTabsViewController.reauthHandler = self.reauthHandler;
}

- (void)setRegularThumbStripHandler:(id<ThumbStripCommands>)handler {
  if (_regularThumbStripHandler == handler)
    return;
  _regularThumbStripHandler = handler;
  self.regularTabsViewController.thumbStripHandler =
      self.regularThumbStripHandler;
}

- (void)setIncognitoThumbStripHandler:(id<ThumbStripCommands>)handler {
  if (_incognitoThumbStripHandler == handler)
    return;
  _incognitoThumbStripHandler = handler;
  self.regularTabsViewController.thumbStripHandler =
      self.incognitoThumbStripHandler;
}

- (void)setRegularTabsContextMenuProvider:(id<TabContextMenuProvider>)provider {
  if (_regularTabsContextMenuProvider == provider)
    return;
  _regularTabsContextMenuProvider = provider;

  self.regularTabsViewController.menuProvider = provider;
  if (IsPinnedTabsEnabled()) {
    self.pinnedTabsViewController.menuProvider = provider;
  }
}

- (void)setIncognitoTabsContextMenuProvider:
    (id<TabContextMenuProvider>)provider {
  if (_incognitoTabsContextMenuProvider == provider)
    return;
  _incognitoTabsContextMenuProvider = provider;

  self.incognitoTabsViewController.menuProvider = provider;
}

- (void)setReauthAgent:(IncognitoReauthSceneAgent*)reauthAgent {
  if (_reauthAgent) {
    [_reauthAgent removeObserver:self];
  }

  _reauthAgent = reauthAgent;

  [_reauthAgent addObserver:self];
}

- (void)setRegularTabsBottomMessage:(UIViewController*)bottomMessage {
  if (_regularTabsBottomMessage == bottomMessage) {
    return;
  }
  [_regularTabsBottomMessage willMoveToParentViewController:nil];
  [_regularTabsBottomMessage.view removeFromSuperview];
  [_regularTabsBottomMessage removeFromParentViewController];
  _regularTabsBottomMessage = bottomMessage;
  if (!_regularTabsBottomMessage) {
    [self slideOutRegularTabsBottomMessage];
    return;
  }
  [self addChildViewController:self.regularTabsBottomMessage];
  [self.view addSubview:self.regularTabsBottomMessage.view];
  [self.regularTabsBottomMessage didMoveToParentViewController:self];
  [self initializeRegularTabsBottomMessageView];
}

#pragma mark - TabGridPaging

- (void)setActivePage:(TabGridPage)activePage {
  [self scrollToPage:activePage animated:YES];
  _activePage = activePage;
}

#pragma mark - TabGridMode

- (void)setTabGridMode:(TabGridMode)mode {
  if (_tabGridMode == mode) {
    return;
  }
  TabGridMode previousMode = _tabGridMode;
  _tabGridMode = mode;

  // Updating toolbars first before the controllers so when they set their
  // content they will account for the updated insets of the toolbars.
  self.topToolbar.mode = self.tabGridMode;
  self.bottomToolbar.mode = self.tabGridMode;

  // Resetting search state when leaving the search mode should happen before
  // changing the mode in the controllers so when they do the cleanup for the
  // new mode they will have the correct items (tabs).
  if (previousMode == TabGridModeSearch) {
    self.remoteTabsViewController.searchTerms = nil;
    self.regularTabsViewController.searchText = nil;
    self.incognitoTabsViewController.searchText = nil;
    [self.regularTabsDelegate resetToAllItems];
    [self.incognitoTabsDelegate resetToAllItems];
    [self hideScrim];
  }

  // Reset the visibility of bottom message, if exists.
  if (self.regularTabsBottomMessage) {
    self.regularTabsBottomMessage.view.hidden =
        self.tabGridMode != TabGridModeNormal;
  }

  [self setInsetForGridViews];
  self.regularTabsViewController.mode = self.tabGridMode;
  self.incognitoTabsViewController.mode = self.tabGridMode;

  self.scrollView.scrollEnabled = (self.tabGridMode == TabGridModeNormal);
  if (mode == TabGridModeSelection)
    [self updateSelectionModeToolbars];
}

#pragma mark - LayoutSwitcherProvider

- (id<LayoutSwitcher>)layoutSwitcher {
  return self;
}

#pragma mark - LayoutSwitcher

- (LayoutSwitcherState)currentLayoutSwitcherState {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  return gridViewController.currentLayoutSwitcherState;
}

- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];

  __block NSMutableArray<NSNumber*>* completeds = [[NSMutableArray alloc] init];
  __block NSMutableArray<NSNumber*>* finisheds = [[NSMutableArray alloc] init];

  void (^combinedCompletion)(BOOL, BOOL) = ^(BOOL completed, BOOL finished) {
    [completeds addObject:[NSNumber numberWithBool:completed]];
    [finisheds addObject:[NSNumber numberWithBool:finished]];
    if ([completeds count] != 2) {
      return;
    }
    DCHECK(completeds[0] == completeds[1]);
    DCHECK(finisheds[0] == finisheds[1]);
    completion(completeds[0], finisheds[0]);
  };

  // Each LayoutSwitcher method calls regular and incognito grid controller's
  // corresponding method. Thus, attaching the completion to only one of the
  // grid view controllers should suffice.
  [regularViewController willTransitionToLayout:nextState
                                     completion:combinedCompletion];
  [incognitoViewController willTransitionToLayout:nextState
                                       completion:combinedCompletion];
}

- (void)didUpdateTransitionLayoutProgress:(CGFloat)progress {
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  [regularViewController didUpdateTransitionLayoutProgress:progress];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];
  [incognitoViewController didUpdateTransitionLayoutProgress:progress];
}

- (void)didTransitionToLayoutSuccessfully:(BOOL)success {
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  [regularViewController didTransitionToLayoutSuccessfully:success];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];
  [incognitoViewController didTransitionToLayoutSuccessfully:success];
}

#pragma mark - ViewRevealingAnimatee

- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState {
  self.currentState = currentViewRevealState;
  self.scrollView.scrollEnabled = NO;
  [self updateNotSelectedTabCellOpacityForState:currentViewRevealState];
  // Reset tab grid mode.
  self.tabGridMode = TabGridModeNormal;
  switch (currentViewRevealState) {
    case ViewRevealState::Hidden: {
      // If the tab grid is just showing up, make sure that the active page is
      // current. This can happen when the user closes the tab grid using the
      // done button on RecentTabs. The current page would stay RecentTabs, but
      // the active page comes from the currently displayed BVC.
      if (self.delegate) {
        self.activePage =
            [self.delegate activePageForTabGridViewController:self];
      }
      if (self.currentPage != self.activePage) {
        [self scrollToPage:self.activePage animated:NO];
      }
      self.topToolbar.transform = CGAffineTransformMakeTranslation(
          0, [self hiddenTopToolbarYTranslation]);
      GridViewController* regularViewController =
          [self gridViewControllerForPage:TabGridPageRegularTabs];
      regularViewController.gridView.transform =
          CGAffineTransformMakeTranslation(0, kThumbStripSlideInHeight);
      GridViewController* incognitoViewController =
          [self gridViewControllerForPage:TabGridPageIncognitoTabs];
      incognitoViewController.gridView.transform =
          CGAffineTransformMakeTranslation(0, kThumbStripSlideInHeight);
      // Don't do any animation in the tab grid. All that animation will be
      // controlled by the pan handler/-animateViewReveal:.
      [self contentWillAppearAnimated:NO];
      break;
    }
    case ViewRevealState::Peeked:
      break;
    case ViewRevealState::Revealed:
      self.plusSignButton.alpha = 0;
      break;
  }
  switch (nextViewRevealState) {
    case ViewRevealState::Hidden:
    case ViewRevealState::Peeked:
      self.plusSignButtonBottomConstraint.constant = kThumbStripHeight;
      break;
    case ViewRevealState::Revealed:
      // Increase height of button while hiding it, for a smooth animation.
      self.plusSignButtonBottomConstraint.constant =
          self.view.frame.size.height;
      break;
  }
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
  [self updateNotSelectedTabCellOpacityForState:nextViewRevealState];
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];
  switch (nextViewRevealState) {
    case ViewRevealState::Hidden: {
      self.foregroundView.alpha = 1;
      self.topToolbar.transform = CGAffineTransformMakeTranslation(
          0, [self hiddenTopToolbarYTranslation]);
      regularViewController.gridView.transform =
          CGAffineTransformMakeTranslation(0, kThumbStripSlideInHeight);
      incognitoViewController.gridView.transform =
          CGAffineTransformMakeTranslation(0, kThumbStripSlideInHeight);
      self.topToolbar.alpha = 0;
      GridViewController* currentGridViewController =
          [self gridViewControllerForPage:self.currentPage];
      [self showPlusSignButtonWithAlpha:1 - currentGridViewController
                                                .fractionVisibleOfLastItem];
      [self contentWillDisappearAnimated:YES];
      self.plusSignButton.transform =
          CGAffineTransformMakeTranslation(0, kThumbStripSlideInHeight);
      break;
    }
    case ViewRevealState::Peeked: {
      self.foregroundView.alpha = 0;
      self.topToolbar.transform = CGAffineTransformMakeTranslation(
          0, [self hiddenTopToolbarYTranslation]);
      regularViewController.gridView.transform = CGAffineTransformIdentity;
      incognitoViewController.gridView.transform = CGAffineTransformIdentity;
      self.topToolbar.alpha = 0;
      GridViewController* currentGridViewController =
          [self gridViewControllerForPage:self.currentPage];
      [self showPlusSignButtonWithAlpha:1 - currentGridViewController
                                                .fractionVisibleOfLastItem];
      break;
    }
    case ViewRevealState::Revealed: {
      self.foregroundView.alpha = 0;
      self.topToolbar.transform = CGAffineTransformIdentity;
      regularViewController.gridView.transform =
          CGAffineTransformMakeTranslation(
              0, self.topToolbar.intrinsicContentSize.height);
      incognitoViewController.gridView.transform =
          CGAffineTransformMakeTranslation(
              0, self.topToolbar.intrinsicContentSize.height);
      self.topToolbar.alpha = 1;
      [self hidePlusSignButton];
      break;
    }
  }
}

- (void)didAnimateViewRevealFromState:(ViewRevealState)startViewRevealState
                              toState:(ViewRevealState)currentViewRevealState
                              trigger:(ViewRevealTrigger)trigger {
  [self updateNotSelectedTabCellOpacityForState:currentViewRevealState];
  self.currentState = currentViewRevealState;
  // Update a11y visibility for browser and grid. Both should be visible
  // when in Peeked mode, and only one visible in the other two modes.
  BOOL updateAccessibility = NO;
  switch (currentViewRevealState) {
    case ViewRevealState::Hidden:
      [self.delegate tabGridViewControllerDidDismiss:self];
      self.view.accessibilityViewIsModal = NO;
      [self.delegate setBVCAccessibilityViewModal:YES];
      updateAccessibility = YES;
      break;
    case ViewRevealState::Peeked:
      self.view.accessibilityViewIsModal = NO;
      [self.delegate setBVCAccessibilityViewModal:NO];
      updateAccessibility = startViewRevealState == ViewRevealState::Hidden;
      break;
    case ViewRevealState::Revealed:
      self.scrollView.scrollEnabled = YES;
      [self setInsetForRemoteTabs];
      [self.delegate dismissBVC];
      self.view.accessibilityViewIsModal = YES;
      [self.delegate setBVCAccessibilityViewModal:NO];
      updateAccessibility = startViewRevealState == ViewRevealState::Hidden;
      break;
  }
  if (updateAccessibility) {
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    nil);
  }
}

// Sets the expected opacity level for each view revealing state.
- (void)updateNotSelectedTabCellOpacityForState:(ViewRevealState)state {
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];
  switch (state) {
    case ViewRevealState::Hidden:
    case ViewRevealState::Peeked:
      regularViewController.notSelectedTabCellOpacity = kNotSelectedTabsOpacity;
      incognitoViewController.notSelectedTabCellOpacity =
          kNotSelectedTabsOpacity;
      break;
    case ViewRevealState::Revealed:
      regularViewController.notSelectedTabCellOpacity = 1.0f;
      incognitoViewController.notSelectedTabCellOpacity = 1.0f;
      break;
  }
}

#pragma mark - Private

// Records the idle page status for the current `currentPage`.
- (void)recordIdlePageStatus {
  if (!self.viewVisible) {
    return;
  }

  // If the page has changed, the idle status of tab grid pages is `NO`.
  BOOL onSamePage = self.currentPage == _activePageWhenAppear;

  switch (self.currentPage) {
    case TabGridPage::TabGridPageIncognitoTabs:
      base::UmaHistogramBoolean(
          kUMATabSwitcherIdleIncognitoTabGridPageHistogram,
          _idleIncognitoTabGrid && onSamePage);
      break;
    case TabGridPage::TabGridPageRegularTabs:
      base::UmaHistogramBoolean(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                _idleRegularTabGrid && onSamePage);
      break;
    case TabGridPage::TabGridPageRemoteTabs:
      base::UmaHistogramBoolean(kUMATabSwitcherIdleRecentTabsHistogram,
                                _idleRecentTabs);
      break;
  }
}

// Sets the idle page status of the `currentPage`.
- (void)setCurrentIdlePageStatus:(BOOL)idlePageStatus {
  if (!self.viewVisible) {
    return;
  }

  switch (self.currentPage) {
    case TabGridPage::TabGridPageIncognitoTabs:
      _idleIncognitoTabGrid = idlePageStatus;
      break;
    case TabGridPage::TabGridPageRegularTabs:
      _idleRegularTabGrid = idlePageStatus;
      break;
    case TabGridPage::TabGridPageRemoteTabs:
      _idleRecentTabs = idlePageStatus;
      break;
  }
}

// Resets idle page status.
- (void)resetIdlePageStatus {
  _idleIncognitoTabGrid = YES;
  _idleRegularTabGrid = YES;
  // `_idleRecentTabs` is set to 'YES' if the "Done" button has been tapped from
  // the "TabGridPageRemoteTabs" or if the page has changed.
  _idleRecentTabs = NO;
}

// Returns wether there is a selected pinned cell.
- (BOOL)isPinnedCellSelected {
  if (!IsPinnedTabsEnabled() || self.currentPage != TabGridPageRegularTabs) {
    return NO;
  }

  return [self.pinnedTabsViewController hasSelectedCell];
}

// Returns whether selcted cell is visible for the provided `page`.
- (BOOL)isSelectedCellVisibleForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return self.incognitoTabsViewController.selectedCellVisible;
    case TabGridPageRegularTabs:
      return [self isSelectedCellVisibleForRegularTabsPage];
    case TabGridPageRemoteTabs:
      return NO;
  }
}

// Returns whether selcted cell is visible for the regular tabs `page`.
- (BOOL)isSelectedCellVisibleForRegularTabsPage {
  BOOL isSelectedCellVisible =
      self.regularTabsViewController.selectedCellVisible;

  if (IsPinnedTabsEnabled()) {
    isSelectedCellVisible |= self.pinnedTabsViewController.selectedCellVisible;
  }

  return isSelectedCellVisible;
}

// Returns transition layout for the provided `page`.
- (GridTransitionLayout*)transitionLayoutForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return [self.incognitoTabsViewController transitionLayout];
    case TabGridPageRegularTabs:
      return [self transitionLayoutForRegularTabsPage];
    case TabGridPageRemoteTabs:
      return nil;
  }
}

// Returns transition layout provider for the regular tabs page.
- (GridTransitionLayout*)transitionLayoutForRegularTabsPage {
  GridTransitionLayout* regularTabsTransitionLayout =
      [self.regularTabsViewController transitionLayout];

  if (IsPinnedTabsEnabled()) {
    GridTransitionLayout* pinnedTabsTransitionLayout =
        [self.pinnedTabsViewController transitionLayout];

    return [self combineTransitionLayout:regularTabsTransitionLayout
                    withTransitionLayout:pinnedTabsTransitionLayout];
  }

  return regularTabsTransitionLayout;
}

// Combines two transition layouts into one. The `primaryLayout` has the
// priority over `secondaryLayout`. This means that in case there are two
// activeItems and/or two selectionItems available, only the ones from
// `primaryLayout` would be picked for a combined layout.
- (GridTransitionLayout*)
    combineTransitionLayout:(GridTransitionLayout*)primaryLayout
       withTransitionLayout:(GridTransitionLayout*)secondaryLayout {
  NSArray<GridTransitionItem*>* primaryInactiveItems =
      primaryLayout.inactiveItems;
  NSArray<GridTransitionItem*>* secondaryInactiveItems =
      secondaryLayout.inactiveItems;

  NSArray<GridTransitionItem*>* inactiveItems =
      [self combineInactiveItems:primaryInactiveItems
               withInactiveItems:secondaryInactiveItems];

  GridTransitionActiveItem* primaryActiveItem = primaryLayout.activeItem;
  GridTransitionActiveItem* secondaryActiveItem = secondaryLayout.activeItem;

  // Prefer primary active item.
  GridTransitionActiveItem* activeItem =
      primaryActiveItem ? primaryActiveItem : secondaryActiveItem;

  GridTransitionItem* primarySelectionItem = primaryLayout.selectionItem;
  GridTransitionItem* secondarySelectionItem = secondaryLayout.selectionItem;

  // Prefer primary selection item.
  GridTransitionItem* selectionItem =
      primarySelectionItem ? primarySelectionItem : secondarySelectionItem;

  return [GridTransitionLayout layoutWithInactiveItems:inactiveItems
                                            activeItem:activeItem
                                         selectionItem:selectionItem];
}

// Combines two arrays of inactive items into one. The `primaryInactiveItems`
// (if any) would be placed in the front of the resulting array, whether the
// `secondaryInactiveItems` would be placed in the back.
- (NSArray<GridTransitionItem*>*)
    combineInactiveItems:(NSArray<GridTransitionItem*>*)primaryInactiveItems
       withInactiveItems:(NSArray<GridTransitionItem*>*)secondaryInactiveItems {
  if (primaryInactiveItems == nil) {
    primaryInactiveItems = @[];
  }

  return [primaryInactiveItems
      arrayByAddingObjectsFromArray:secondaryInactiveItems];
}

// Hides the thumb strip's plus sign button by translating it away and making it
// transparent.
- (void)hidePlusSignButton {
  CGFloat xDistance = UseRTLLayout()
                          ? -kThumbStripPlusSignButtonSlideOutDistance
                          : kThumbStripPlusSignButtonSlideOutDistance;
  self.plusSignButton.transform =
      CGAffineTransformMakeTranslation(xDistance, 0);
  self.plusSignButton.alpha = 0;
}

// Show the thumb strip's plus sign button by translating it back into position
// and setting its alpha to `opacity`.
- (void)showPlusSignButtonWithAlpha:(CGFloat)opacity {
  self.plusSignButton.transform = CGAffineTransformIdentity;
  self.plusSignButton.alpha = opacity;
}

// Returns the ammount by which the top toolbar should be translated in the y
// direction when hidden. Used for the slide-in animation.
- (CGFloat)hiddenTopToolbarYTranslation {
  return -self.topToolbar.frame.size.height -
         self.scrollView.safeAreaInsets.top;
}

// Sets the proper insets for the Remote Tabs ViewController to accomodate for
// the safe area, toolbar, and status bar.
- (void)setInsetForRemoteTabs {
  // Sync the scroll view offset to the current page value if the scroll view
  // isn't scrolling. Don't animate this.
  if (!self.scrollView.dragging && !self.scrollView.decelerating) {
    [self scrollToPage:self.currentPage animated:NO];
  }
  // The content inset of the tab grids must be modified so that the toolbars
  // do not obscure the tabs. This may change depending on orientation.
  CGFloat bottomInset = self.bottomToolbar.intrinsicContentSize.height;
  UIEdgeInsets inset = UIEdgeInsetsMake(
      self.topToolbar.intrinsicContentSize.height, 0, bottomInset, 0);
  // Left and right side could be missing correct safe area
  // inset upon rotation. Manually correct it.
  self.remoteTabsViewController.additionalSafeAreaInsets = UIEdgeInsetsZero;
  UIEdgeInsets additionalSafeArea = inset;
  UIEdgeInsets safeArea = self.scrollView.safeAreaInsets;
  // If Remote Tabs isn't on the screen, it will not have the right safe area
  // insets. Pass down the safe area insets of the scroll view.
  if (self.currentPage != TabGridPageRemoteTabs) {
    additionalSafeArea.right = safeArea.right;
    additionalSafeArea.left = safeArea.left;
  }

  // Ensure that the View Controller doesn't have safe area inset that already
  // covers the view's bounds. This can happen in tests.
  if (!CGRectIsEmpty(UIEdgeInsetsInsetRect(
          self.remoteTabsViewController.tableView.bounds,
          self.remoteTabsViewController.tableView.safeAreaInsets))) {
    self.remoteTabsViewController.additionalSafeAreaInsets = additionalSafeArea;
  }
}

// Sets the proper insets for the Grid ViewControllers to accomodate for the
// safe area and toolbars.
- (void)setInsetForGridViews {
  // Sync the scroll view offset to the current page value if the scroll view
  // isn't scrolling. Don't animate this.
  if (!self.scrollViewAnimatingContentOffset && !self.scrollView.dragging &&
      !self.scrollView.decelerating) {
    [self scrollToPage:self.currentPage animated:NO];
  }

  self.incognitoTabsViewController.gridView.contentInset =
      [self calculateInsetForIncognitoGridView];
  self.regularTabsViewController.gridView.contentInset =
      [self calculateInsetForRegularGridView];
}

// Returns the corresponding GridViewController for `page`. Returns `nil` if
// page does not have a corresponding GridViewController.
- (GridViewController*)gridViewControllerForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return self.incognitoTabsViewController;
    case TabGridPageRegularTabs:
      return self.regularTabsViewController;
    case TabGridPageRemoteTabs:
      return nil;
  }
}

- (void)setCurrentPage:(TabGridPage)currentPage {
  BOOL samePage = _currentPage == currentPage;

  // Record the idle metric if the previous page was `TabGridPageRemoteTabs`.
  if (!samePage && _currentPage == TabGridPageRemoteTabs) {
    [self setCurrentIdlePageStatus:YES];
    [self recordIdlePageStatus];
    [self setCurrentIdlePageStatus:NO];
  }

  // Original current page is about to not be visible. Disable it from being
  // focused by VoiceOver.
  self.currentPageViewController.view.accessibilityElementsHidden = YES;
  UIViewController* previousPageVC = self.currentPageViewController;
  _currentPage = currentPage;
  self.currentPageViewController.view.accessibilityElementsHidden = NO;

  if (self.tabGridMode == TabGridModeSearch) {
    // `UIAccessibilityLayoutChangedNotification` doesn't change the current
    // item focused by the voiceOver if the notification argument provided with
    // it is `nil`. In search mode, the item focused by the voiceOver needs to
    // be reset and to do that `UIAccessibilityScreenChangedNotification` should
    // be posted instead.
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    nil);
    // If the search mode is active. the previous page should have the result
    // gesture recognizer installed, make sure to move the gesture recognizer to
    // the new page's view.
    [previousPageVC.view
        removeGestureRecognizer:self.searchResultPanRecognizer];
    [self.currentPageViewController.view
        addGestureRecognizer:self.searchResultPanRecognizer];
  } else {
    UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                    nil);
  }
  if (IsPinnedTabsEnabled()) {
    const BOOL pinnedTabsAvailable =
        currentPage == TabGridPage::TabGridPageRegularTabs &&
        self.tabGridMode == TabGridModeNormal;
    [self.pinnedTabsViewController pinnedTabsAvailable:pinnedTabsAvailable];
  }
  [self updateToolbarsAppearance];
  [self setupEditButton];
  // Make sure the current page becomes the first responder, so that it can
  // register and handle key commands.
  [self.currentPageViewController becomeFirstResponder];
}

// Sets the value of `currentPage`, adjusting the position of the scroll view
// to match. If `animated` is YES, the scroll view change may animate; if it is
// NO, it will never animate.
- (void)scrollToPage:(TabGridPage)targetPage animated:(BOOL)animated {
  // This method should never early return if `targetPage` == `_currentPage`;
  // the ivar may have been set before the scroll view could be updated. Calling
  // this method should always update the scroll view's offset if possible.

  // When VoiceOver is running, the animation can cause state to get out of
  // sync. If the user swipes right during the animation, the VoiceOver cursor
  // goes to the old page, instead of the new page. See crbug.com/978673 for
  // more details.
  if (UIAccessibilityIsVoiceOverRunning()) {
    animated = NO;
  }

  // If the view isn't loaded yet, just do bookkeeping on `currentPage`.
  if (!self.viewLoaded) {
    self.currentPage = targetPage;
    return;
  }

  CGFloat pageWidth = self.scrollView.frame.size.width;
  NSUInteger pageIndex = GetPageIndexFromPage(targetPage);
  CGPoint targetOffset = CGPointMake(pageIndex * pageWidth, 0);
  BOOL changed = self.currentPage != targetPage;
  BOOL scrolled =
      !CGPointEqualToPoint(self.scrollView.contentOffset, targetOffset);

  // If the view is visible and `animated` is YES, animate the change.
  // Otherwise don't.
  if (!self.viewVisible || !animated) {
    [self.scrollView setContentOffset:targetOffset animated:NO];
    self.currentPage = targetPage;
    // Important updates (e.g., button configurations, incognito visibility) are
    // made at the end of scrolling animations after `self.currentPage` is set.
    // Since this codepath has no animations, updates must be called manually.
    [self broadcastIncognitoContentVisibility];
    [self configureButtonsForActiveAndCurrentPage];
  } else {
    // Only set `scrollViewAnimatingContentOffset` to YES if there's an actual
    // change in the contentOffset, as `-scrollViewDidEndScrollingAnimation:` is
    // never called if the animation does not occur.
    if (scrolled) {
      self.scrollViewAnimatingContentOffset = YES;
      [self.scrollView setContentOffset:targetOffset animated:YES];
      // `self.currentPage` is set in scrollViewDidEndScrollingAnimation:
    } else {
      self.currentPage = targetPage;
      if (changed) {
        // When there is no scrolling and the page changed, it can be due to
        // the user dragging the slider and dropping it right on the spot.
        // Something easy to reproduce with the two edges (incognito / recent
        // tabs), but also possible with middle position (normal).
        [self broadcastIncognitoContentVisibility];
        [self configureButtonsForActiveAndCurrentPage];
      }
    }
  }

  // TODO(crbug.com/872303) : This is a workaround because TabRestoreService
  // does not notify observers when entries are removed. When close all tabs
  // removes entries, the remote tabs page in the tab grid are not updated. This
  // ensures that the table is updated whenever scrolling to it.
  if (targetPage == TabGridPageRemoteTabs && (changed || scrolled)) {
    [self.remoteTabsViewController loadModel];
    [self.remoteTabsViewController.tableView reloadData];
  }
}

- (UIViewController*)currentPageViewController {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      return self.incognitoTabsViewController
                 ? self.incognitoTabsViewController
                 : self.incognitoDisabledTabViewController;
    case TabGridPageRegularTabs:
      return self.regularTabsViewController
                 ? self.regularTabsViewController
                 : self.regularDisabledTabViewController;
    case TabGridPageRemoteTabs:
      return self.remoteTabsViewController
                 ? self.remoteTabsViewController
                 : self.recentDisabledTabViewController;
  }
}

- (void)setScrollViewAnimatingContentOffset:
    (BOOL)scrollViewAnimatingContentOffset {
  if (_scrollViewAnimatingContentOffset == scrollViewAnimatingContentOffset)
    return;
  _scrollViewAnimatingContentOffset = scrollViewAnimatingContentOffset;
}

// Adds the scroll view and sets constraints.
- (void)setupScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.scrollEnabled = YES;
  scrollView.pagingEnabled = YES;
  scrollView.delegate = self;
  // Ensures that scroll view does not add additional margins based on safe
  // areas.
  scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  UIView* contentView = [[UIView alloc] init];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:contentView];
  [self.view addSubview:scrollView];
  self.scrollContentView = contentView;
  self.scrollView = scrollView;
  self.scrollView.scrollEnabled = !self.isThumbStripEnabled;
  self.scrollView.accessibilityIdentifier = kTabGridScrollViewIdentifier;
  NSArray* constraints = @[
    [contentView.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [contentView.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
    [contentView.leadingAnchor
        constraintEqualToAnchor:scrollView.leadingAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:scrollView.trailingAnchor],
    [contentView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor],
    [scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the incognito tabs GridViewController as a contained view controller,
// and sets constraints.
- (void)setupIncognitoTabsViewController {
  UIView* contentView = self.scrollContentView;
  GridViewController* viewController = self.incognitoTabsViewController;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  viewController.view.accessibilityIdentifier = kIncognitoTabGridIdentifier;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageIncognitoTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridIncognitoTabsEmptyStateIdentifier;
  viewController.theme = GridThemeDark;
  viewController.delegate = self;
  viewController.dragDropHandler = self.incognitoTabsDragDropHandler;
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:contentView.leadingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the regular tabs GridViewController as a contained view controller,
// and sets constraints.
- (void)setupRegularTabsViewController {
  UIView* contentView = self.scrollContentView;
  GridViewController* viewController = self.regularTabsViewController;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  viewController.view.accessibilityIdentifier = kRegularTabGridIdentifier;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridRegularTabsEmptyStateIdentifier;
  viewController.theme = GridThemeLight;
  viewController.delegate = self;
  viewController.dragDropHandler = self.regularTabsDragDropHandler;
  viewController.suggestedActionsDelegate = self;
  UIViewController* leadingSideViewController =
      self.incognitoTabsViewController
          ? self.incognitoTabsViewController
          : self.incognitoDisabledTabViewController;

  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:leadingSideViewController.view.trailingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the remote tabs view controller as a contained view controller, and
// sets constraints.
- (void)setupRemoteTabsViewController {
  RecentTabsTableViewController* viewController = self.remoteTabsViewController;
  viewController.UIDelegate = self;

  // TODO(crbug.com/804589) : Dark style on remote tabs.
  // The styler must be set before the view controller is loaded.
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  styler.tableViewBackgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  viewController.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  viewController.styler = styler;

  UIView* contentView = self.scrollContentView;
  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:self.regularTabsViewController.view
                                    .trailingAnchor],
    [viewController.view.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds a DisabledTabViewController as a contained view controller, and sets
// constraints.
- (void)setupDisabledTabViewForPageType:(TabGridPage)pageType {
  UIView* contentView = self.scrollContentView;
  DisabledTabViewController* viewController;
  NSLayoutConstraint* leadingAnchorConstraint;

  switch (pageType) {
    case TabGridPage::TabGridPageIncognitoTabs:
      viewController = self.incognitoDisabledTabViewController;
      leadingAnchorConstraint = [viewController.view.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor];
      break;
    case TabGridPage::TabGridPageRegularTabs:
      viewController = self.regularDisabledTabViewController;
      leadingAnchorConstraint = [viewController.view.leadingAnchor
          constraintEqualToAnchor:self.incognitoTabsViewController.view
                                      .trailingAnchor];
      break;
    case TabGridPage::TabGridPageRemoteTabs:
      viewController = self.recentDisabledTabViewController;
      leadingAnchorConstraint = [viewController.view.leadingAnchor
          constraintEqualToAnchor:self.regularDisabledTabViewController.view
                                      .trailingAnchor];
      break;
  }

  viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.delegate = self;

  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    leadingAnchorConstraint,
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];

  if (pageType == TabGridPage::TabGridPageRemoteTabs) {
    NSLayoutConstraint* trailingConstraint = [viewController.view.trailingAnchor
        constraintEqualToAnchor:contentView.trailingAnchor];
    trailingConstraint.active = YES;
  }
}

- (void)setupEditButton API_AVAILABLE(ios(14.0)) {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:MenuScenarioHistogram::kTabGridEdit];
  __weak TabGridViewController* weakSelf = self;
  NSMutableArray<UIMenuElement*>* menuElements =
      [@[ [actionFactory actionToCloseAllTabsWithBlock:^{
        [weakSelf closeAllButtonTapped:nil];
      }] ] mutableCopy];
  // Disable the "Select All" option from the edit button when there is no tabs
  // in the regular tab grid. "Close All" can still be called if there is
  // element in inactive tabs.
  BOOL disabledSelectAll = self.currentPage == TabGridPageRegularTabs &&
                           self.regularTabsViewController.isGridEmpty;
  if (!disabledSelectAll) {
    [menuElements addObject:[actionFactory actionToSelectTabsWithBlock:^{
                    [weakSelf selectTabsButtonTapped:nil];
                  }]];
  }

  UIMenu* menu = [UIMenu menuWithChildren:menuElements];
  [self.topToolbar setEditButtonMenu:menu];
  [self.bottomToolbar setEditButtonMenu:menu];
}

// Adds the top toolbar and sets constraints.
- (void)setupTopToolbar {
  // In iOS 13+, constraints break if the UIToolbar is initialized with a null
  // or zero rect frame. An arbitrary non-zero frame fixes this issue.
  TabGridTopToolbar* topToolbar =
      [[TabGridTopToolbar alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  self.topToolbar = topToolbar;
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topToolbar];

  // Sets the leadingButton title during initialization allows the actionSheet
  // to be correctly anchored. See: crbug.com/1140982.
  [topToolbar setCloseAllButtonTarget:self
                               action:@selector(closeAllButtonTapped:)];
  [topToolbar setDoneButtonTarget:self action:@selector(doneButtonTapped:)];
  [topToolbar setNewTabButtonTarget:self action:@selector(newTabButtonTapped:)];
  [topToolbar setSelectAllButtonTarget:self
                                action:@selector(selectAllButtonTapped:)];
  [topToolbar setSearchButtonTarget:self action:@selector(searchButtonTapped:)];
  [topToolbar setCancelSearchButtonTarget:self
                                   action:@selector(cancelSearchButtonTapped:)];
  [topToolbar setSearchBarDelegate:self];

  // Configure and initialize the page control.
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedValue:)
                   forControlEvents:UIControlEventValueChanged];
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedPageByDrag:)
                   forControlEvents:TabGridPageChangeByDragEvent];
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedPageByTap:)
                   forControlEvents:TabGridPageChangeByTapEvent];

  [NSLayoutConstraint activateConstraints:@[
    [topToolbar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [topToolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [topToolbar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]
  ]];
}

// Adds the bottom toolbar and sets constraints.
- (void)setupBottomToolbar {
  TabGridBottomToolbar* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  self.bottomToolbar = bottomToolbar;
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomToolbar];
  [NSLayoutConstraint activateConstraints:@[
    [bottomToolbar.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  [bottomToolbar setCloseAllButtonTarget:self
                                  action:@selector(closeAllButtonTapped:)];
  [bottomToolbar setDoneButtonTarget:self action:@selector(doneButtonTapped:)];

  [bottomToolbar setNewTabButtonTarget:self
                                action:@selector(newTabButtonTapped:)];
  [bottomToolbar setCloseTabsButtonTarget:self
                                   action:@selector(closeSelectedTabs:)];
  [bottomToolbar setShareTabsButtonTarget:self
                                   action:@selector(shareSelectedTabs:)];

  [self.layoutGuideCenter referenceView:bottomToolbar
                              underName:kTabGridBottomToolbarGuide];
}

// Adds the foreground view and sets constraints.
- (void)setupForegroundView {
  UIView* foregroundView = [[UIView alloc] init];
  self.foregroundView = foregroundView;
  foregroundView.translatesAutoresizingMaskIntoConstraints = NO;
  foregroundView.userInteractionEnabled = NO;
  foregroundView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  [self.view insertSubview:foregroundView aboveSubview:self.plusSignButton];
  AddSameConstraints(foregroundView, self.view);
}

// Adds the PinnedTabsViewController and sets constraints.
- (void)setupPinnedTabsViewController {
  PinnedTabsViewController* pinnedTabsViewController =
      self.pinnedTabsViewController;
  pinnedTabsViewController.delegate = self;
  pinnedTabsViewController.dragDropHandler = self.pinnedTabsDragDropHandler;

  [self addChildViewController:pinnedTabsViewController];
  [self.view addSubview:pinnedTabsViewController.view];
  [pinnedTabsViewController didMoveToParentViewController:self];

  [self updatePinnedTabsViewControllerConstraints];
}

// Adds the thumb strip's plus sign button, which is visible when the plus sign
// cell isn't.
- (void)setupThumbStripPlusSignButton {
  ThumbStripPlusSignButton* plusSignButton =
      [[ThumbStripPlusSignButton alloc] init];
  self.plusSignButton = plusSignButton;
  plusSignButton.translatesAutoresizingMaskIntoConstraints = NO;
  [plusSignButton addTarget:self
                     action:@selector(plusSignButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];
  DCHECK(self.bottomToolbar);
  [self.view insertSubview:plusSignButton aboveSubview:self.bottomToolbar];
  self.plusSignButtonBottomConstraint =
      [plusSignButton.bottomAnchor constraintEqualToAnchor:self.view.topAnchor
                                                  constant:0];
  NSArray* constraints = @[
    [plusSignButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    self.plusSignButtonBottomConstraint,
    [plusSignButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [plusSignButton.widthAnchor constraintEqualToConstant:kPlusSignButtonWidth],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

- (void)configureViewControllerForCurrentSizeClassesAndPage {
  self.configuration = TabGridConfigurationFloatingButton;
  if ([self shouldUseCompactLayout] ||
      self.tabGridMode == TabGridModeSelection) {
    // The bottom toolbar configuration is applied when the UI is narrow but
    // vertically long or the selection mode is enabled.
    self.configuration = TabGridConfigurationBottomToolbar;
  }
  [self configureButtonsForActiveAndCurrentPage];
}

- (void)configureButtonsForActiveAndCurrentPage {
  self.bottomToolbar.page = self.currentPage;
  self.topToolbar.page = self.currentPage;
  self.bottomToolbar.mode = self.tabGridMode;
  self.topToolbar.mode = self.tabGridMode;

  [self configureAddToButtonMenuForSelectedItems];
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  [self.bottomToolbar setAddToButtonEnabled:!incognitoTabsNeedsAuth];

  // When current page is a remote tabs page.
  if (self.currentPage == TabGridPageRemoteTabs) {
    if (self.pageConfiguration ==
        TabGridPageConfiguration::kIncognitoPageOnly) {
      // Disable done button if showing a disabled tab view for recent tab.
      [self configureDoneButtonOnDisabledPage];
    } else {
      [self configureDoneButtonBasedOnPage:self.activePage];
    }
    // Configure the "Close All" button on the recent tabs page.
    [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
    return;
  }

  // When current page is a disabled tab page.
  if ((self.currentPage == TabGridPageIncognitoTabs &&
       !self.incognitoTabsViewController) ||
      (self.currentPage == TabGridPageRegularTabs &&
       !self.regularTabsViewController)) {
    [self configureDoneButtonOnDisabledPage];
    [self.bottomToolbar setNewTabButtonEnabled:NO];
    [self.topToolbar setCloseAllButtonEnabled:NO];
    [self.bottomToolbar setCloseAllButtonEnabled:NO];
    [self.bottomToolbar setEditButtonEnabled:NO];
    [self.topToolbar setEditButtonEnabled:NO];
    return;
  }

  if (self.tabGridMode == TabGridModeSelection) {
    [self updateSelectionModeToolbars];
  }

  [self configureDoneButtonBasedOnPage:self.currentPage];
  [self configureNewTabButtonBasedOnContentPermissions];
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
}

// Updates the add to menu items with all the currently selected items.
- (void)configureAddToButtonMenuForSelectedItems {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  NSArray<NSString*>* items =
      gridViewController.selectedShareableItemIDsForEditing;
  UIMenu* menu = nil;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      menu =
          [UIMenu menuWithChildren:[self.incognitoTabsDelegate
                                       addToButtonMenuElementsForItems:items]];
      break;
    case TabGridPageRegularTabs:
      menu =
          [UIMenu menuWithChildren:[self.regularTabsDelegate
                                       addToButtonMenuElementsForItems:items]];
      break;
    case TabGridPageRemoteTabs:
      // No-op, Add To button inaccessible in remote tabs page.
      break;
  }
  [self.bottomToolbar setAddToButtonMenu:menu];
}

- (void)configureNewTabButtonBasedOnContentPermissions {
  BOOL isRecentTabPage = self.currentPage == TabGridPageRemoteTabs;
  BOOL allowedByContentAuthentication =
      !((self.currentPage == TabGridPageIncognitoTabs) &&
        self.incognitoTabsViewController.contentNeedsAuthentication);
  BOOL allowNewTab = !isRecentTabPage && allowedByContentAuthentication;
  [self.bottomToolbar setNewTabButtonEnabled:allowNewTab];
  [self.topToolbar setNewTabButtonEnabled:allowNewTab];
}

- (void)configureDoneButtonBasedOnPage:(TabGridPage)page {
  const BOOL tabsPresent = [self tabsPresentForPage:page];

  self.topToolbar.pageControl.userInteractionEnabled = YES;

  // The Done button should have the same behavior as the other buttons on the
  // top Toolbar.
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  BOOL doneEnabled = tabsPresent && !incognitoTabsNeedsAuth;
  [self.topToolbar setDoneButtonEnabled:doneEnabled];
  [self.bottomToolbar setDoneButtonEnabled:doneEnabled];
}

// YES if there are tabs present on `page`. For `TabGridPageRemoteTabs`, YES
// if there are tabs on either of the other pages.
- (BOOL)tabsPresentForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageRemoteTabs:
      return !([self.regularTabsViewController isGridEmpty] &&
               (!IsPinnedTabsEnabled() ||
                [self.pinnedTabsViewController isCollectionEmpty]) &&
               [self.incognitoTabsViewController isGridEmpty]);
    case TabGridPageRegularTabs:
      return !([self.regularTabsViewController isGridEmpty] &&
               (!IsPinnedTabsEnabled() ||
                [self.pinnedTabsViewController isCollectionEmpty]));
    case TabGridPageIncognitoTabs:
      return ![self.incognitoTabsViewController isGridEmpty];
  }
}

// Disables the done button on bottom toolbar if a disabled tab view is
// presented.
- (void)configureDoneButtonOnDisabledPage {
  self.topToolbar.pageControl.userInteractionEnabled = YES;
  [self.bottomToolbar setDoneButtonEnabled:NO];
  [self.topToolbar setDoneButtonEnabled:NO];
}

- (void)configureCloseAllButtonForCurrentPageAndUndoAvailability {
  BOOL useUndo =
      self.undoCloseAllAvailable && self.currentPage == TabGridPageRegularTabs;
  [self.bottomToolbar useUndoCloseAll:useUndo];
  [self.topToolbar useUndoCloseAll:useUndo];
  if (useUndo)
    return;

  // Otherwise setup as a Close All button.
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];

  // "Close all" can be called if there is element in regular tab grid or in
  // inactive tabs.
  BOOL enabled =
      gridViewController && (![gridViewController isGridEmpty] ||
                             ![gridViewController isInactiveGridEmpty]);
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  enabled = enabled && !incognitoTabsNeedsAuth && !self.isDragSeesionInProgress;

  [self.topToolbar setCloseAllButtonEnabled:enabled];
  [self.bottomToolbar setCloseAllButtonEnabled:enabled];
  [self.bottomToolbar setEditButtonEnabled:enabled];
  [self.topToolbar setEditButtonEnabled:enabled];
  [self.topToolbar setNewTabButtonEnabled:enabled];
}

// Shows the two toolbars and the floating button. Suitable for use in
// animations.
- (void)showToolbars {
  [self.topToolbar show];
  if (self.thumbStripEnabled) {
    GridViewController* gridViewController =
        [self gridViewControllerForPage:self.currentPage];
    // gridViewController can be null if page configuration disables the
    // currentPage mode.
    if (gridViewController) {
      self.plusSignButton.alpha =
          1 - gridViewController.fractionVisibleOfLastItem;
    }
  }
  [self.bottomToolbar show];
}

// Hides the two toolbars. Suitable for use in animations.
- (void)hideToolbars {
  [self.topToolbar hide];
  [self.bottomToolbar hide];
}

// Translates the toolbar views offscreen and then animates them back in using
// the transition coordinator. Transitions are preferred here since they don't
// interact with the layout system at all.
- (void)animateToolbarsForAppearance {
  DCHECK(self.transitionCoordinator);
  // Unless reduce motion is enabled, hide the scroll view during the
  // animation.
  if (!UIAccessibilityIsReduceMotionEnabled()) {
    self.scrollView.hidden = YES;
  }
  // Fade the toolbars in for the last 60% of the transition.
  auto keyframe = ^{
    [UIView addKeyframeWithRelativeStartTime:0.2
                            relativeDuration:0.6
                                  animations:^{
                                    [self showToolbars];
                                  }];
  };
  // Animation block that does the keyframe animation.
  auto animation = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [UIView animateKeyframesWithDuration:context.transitionDuration
                                   delay:0
                                 options:UIViewAnimationOptionLayoutSubviews
                              animations:keyframe
                              completion:nil];
  };

  // Restore the scroll view and toolbar opacities (in case the animation didn't
  // complete) as part of the completion.
  auto cleanup = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    self.scrollView.hidden = NO;
    [self showToolbars];
  };

  // Animate the toolbar alphas alongside the current transition.
  [self.transitionCoordinator animateAlongsideTransition:animation
                                              completion:cleanup];
}

// Translates the toolbar views offscreen using the transition coordinator.
- (void)animateToolbarsForDisappearance {
  DCHECK(self.transitionCoordinator);
  // Unless reduce motion is enabled, hide the scroll view during the
  // animation.
  if (!UIAccessibilityIsReduceMotionEnabled()) {
    self.scrollView.hidden = YES;
  }
  // Fade the toolbars out in the first 66% of the transition.
  auto keyframe = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:0.40
                                  animations:^{
                                    [self hideToolbars];
                                  }];
  };

  // Animation block that does the keyframe animation.
  auto animation = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [UIView animateKeyframesWithDuration:context.transitionDuration
                                   delay:0
                                 options:UIViewAnimationOptionLayoutSubviews
                              animations:keyframe
                              completion:nil];
  };

  // Hide the scroll view (and thus the tab grids) until the transition
  // completes. Restore the toolbar opacity when the transition completes.
  auto cleanup = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    self.scrollView.hidden = NO;
  };

  // Animate the toolbar alphas alongside the current transition.
  [self.transitionCoordinator animateAlongsideTransition:animation
                                              completion:cleanup];
}

// Updates the labels and the buttons on the top and the bottom toolbars based
// based on the selected tabs count.
- (void)updateSelectionModeToolbars {
  GridViewController* currentGridViewController =
      [self gridViewControllerForPage:self.currentPage];
  NSUInteger selectedItemsCount =
      [currentGridViewController.selectedItemIDsForEditing count];
  NSUInteger sharableSelectedItemsCount =
      [currentGridViewController.selectedShareableItemIDsForEditing count];
  self.topToolbar.selectedTabsCount = selectedItemsCount;
  self.bottomToolbar.selectedTabsCount = selectedItemsCount;

  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  BOOL enableMultipleItemsSharing =
      !incognitoTabsNeedsAuth && sharableSelectedItemsCount > 0;
  [self.bottomToolbar setShareTabsButtonEnabled:enableMultipleItemsSharing];
  [self.bottomToolbar setAddToButtonEnabled:enableMultipleItemsSharing];
  [self.bottomToolbar
      setCloseTabsButtonEnabled:!incognitoTabsNeedsAuth && selectedItemsCount];
  [self.topToolbar setSelectAllButtonEnabled:!incognitoTabsNeedsAuth];

  if (currentGridViewController.allItemsSelectedForEditing) {
    [self.topToolbar configureDeselectAllButtonTitle];
  } else {
    [self.topToolbar configureSelectAllButtonTitle];
  }

  [self configureAddToButtonMenuForSelectedItems];
}

// Records when the user switches between incognito and regular pages in the tab
// grid. Switching to a different TabGridPage can either be driven by dragging
// the scrollView or tapping on the pageControl.
- (void)recordActionSwitchingToPage:(TabGridPage)page
                     withInteration:
                         (TabSwitcherPageChangeInteraction)interaction {
  switch (page) {
    case TabGridPageIncognitoTabs:
      // There are duplicate metrics below that correspond to the previous
      // separate implementations for iPhone and iPad. Having both allow for
      // comparisons to the previous implementations.
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectIncognitoPanel"));
      break;
    case TabGridPageRegularTabs:
      // There are duplicate metrics below that correspond to the previous
      // separate implementations for iPhone and iPad. Having both allow for
      // comparisons to the previous implementations.
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectRegularPanel"));
      break;
    case TabGridPageRemoteTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridSelectRemotePanel"));
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
      break;
  }
  UMA_HISTOGRAM_ENUMERATION(kUMATabSwitcherPageChangeInteractionHistogram,
                            interaction);
}

// Tells the appropriate delegate to create a new item, and then tells the
// presentation delegate to show the new item.
- (void)openNewTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  switch (page) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsViewController prepareForDismissal];
      [self.incognitoTabsDelegate addNewItem];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsViewController prepareForDismissal];
      [self.regularTabsDelegate addNewItem];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      break;
  }
  self.activePage = page;
  [self.tabPresentationDelegate showActiveTabInPage:page
                                       focusOmnibox:focusOmnibox
                                       closeTabGrid:YES];
}

// Creates and shows a new regular tab.
- (void)openNewRegularTabForKeyboardCommand {
  [self.handler dismissModalDialogs];
  [self openNewTabInPage:TabGridPageRegularTabs focusOmnibox:YES];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCreateRegularTabKeyboard"));
}

// Creates and shows a new incognito tab.
- (void)openNewIncognitoTabForKeyboardCommand {
  [self.handler dismissModalDialogs];
  [self openNewTabInPage:TabGridPageIncognitoTabs focusOmnibox:YES];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCreateIncognitoTabKeyboard"));
}

// Creates and shows a new tab in the current page.
- (void)openNewTabInCurrentPageForKeyboardCommand {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self openNewIncognitoTabForKeyboardCommand];
      break;
    case TabGridPageRegularTabs:
      [self openNewRegularTabForKeyboardCommand];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      break;
  }
}

// Updates the views, buttons, toolbars as well as broadcasts incognito tabs
// visibility after the tab count has changed.
- (void)handleTabCountChangeWithTabCount:(NSUInteger)tabCount {
  if (self.tabGridMode == TabGridModeSelection) {
    // Exit selection mode if there are no more tabs.
    if (tabCount == 0) {
      self.tabGridMode = TabGridModeNormal;
    }

    [self updateSelectionModeToolbars];
  }

  if (tabCount > 0) {
    // Undo is only available when the tab grid is empty.
    self.undoCloseAllAvailable = NO;
  }

  [self configureButtonsForActiveAndCurrentPage];
  [self broadcastIncognitoContentVisibility];
}

// Broadcasts whether incognito tabs are showing.
- (void)broadcastIncognitoContentVisibility {
  // It is programmer error to broadcast incognito content visibility when the
  // view is not visible.
  if (!self.viewVisible)
    return;
  BOOL incognitoContentVisible =
      (self.currentPage == TabGridPageIncognitoTabs &&
       !self.incognitoTabsViewController.gridEmpty);
  [self.handler setIncognitoContentVisible:incognitoContentVisible];
}

- (void)setupSearchUI {
  self.scrimView = [[UIControl alloc] init];
  self.scrimView.backgroundColor =
      [UIColor colorNamed:kDarkerScrimBackgroundColor];
  self.scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrimView.accessibilityIdentifier = kTabGridScrimIdentifier;
  [self.scrimView addTarget:self
                     action:@selector(cancelSearchButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];
  // Add a gesture recognizer to identify when the user interactions with the
  // search results.
  self.searchResultPanRecognizer =
      [[UIPanGestureRecognizer alloc] initWithTarget:self.view
                                              action:@selector(endEditing:)];
  self.searchResultPanRecognizer.cancelsTouchesInView = NO;
  self.searchResultPanRecognizer.delegate = self;
}

// Shows scrim overlay.
- (void)showScrim {
  self.scrimView.alpha = 0.0f;
  if (!self.scrimView.superview) {
    [self.scrollContentView addSubview:self.scrimView];
    AddSameConstraints(self.scrimView, self.view.superview);
    [self.view layoutIfNeeded];
  }
  self.currentPageViewController.accessibilityElementsHidden = YES;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration.InSecondsF()
      animations:^{
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf.scrimView.hidden = NO;
        strongSelf.scrimView.alpha = 1.0f;
      }
      completion:^(BOOL finished) {
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf.isScrimDisplayed = (strongSelf.scrimView.alpha > 0);
        strongSelf.currentPageViewController.accessibilityElementsHidden = YES;
      }];
}

// Hides scrim overlay.
- (void)hideScrim {
  __weak TabGridViewController* weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration.InSecondsF()
      animations:^{
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;

        strongSelf.scrimView.alpha = 0.0f;
        strongSelf.scrimView.hidden = YES;
      }
      completion:^(BOOL finished) {
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf.currentPageViewController.accessibilityElementsHidden = NO;
        strongSelf.isScrimDisplayed = (strongSelf.scrimView.alpha > 0);
      }];
}

// Updates the appearance of the toolbars based on the scroll position of the
// currently active Grid.
- (void)updateToolbarsAppearance {
  UIScrollView* scrollView;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      scrollView = self.incognitoTabsViewController.gridView;
      break;
    case TabGridPageRegularTabs:
      scrollView = self.regularTabsViewController.gridView;
      break;
    case TabGridPageRemoteTabs:
      scrollView = self.remoteTabsViewController.tableView;
      break;
  }

  BOOL gridScrolledToTop =
      scrollView.contentOffset.y <= -scrollView.adjustedContentInset.top;
  [self.topToolbar setScrollViewScrolledToEdge:gridScrolledToTop];

  CGFloat scrollableHeight = scrollView.contentSize.height +
                             scrollView.adjustedContentInset.bottom -
                             scrollView.bounds.size.height;
  BOOL gridScrolledToBottom = scrollView.contentOffset.y >= scrollableHeight;
  [self.bottomToolbar setScrollViewScrolledToEdge:gridScrolledToBottom];
}

- (void)reportTabSelectionTime {
  if (self.tabGridEnterTime.is_null()) {
    // The enter time was not recorded. Bail out.
    return;
  }
  base::TimeDelta duration = base::TimeTicks::Now() - self.tabGridEnterTime;
  base::UmaHistogramLongTimes("IOS.TabSwitcher.TimeSpentOpeningExistingTab",
                              duration);
  self.tabGridEnterTime = base::TimeTicks();
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer == self.searchResultPanRecognizer)
    return YES;
  return NO;
}

#pragma mark - UISearchBarDelegate

- (void)searchBarTextDidBeginEditing:(UISearchBar*)searchBar {
  [self updateScrimVisibilityForText:searchBar.text];
  [self.currentPageViewController.view
      addGestureRecognizer:self.searchResultPanRecognizer];
}

- (void)searchBarTextDidEndEditing:(UISearchBar*)searchBar {
  [self.currentPageViewController.view
      removeGestureRecognizer:self.searchResultPanRecognizer];
}

- (void)searchBarSearchButtonClicked:(UISearchBar*)searchBar {
  [searchBar resignFirstResponder];
}

- (void)searchBar:(UISearchBar*)searchBar textDidChange:(NSString*)searchText {
  searchBar.searchTextField.accessibilityIdentifier =
      [kTabGridSearchTextFieldIdentifierPrefix
          stringByAppendingString:searchText];
  [self updateScrimVisibilityForText:searchText];
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      self.incognitoTabsViewController.searchText = searchText;
      [self updateSearchGrid:self.incognitoTabsDelegate
              withSearchText:searchText];
      break;
    case TabGridPageRegularTabs:
      self.regularTabsViewController.searchText = searchText;
      [self updateSearchGrid:self.regularTabsDelegate
              withSearchText:searchText];
      break;
    case TabGridPageRemoteTabs:
      self.remoteTabsViewController.searchTerms = searchText;
      break;
  }
}

- (void)updateSearchGrid:(id<GridCommands>)tabsDelegate
          withSearchText:(NSString*)searchText {
  if (searchText.length) {
    [tabsDelegate searchItemsWithText:searchText];
  } else {
    // The expectation from searchItemsWithText is to search tabs from all
    // the available windows to the app. However in the case of empy string
    // the grid should revert back to its original state so it doesn't
    // display all the tabs from all the available windows.
    [tabsDelegate resetToAllItems];
  }
}

- (void)updateScrimVisibilityForText:(NSString*)searchText {
  if (_tabGridMode != TabGridModeSearch)
    return;
  if (searchText.length == 0) {
    self.isPerformingSearch = NO;
    [self showScrim];
  } else if (!self.isPerformingSearch) {
    self.isPerformingSearch = YES;
    // If no results have been presented yet, then hide the scrim to present
    // the results.
    [self hideScrim];
  }
}

// Calculates the proper insets for the Incognito Grid ViewController to
// accomodate for the safe area and toolbar.
- (UIEdgeInsets)calculateInsetForIncognitoGridView {
  // The content inset of the tab grids must be modified so that the toolbars
  // do not obscure the tabs. This may change depending on orientation.
  CGFloat bottomInset = self.configuration == TabGridConfigurationBottomToolbar
                            ? self.bottomToolbar.intrinsicContentSize.height
                            : 0;

  BOOL showThumbStrip = self.thumbStripEnabled;
  if (showThumbStrip) {
    bottomInset += self.topToolbar.intrinsicContentSize.height;
  }

  CGFloat topInset =
      showThumbStrip ? 0 : self.topToolbar.intrinsicContentSize.height;
  UIEdgeInsets inset = UIEdgeInsetsMake(topInset, 0, bottomInset, 0);
  inset.left = self.scrollView.safeAreaInsets.left;
  inset.right = self.scrollView.safeAreaInsets.right;
  inset.top += self.scrollView.safeAreaInsets.top;
  inset.bottom += self.scrollView.safeAreaInsets.bottom;

  return inset;
}

// Calculates the proper insets for the Regular Grid ViewController to
// accomodate for the safe area and toolbars.
- (UIEdgeInsets)calculateInsetForRegularGridView {
  UIEdgeInsets inset = [self calculateInsetForIncognitoGridView];

  if (self.regularTabsBottomMessage &&
      !self.regularTabsBottomMessage.view.hidden) {
    inset.bottom += self.regularTabsBottomMessage.view.bounds.size.height;
  }
  if (IsPinnedTabsEnabled() && self.pinnedTabsViewController.visible) {
    CGFloat pinnedViewHeight =
        self.pinnedTabsViewController.view.bounds.size.height;
    inset.bottom += pinnedViewHeight + kPinnedViewBottomPadding;
  }

  return inset;
}

#pragma mark - RecentTabsTableViewControllerUIDelegate

- (void)recentTabsScrollViewDidScroll:
    (RecentTabsTableViewController*)recentTabsTableViewController {
  [self updateToolbarsAppearance];
}

#pragma mark - SuggestedActionsDelegate

- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion {
  if (self.currentPage == TabGridPageIncognitoTabs) {
    // History retrival shouldn't be done from incognito tabs page.
    completion(0);
    return;
  }
  [self.regularTabsDelegate fetchSearchHistoryResultsCountForText:searchText
                                                       completion:completion];
}

- (void)searchHistoryForText:(NSString*)searchText {
  DCHECK(self.tabGridMode == TabGridModeSearch);
  [self.delegate showHistoryFilteredBySearchText:searchText];
}

- (void)searchWebForText:(NSString*)searchText {
  DCHECK(self.tabGridMode == TabGridModeSearch);
  [self.delegate openSearchResultsPageForSearchText:searchText];
}

- (void)searchRecentTabsForText:(NSString*)searchText {
  DCHECK(self.tabGridMode == TabGridModeSearch);
  [self setCurrentPageAndPageControl:TabGridPageRemoteTabs animated:YES];
}

#pragma mark - ThumbStripSupporting

- (BOOL)isThumbStripEnabled {
  return self.foregroundView != nil;
}

- (void)thumbStripEnabledWithPanHandler:
    (ViewRevealingVerticalPanHandler*)panHandler {
  DCHECK(!self.thumbStripEnabled);
  self.scrollView.scrollEnabled = NO;
  [self setupThumbStripPlusSignButton];
  [self setupForegroundView];
  [panHandler addAnimatee:self];
  [self.regularTabsViewController thumbStripEnabledWithPanHandler:panHandler];
  [self.incognitoTabsViewController thumbStripEnabledWithPanHandler:panHandler];
}

- (void)thumbStripDisabled {
  [self.regularTabsViewController thumbStripDisabled];
  [self.incognitoTabsViewController thumbStripDisabled];

  DCHECK(self.thumbStripEnabled);
  self.scrollView.scrollEnabled = YES;
  [self.plusSignButton removeFromSuperview];
  self.plusSignButton = nil;
  [self.foregroundView removeFromSuperview];
  self.foregroundView = nil;

  self.topToolbar.transform = CGAffineTransformIdentity;
  self.topToolbar.alpha = 1;
  [self showToolbars];

  self.regularTabsViewController.gridView.transform = CGAffineTransformIdentity;
  self.incognitoTabsViewController.gridView.transform =
      CGAffineTransformIdentity;
}

#pragma mark - PinnedTabsViewControllerDelegate

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
             didSelectItemWithID:(NSString*)itemID {
  // Record how long it took to select an item.
  [self reportTabSelectionTime];

  [self.pinnedTabsDelegate selectItemWithID:itemID];

  self.activePage = self.currentPage;
  [self setCurrentIdlePageStatus:NO];

  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO
                                       closeTabGrid:YES];
}

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
              didChangeItemCount:(NSUInteger)count {
  self.topToolbar.pageControl.pinnedTabCount = count;
  const NSUInteger totalTabCount =
      count + self.topToolbar.pageControl.regularTabCount;

  crash_keys::SetRegularTabCount(totalTabCount);
  [self handleTabCountChangeWithTabCount:totalTabCount];
}

- (void)pinnedTabsViewControllerVisibilityDidChange:
    (PinnedTabsViewController*)pinnedTabsViewController {
  UIEdgeInsets inset = [self calculateInsetForRegularGridView];
  [UIView animateWithDuration:kPinnedViewInsetAnimationTime
                   animations:^{
                     self.regularTabsViewController.gridView.contentInset =
                         inset;
                     [self updateRegularTabsBottomMessageConstraintsIfExists];
                   }];
}

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
               didMoveItemWithID:(NSString*)itemID {
  [self setCurrentIdlePageStatus:NO];
}

- (void)pinnedTabsViewController:(GridViewController*)gridViewController
             didRemoveItemWIthID:(NSString*)itemID {
  [self setCurrentIdlePageStatus:NO];
}

- (void)pinnedViewControllerDropAnimationWillBegin:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.regularTabsViewController.dropAnimationInProgress = YES;
}

- (void)pinnedViewControllerDropAnimationDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.regularTabsViewController.dropAnimationInProgress = NO;
}

- (void)pinnedViewControllerDragSessionDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.dragSeesionInProgress = NO;

  [self.topToolbar setSearchButtonEnabled:YES];

  [self configureDoneButtonBasedOnPage:self.currentPage];
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
  [self configureNewTabButtonBasedOnContentPermissions];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  if (self.tabGridMode == TabGridModeSelection) {
    [self updateSelectionModeToolbars];
    return;
  }

  id<GridCommands> tabsDelegate;
  if (gridViewController == self.regularTabsViewController) {
    tabsDelegate = self.regularTabsDelegate;
    base::RecordAction(base::UserMetricsAction("MobileTabGridOpenRegularTab"));
    if (self.tabGridMode == TabGridModeSearch) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenRegularTabSearchResult"));
    }
  } else if (gridViewController == self.incognitoTabsViewController) {
    tabsDelegate = self.incognitoTabsDelegate;
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenIncognitoTab"));
    if (self.tabGridMode == TabGridModeSearch) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenIncognitoTabSearchResult"));
    }
  }
  // Record how long it took to select an item.
  [self reportTabSelectionTime];

  // Check if the tab being selected is already selected.
  BOOL alreadySelected = [tabsDelegate isItemWithIDSelected:itemID];
  if (!alreadySelected) {
    [self setCurrentIdlePageStatus:NO];
  }

  [tabsDelegate selectItemWithID:itemID];

  if (self.tabGridMode == TabGridModeSearch) {
    if (![tabsDelegate isItemWithIDSelected:itemID]) {
      // That can happen when the search result that was selected is from
      // another window. In that case don't change the active page for this
      // window and don't close the tab grid.
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridOpenSearchResultInAnotherWindow"));
      return;
    } else {
      // Make sure that the keyboard is dismissed before starting the transition
      // to the selected tab.
      [self.view endEditing:YES];
    }
  }
  self.activePage = self.currentPage;
  // When the tab grid is peeked, selecting an item should not close the grid
  // unless the user has selected an already selected tab.
  BOOL closeTabGrid = !self.thumbStripEnabled || alreadySelected ||
                      self.currentState != ViewRevealState::Peeked;
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO
                                       closeTabGrid:closeTabGrid];
}

- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID {
  [self setCurrentIdlePageStatus:NO];

  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate closeItemWithID:itemID];
    // Record when a regular tab is closed.
    base::RecordAction(base::UserMetricsAction("MobileTabGridCloseRegularTab"));
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate closeItemWithID:itemID];
    // Record when an incognito tab is closed.
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCloseIncognitoTab"));
  }
}

- (void)didTapPlusSignInGridViewController:
    (GridViewController*)gridViewController {
  [self setCurrentIdlePageStatus:NO];

  [self plusSignButtonTapped:self];
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO
                                       closeTabGrid:YES];
}

- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex {
  [self setCurrentIdlePageStatus:NO];

  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  }
}

- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  if (gridViewController == self.regularTabsViewController) {
    self.topToolbar.pageControl.regularTabCount = count;
    const NSUInteger totalTabCount =
        count + self.topToolbar.pageControl.pinnedTabCount;

    crash_keys::SetRegularTabCount(totalTabCount);
    [self handleTabCountChangeWithTabCount:totalTabCount];
  } else if (gridViewController == self.incognitoTabsViewController) {
    crash_keys::SetIncognitoTabCount(count);
    [self handleTabCountChangeWithTabCount:count];
  }
  [self setupEditButton];
}

- (void)gridViewController:(GridViewController*)gridViewController
       didRemoveItemWIthID:(NSString*)itemID {
  [self setCurrentIdlePageStatus:NO];
}

- (void)didChangeLastItemVisibilityInGridViewController:
    (GridViewController*)gridViewController {
  self.plusSignButton.plusSignVerticalOffset =
      gridViewController.gridView.adjustedContentInset.top -
      kGridExpectedTopContentInset;

  CGFloat lastItemVisiblity = gridViewController.fractionVisibleOfLastItem;
  self.plusSignButton.alpha = 1 - lastItemVisiblity;
  CGFloat xDistance = UseRTLLayout() ? -kScrollThresholdForPlusSignButtonHide
                                     : kScrollThresholdForPlusSignButtonHide;
  self.plusSignButton.plusSignImage.transform =
      lastItemVisiblity < 1
          ? CGAffineTransformMakeTranslation(lastItemVisiblity * xDistance, 0)
          : CGAffineTransformIdentity;
}

- (void)gridViewController:(GridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
  [self configureButtonsForActiveAndCurrentPage];
}

- (void)gridViewControllerWillBeginDragging:
    (GridViewController*)gridViewController {
  if (!self.thumbStripEnabled) {
    return;
  }
  [self.incognitoPopupMenuHandler dismissPopupMenuAnimated:YES];
  [self.regularPopupMenuHandler dismissPopupMenuAnimated:YES];
}

- (void)gridViewControllerDragSessionWillBegin:
    (GridViewController*)gridViewController {
  self.dragSeesionInProgress = YES;

  // Actions on both bars should be disabled during dragging.
  [self.topToolbar setDoneButtonEnabled:NO];
  self.topToolbar.pageControl.userInteractionEnabled = NO;
  [self.bottomToolbar setDoneButtonEnabled:NO];
  [self.topToolbar setNewTabButtonEnabled:NO];
  [self.topToolbar setSelectAllButtonEnabled:NO];
  [self.topToolbar setEditButtonEnabled:NO];
  [self.topToolbar setSearchButtonEnabled:NO];
  [self.bottomToolbar setEditButtonEnabled:NO];
  [self.bottomToolbar setAddToButtonEnabled:NO];
  [self.bottomToolbar setShareTabsButtonEnabled:NO];
  [self.bottomToolbar setCloseTabsButtonEnabled:NO];
  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dragSessionEnabled:YES];
  }
}

- (void)gridViewControllerDragSessionDidEnd:
    (GridViewController*)gridViewController {
  self.dragSeesionInProgress = NO;

  [self.topToolbar setSearchButtonEnabled:YES];

  // -configureDoneButtonBasedOnPage will enable the page control.
  [self configureDoneButtonBasedOnPage:self.currentPage];
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
  [self configureNewTabButtonBasedOnContentPermissions];
  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dragSessionEnabled:NO];
  }
  if (self.tabGridMode == TabGridModeSelection) {
    [self updateSelectionModeToolbars];
  }
}

- (void)gridViewControllerScrollViewDidScroll:
    (GridViewController*)gridViewController {
  [self updateToolbarsAppearance];
}

- (void)gridViewControllerDropAnimationWillBegin:
    (GridViewController*)gridViewController {
  if (IsPinnedTabsEnabled()) {
    self.pinnedTabsViewController.dropAnimationInProgress = YES;
  }
}

- (void)gridViewControllerDropAnimationDidEnd:
    (GridViewController*)gridViewController {
  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dropAnimationDidEnd];
  }
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (GridViewController*)gridViewController {
  CHECK(IsInactiveTabsEnabled());
  CHECK_EQ(self.currentPage, TabGridPageRegularTabs);
  base::RecordAction(base::UserMetricsAction("MobileTabGridShowInactiveTabs"));
  [self.delegate showInactiveTabs];
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

#pragma mark - Control actions

- (void)doneButtonTapped:(id)sender {
  // Tapping Done when in selection mode, should only return back to the normal
  // mode.
  if (self.tabGridMode == TabGridModeSelection) {
    self.tabGridMode = TabGridModeNormal;
    // Records action when user exit the selection mode.
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectionDone"));
    return;
  }

  TabGridPage newActivePage = self.currentPage;

  if (self.currentPage == TabGridPageRemoteTabs) {
    [self setCurrentIdlePageStatus:YES];
    newActivePage = self.activePage;
  }
  self.activePage = newActivePage;
  // Holding the done button down when it is enabled could result in done tap
  // being triggered on release after tabs have been closed and the button
  // disabled. Ensure that action is only taken on a valid state.
  if ([self tabsPresentForPage:newActivePage]) {
    [self.tabPresentationDelegate showActiveTabInPage:newActivePage
                                         focusOmnibox:NO
                                         closeTabGrid:YES];
    // Record when users exit the tab grid to return to the current foreground
    // tab.
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
  }
}

- (void)selectTabsButtonTapped:(id)sender {
  self.tabGridMode = TabGridModeSelection;
  base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabs"));
}

- (void)selectAllButtonTapped:(id)sender {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];

  // Deselect all items if they are all already selected.
  if (gridViewController.allItemsSelectedForEditing) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionDeselectAll"));
    [gridViewController deselectAllItemsForEditing];
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionSelectAll"));
    [gridViewController selectAllItemsForEditing];
  }

  [self updateSelectionModeToolbars];
}

- (void)closeAllButtonTapped:(id)sender {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate closeAllItems];
      break;
    case TabGridPageRegularTabs:
      [self handleCloseAllButtonForRegularTabsWithAnchor:sender];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to call close all tabs on remote tabs.";
      break;
  }
}

- (void)handleCloseAllButtonForRegularTabsWithAnchor:(UIBarButtonItem*)anchor {
  DCHECK_EQ(self.undoCloseAllAvailable,
            (self.regularTabsViewController.gridEmpty &&
             self.regularTabsViewController.inactiveGridEmpty));

  if (self.undoCloseAllAvailable) {
    [self undoCloseAllItemsForRegularTabs];
  } else {
    [self saveAndCloseAllItemsForRegularTabs];
  }
}

- (void)undoCloseAllItemsForRegularTabs {
  // This was saved as a stack: first save the inactive tabs, then the active
  // tabs. So undo in the reverse order: first undo the active tabs, then the
  // inactive tabs.
  [self.regularTabsDelegate undoCloseAllItems];
  [self.inactiveTabsDelegate undoCloseAllItems];

  self.undoCloseAllAvailable = NO;
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
}

- (void)saveAndCloseAllItemsForRegularTabs {
  // This was saved as a stack: first save the inactive tabs, then the active
  // tabs. So undo in the reverse order: first undo the active tabs, then the
  // inactive tabs.
  [self.inactiveTabsDelegate saveAndCloseAllItems];
  [self.regularTabsDelegate saveAndCloseAllItems];

  self.undoCloseAllAvailable = YES;
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
}

- (void)searchButtonTapped:(id)sender {
  self.tabGridMode = TabGridModeSearch;
  base::RecordAction(base::UserMetricsAction("MobileTabGridSearchTabs"));
}

- (void)cancelSearchButtonTapped:(id)sender {
  if (!self.isScrimDisplayed) {
    // Only record search cancel event when an actual search happened.
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCancelSearchTabs"));
  }
  self.tabGridMode = TabGridModeNormal;
}

- (void)newTabButtonTapped:(id)sender {
  [self setCurrentIdlePageStatus:NO];
  base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
  [self openNewTabInPage:self.currentPage focusOmnibox:NO];
  // Record metrics for button taps
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridCreateIncognitoTab"));
      break;
    case TabGridPageRegularTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridCreateRegularTab"));
      break;
    case TabGridPageRemoteTabs:
      // No-op.
      break;
  }
}

- (void)plusSignButtonTapped:(id)sender {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
      [self.incognitoTabsDelegate addNewItem];
      if (self.currentState == ViewRevealState::Peeked) {
        base::RecordAction(
            base::UserMetricsAction("MobileThumbstripCreateIncognitoTab"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridCreateIncognitoTab"));
      }
      break;
    case TabGridPageRegularTabs:
      base::RecordAction(base::UserMetricsAction("MobileTabNewTab"));
      [self.regularTabsDelegate addNewItem];
      if (self.currentState == ViewRevealState::Peeked) {
        base::RecordAction(
            base::UserMetricsAction("MobileThumbstripCreateRegularTab"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridCreateRegularTab"));
      }
      break;
    case TabGridPageRemoteTabs:
      // No-op.
      break;
  }
}

- (void)closeSelectedTabs:(id)sender {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  NSArray<NSString*>* items = gridViewController.selectedItemIDsForEditing;

  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate
          showCloseItemsConfirmationActionSheetWithItems:items
                                                  anchor:sender];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsDelegate
          showCloseItemsConfirmationActionSheetWithItems:items
                                                  anchor:sender];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED()
          << "It is invalid to call close selected tabs on remote tabs.";
      break;
  }
}

- (void)shareSelectedTabs:(id)sender {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  NSArray<NSString*>* items =
      gridViewController.selectedShareableItemIDsForEditing;

  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate shareItems:items anchor:sender];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsDelegate shareItems:items anchor:sender];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "Multiple tab selection invalid on remote tabs.";
      break;
  }
}

- (void)pageControlChangedValue:(id)sender {
  // Map the page control slider position (in the range 0.0-1.0) to an
  // x-offset for the scroll view.
  CGFloat offset = self.topToolbar.pageControl.sliderPosition;
  // In RTL, flip the offset.
  if (UseRTLLayout())
    offset = 1.0 - offset;

  // Total space available for the scroll view to scroll (horizontally).
  CGFloat offsetWidth =
      self.scrollView.contentSize.width - self.scrollView.frame.size.width;
  CGPoint contentOffset = self.scrollView.contentOffset;
  // Find the final offset by using `offset` as a fraction of the available
  // scroll width.
  contentOffset.x = offsetWidth * offset;
  self.scrollView.contentOffset = contentOffset;
}

- (void)pageControlChangedPageByDrag:(id)sender {
  TabGridPage newPage = self.topToolbar.pageControl.selectedPage;

  [self scrollToPage:newPage animated:YES];
  // Records when the user uses the pageControl to switch pages.
  if (self.currentPage != newPage) {
    [self recordActionSwitchingToPage:newPage
                       withInteration:TabSwitcherPageChangeInteraction::
                                          kControlDrag];
  }
}

- (void)pageControlChangedPageByTap:(id)sender {
  TabGridPage newPage = self.topToolbar.pageControl.selectedPage;

  [self scrollToPage:newPage animated:YES];
  // Records when the user uses the pageControl to switch pages.
  if (self.currentPage != newPage) {
    [self recordActionSwitchingToPage:newPage
                       withInteration:TabSwitcherPageChangeInteraction::
                                          kControlTap];
  }
}

#pragma mark - DisabledTabViewControllerDelegate

- (void)didTapLinkWithURL:(const GURL&)URL {
  [self.delegate openLinkWithURL:URL];
}

#pragma mark - IncognitoReauthObserver

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateAuthenticationRequirement:(BOOL)isRequired {
  if (isRequired) {
    self.tabGridMode = TabGridModeNormal;
  }
}

#pragma mark - UIResponder Helper

// Returns YES if "close all" can be performed. Conditions are:
// * Tab grid is currently displayed,
// * There are tabs to close in the current page,
// * Not in an undo scenario.
- (BOOL)canCloseAllTab {
  return self.viewVisible && ((self.currentPage == TabGridPageIncognitoTabs &&
                               !self.incognitoTabsViewController.gridEmpty) ||
                              (self.currentPage == TabGridPageRegularTabs &&
                               !self.regularTabsViewController.gridEmpty &&
                               !self.undoCloseAllAvailable));
}

// Returns YES if "undo" the close all action can be performed.
- (BOOL)canUndoCloseAllTab {
  return self.viewVisible && self.currentPage == TabGridPageRegularTabs &&
         self.undoCloseAllAvailable;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  // On iOS 15+, key commands visible in the app's menu are created in
  // MenuBuilder.
  if (@available(iOS 15, *)) {
    // Return the key commands that are not already present in the menu.
    return @[
      UIKeyCommand.cr_openNewRegularTab,
      UIKeyCommand.cr_undo,
      UIKeyCommand.cr_close,
      // TODO(crbug.com/1385469): Move it to the menu builder once we have the
      // strings.
      UIKeyCommand.cr_select2,
      UIKeyCommand.cr_select3,
    ];
  } else {
    // Return all the commands supported by TabGridViewController.
    return @[
      UIKeyCommand.cr_openNewTab,
      UIKeyCommand.cr_openNewIncognitoTab,
      UIKeyCommand.cr_openNewRegularTab,
      UIKeyCommand.cr_close,
    ];
  }
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_openNewTab)) ||
      sel_isEqual(action, @selector(keyCommand_openNewRegularTab)) ||
      sel_isEqual(action, @selector(keyCommand_openNewIncognitoTab))) {
    return self.currentPage != TabGridPageRemoteTabs;
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return self.viewVisible;
  }
  if (sel_isEqual(action, @selector(keyCommand_closeAll))) {
    return [self canCloseAllTab];
  }
  if (sel_isEqual(action, @selector(keyCommand_undo))) {
    return [self canUndoCloseAllTab];
  }
  return [super canPerformAction:action withSender:sender];
}

- (void)validateCommand:(UICommand*)command {
  if (command.action == @selector(keyCommand_find)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_SEARCH_TABS);
  } else {
    // TODO(crbug.com/1385469): Add string for change pane's functions.
    return [super validateCommand:command];
  }
}

- (void)keyCommand_openNewTab {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandOpenNewTab"));
  [self openNewTabInCurrentPageForKeyboardCommand];
}

- (void)keyCommand_openNewRegularTab {
  base::RecordAction(
      base::UserMetricsAction("MobileKeyCommandOpenNewRegularTab"));
  [self openNewRegularTabForKeyboardCommand];
}

- (void)keyCommand_openNewIncognitoTab {
  base::RecordAction(
      base::UserMetricsAction("MobileKeyCommandOpenNewIncognitoTab"));
  [self openNewIncognitoTabForKeyboardCommand];
}

- (void)keyCommand_find {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandSearchTabs"));
  [self searchButtonTapped:nil];
}

- (void)keyCommand_select1 {
  base::RecordAction(
      base::UserMetricsAction("MobileKeyCommandGoToIncognitoTabGrid"));
  [self setCurrentPageAndPageControl:TabGridPageIncognitoTabs animated:YES];
}

- (void)keyCommand_select2 {
  base::RecordAction(
      base::UserMetricsAction("MobileKeyCommandGoToRegularTabGrid"));
  [self setCurrentPageAndPageControl:TabGridPageRegularTabs animated:YES];
}

- (void)keyCommand_select3 {
  base::RecordAction(
      base::UserMetricsAction("MobileKeyCommandGoToRemoteTabGrid"));
  [self setCurrentPageAndPageControl:TabGridPageRemoteTabs animated:YES];
}

- (void)keyCommand_closeAll {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandCloseAll"));
  [self closeAllButtonTapped:nil];
}

- (void)keyCommand_undo {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandUndo"));
  // This function is also responsible for handling undo.
  [self closeAllButtonTapped:nil];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  if (self.tabGridMode == TabGridModeSearch) {
    [self cancelSearchButtonTapped:nil];
  } else {
    [self doneButtonTapped:nil];
  }
}

// Returns `YES` if should use compact layout.
- (BOOL)shouldUseCompactLayout {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassRegular &&
         self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

// Updates and sets constraints for `pinnedTabsViewController`.
- (void)updatePinnedTabsViewControllerConstraints {
  if ([self.pinnedTabsConstraints count] > 0) {
    [NSLayoutConstraint deactivateConstraints:self.pinnedTabsConstraints];
    self.pinnedTabsConstraints = nil;
  }

  UIView* pinnedView = self.pinnedTabsViewController.view;
  NSMutableArray<NSLayoutConstraint*>* pinnedTabsConstraints =
      [[NSMutableArray alloc] init];
  BOOL compactLayout = [self shouldUseCompactLayout];

  if (compactLayout) {
    [pinnedTabsConstraints addObjectsFromArray:@[
      [pinnedView.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor
                         constant:kPinnedViewHorizontalPadding],
      [pinnedView.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor
                         constant:-kPinnedViewHorizontalPadding],
      [pinnedView.bottomAnchor
          constraintEqualToAnchor:self.bottomToolbar.topAnchor
                         constant:-kPinnedViewBottomPadding],
    ]];
  } else {
    [pinnedTabsConstraints addObjectsFromArray:@[
      [pinnedView.centerXAnchor
          constraintEqualToAnchor:self.view.centerXAnchor],
      [pinnedView.widthAnchor
          constraintEqualToAnchor:self.view.widthAnchor
                       multiplier:kPinnedViewMaxWidthInPercent],
      [pinnedView.topAnchor
          constraintEqualToAnchor:self.bottomToolbar.topAnchor],
    ]];
  }

  self.pinnedTabsConstraints = pinnedTabsConstraints;
  [NSLayoutConstraint activateConstraints:self.pinnedTabsConstraints];
}

// Updates the bottom constraint for the bottom message on the regular tabs.
- (void)updateRegularTabsBottomMessageConstraintsIfExists {
  if (!self.regularTabsBottomMessage) {
    return;
  }
  [NSLayoutConstraint
      deactivateConstraints:self.regularTabsBottomMessageConstraints];
  self.regularTabsBottomMessageConstraints = nil;

  UIView* bottomMessageView = self.regularTabsBottomMessage.view;
  [bottomMessageView invalidateIntrinsicContentSize];
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [[NSMutableArray alloc] init];
  // left and right anchors.
  if ([self shouldUseCompactLayout]) {
    [constraints addObjectsFromArray:@[
      [bottomMessageView.widthAnchor
          constraintEqualToAnchor:self.view.widthAnchor],
      [bottomMessageView.centerXAnchor
          constraintEqualToAnchor:self.regularTabsViewController.view
                                      .centerXAnchor]
    ]];
  } else {
    // Make space on the right so that the message would NOT cover the new tab
    // button.
    CGFloat trailingMarginToShowNewTabButton =
        kTabGridFloatingButtonHorizontalInset +
        self.bottomToolbar.largeNewTabButton.intrinsicContentSize.width;
    [constraints addObjectsFromArray:@[
      [bottomMessageView.widthAnchor
          constraintEqualToAnchor:self.view.widthAnchor
                         constant:self.regularTabsViewController.gridView
                                      .contentOffset.x -
                                  trailingMarginToShowNewTabButton],
      [bottomMessageView.leadingAnchor
          constraintEqualToAnchor:self.regularTabsViewController.view
                                      .leadingAnchor]
    ]];
  }
  // Bottom and top anchors.
  CGFloat topLayoutAnchorConstant =
      [self shouldUseCompactLayout]
          ? self.topToolbar.intrinsicContentSize.height +
                self.bottomToolbar.intrinsicContentSize.height
          : self.topToolbar.intrinsicContentSize.height;
  NSLayoutYAxisAnchor* bottomAnchor = [self shouldUseCompactLayout]
                                          ? self.bottomToolbar.topAnchor
                                          : self.view.bottomAnchor;
  if (IsPinnedTabsEnabled() && self.pinnedTabsViewController.visible) {
    bottomAnchor = self.pinnedTabsViewController.view.topAnchor;
  }
  [constraints addObjectsFromArray:@[
    [bottomMessageView.bottomAnchor constraintEqualToAnchor:bottomAnchor],
    [bottomMessageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.topAnchor
                                    constant:topLayoutAnchorConstant],
    [bottomMessageView.heightAnchor
        constraintLessThanOrEqualToConstant:bottomMessageView
                                                .intrinsicContentSize.height],
  ]];
  self.regularTabsBottomMessageConstraints = constraints;
  [NSLayoutConstraint
      activateConstraints:self.regularTabsBottomMessageConstraints];
}

// Sets up the view for `self.regularTabsBottomMessage`. This should be called
// when the bottom message is just set.
- (void)initializeRegularTabsBottomMessageView {
  UIView* bottomMessageView = self.regularTabsBottomMessage.view;
  bottomMessageView.translatesAutoresizingMaskIntoConstraints = NO;
  // The bottom message should cover all grid cells but not cover the blocking
  // view.
  bottomMessageView.hidden = self.tabGridMode != TabGridModeNormal;
  [self slideInRegularTabsBottomMessage];
}

// Slides `self.regularTabsBottomMessage` from the bottom edge into place. This
// should be called only when the bottom message is just set.
- (void)slideInRegularTabsBottomMessage {
  UIView* bottomMessageView = self.regularTabsBottomMessage.view;
  UIScrollView* regularTabsGridView = self.regularTabsViewController.gridView;
  CGFloat scrollableHeight = regularTabsGridView.contentSize.height +
                             regularTabsGridView.adjustedContentInset.bottom -
                             regularTabsGridView.bounds.size.height;
  // Slide if there are more active tabs that the screen could hold, and that
  // the user has scrolled to the bottom.
  BOOL shouldScrollAgainAfterSliding =
      regularTabsGridView.contentSize.height >= self.view.bounds.size.height &&
      regularTabsGridView.contentOffset.y >= scrollableHeight;
  // Initial position of `bottomMessageView should be below the view, so that
  // the animation slides it up from the bottom, instead of sliding it down from
  // the top.
  NSLayoutConstraint* initialConstraint = [bottomMessageView.topAnchor
      constraintEqualToAnchor:self.view.bottomAnchor];
  initialConstraint.active = YES;
  [self.view layoutIfNeeded];
  // Perform initial animation.
  __weak TabGridViewController* weakSelf = self;
  [UIView
      animateWithDuration:kAnimationDuration.InSecondsF()
               animations:^{
                 initialConstraint.active = NO;
                 [weakSelf updateRegularTabsBottomMessageConstraintsIfExists];
                 [weakSelf.view layoutIfNeeded];
                 if (shouldScrollAgainAfterSliding) {
                   CGFloat newScrollableHeight =
                       scrollableHeight + bottomMessageView.bounds.size.height;
                   [regularTabsGridView
                       setContentOffset:CGPointMake(0, newScrollableHeight)
                               animated:NO];
                 }
               }];
}

// Slides an existing `self.regularTabsBottomMessage` out of the view. This
// should be called when the bottom message is just unset.
- (void)slideOutRegularTabsBottomMessage {
  UIEdgeInsets inset = [self calculateInsetForRegularGridView];
  [UIView animateWithDuration:kAnimationDuration.InSecondsF()
                   animations:^{
                     self.regularTabsViewController.gridView.contentInset =
                         inset;
                   }];
}

@end
