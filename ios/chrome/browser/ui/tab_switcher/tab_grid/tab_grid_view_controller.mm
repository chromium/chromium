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
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller_ui_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_collection_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

BASE_FEATURE(kTabGridAccessibilityScrollKillSwitch,
             "TabGridAccessibilityScrollKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Types of configurations of this view controller.
typedef NS_ENUM(NSUInteger, TabGridConfiguration) {
  TabGridConfigurationBottomToolbar = 1,
  TabGridConfigurationFloatingButton,
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

@interface TabGridViewController () <GridViewControllerDelegate,
                                     PinnedTabsViewControllerDelegate,
                                     RecentTabsTableViewControllerUIDelegate,
                                     SuggestedActionsDelegate,
                                     UIGestureRecognizerDelegate,
                                     UIScrollViewAccessibilityDelegate>
// Whether the view is visible. Bookkeeping is based on
// `-contentWillAppearAnimated:` and
// `-contentWillDisappearAnimated methods. Note that the `Did` methods are not
// reliably called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL viewVisible;
// Child view controllers.
@property(nonatomic, strong) PinnedTabsViewController* pinnedTabsViewController;

// The view controller to display when the recent tabs are disabled.
@property(nonatomic, strong)
    DisabledGridViewController* remoteDisabledViewController;

// Other UI components.
@property(nonatomic, weak) UIScrollView* scrollView;
@property(nonatomic, weak) UIView* scrollContentView;
// Scrim view to be presented when the search box in focused with no text.
@property(nonatomic, strong) UIControl* scrimView;
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
// Constraints for the pinned tabs view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* pinnedTabsConstraints;
// The configuration for tab grid pages.
@property(nonatomic, assign) TabGridPageConfiguration pageConfiguration;
// If the scrim view is being presented.
@property(nonatomic, assign) BOOL isScrimDisplayed;
// Wether there is a search being performed in the tab grid or not.
@property(nonatomic, assign) BOOL isPerformingSearch;
// Pan gesture for when the search results view is scrolled during the search
// mode.
@property(nonatomic, strong) UIPanGestureRecognizer* searchResultPanRecognizer;

@property(nonatomic, assign, getter=isDragSessionInProgress)
    BOOL dragSessionInProgress;

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
    _dragSessionInProgress = NO;

    if (_pageConfiguration == TabGridPageConfiguration::kIncognitoPageOnly) {
      _remoteDisabledViewController = [[DisabledGridViewController alloc]
          initWithPage:TabGridPageRemoteTabs];
      _remoteDisabledViewController.delegate = self;
    } else {
      _remoteTabsViewController = [[RecentTabsTableViewController alloc] init];
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

  if (_pageConfiguration == TabGridPageConfiguration::kIncognitoPageOnly) {
    [self setupDisabledRemoteTabsViewController];
  } else {
    [self setupRemoteTabsViewController];
  }

  [self setupSearchUI];
  [self setupTopToolbar];
  [self setupBottomToolbar];

  if (IsPinnedTabsEnabled()) {
    [self setupPinnedTabsViewController];
  }

  // Hide the toolbars and the floating button, so they can fade in the first
  // time there's a transition into this view controller.
  [self hideToolbars];
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
  if (IsPinnedTabsEnabled()) {
    [self updatePinnedTabsViewControllerConstraints];
  }
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
      [self.mutator pageChanged:page
                    interaction:TabSwitcherPageChangeInteraction::kScrollDrag];
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
    if (base::FeatureList::IsEnabled(kTabGridAccessibilityScrollKillSwitch)) {
      [self.mutator
          pageChanged:page
          interaction:TabSwitcherPageChangeInteraction::kAccessibilitySwipe];
      [self.topToolbar.pageControl setSelectedPage:page animated:YES];
    }
  }
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  TabGridPage currentPage = GetPageFromScrollView(scrollView);
  if (currentPage != self.currentPage && self.isDragSessionInProgress) {
    // This happens when the user drags an item from one scroll view into
    // another.
    [self.mutator pageChanged:currentPage
                  interaction:TabSwitcherPageChangeInteraction::kItemDrag];
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

#pragma mark - LegacyGridTransitionAnimationLayoutProviding properties

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

- (LegacyGridTransitionLayout*)transitionLayout:(TabGridPage)activePage {
  LegacyGridTransitionLayout* layout =
      [self transitionLayoutForPage:activePage];
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
  [self.pinnedTabsConsumer dismissModals];
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

- (void)setIncognitoTabsViewController:
    (IncognitoGridViewController*)incognitoTabsViewController {
  _incognitoTabsViewController = incognitoTabsViewController;
  _incognitoTabsViewController.mode = self.tabGridMode;
  _incognitoTabsViewController.delegate = self;
  _incognitoTabsViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageIncognitoTabs;
}

- (void)setIncognitoDisabledGridViewController:
    (DisabledGridViewController*)incognitoDisabledGridViewController {
  _incognitoDisabledGridViewController = incognitoDisabledGridViewController;
  _incognitoDisabledGridViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageIncognitoTabs;
}

- (void)setRegularTabsViewController:
    (RegularGridViewController*)regularTabsViewController {
  _regularTabsViewController = regularTabsViewController;
  _regularTabsViewController.mode = self.tabGridMode;
  _regularTabsViewController.delegate = self;
  _regularTabsViewController.suggestedActionsDelegate = self;
  _regularTabsViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRegularTabs;
}

- (void)setRegularDisabledGridViewController:
    (DisabledGridViewController*)regularDisabledGridViewController {
  _regularDisabledGridViewController = regularDisabledGridViewController;
  _regularDisabledGridViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRegularTabs;
}

- (void)setRemoteTabsViewController:
    (RecentTabsTableViewController*)remoteTabsViewController {
  _remoteTabsViewController = remoteTabsViewController;
  _remoteTabsViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRemoteTabs;
}

- (void)setRemoteDisabledViewController:
    (DisabledGridViewController*)remoteDisabledViewController {
  _remoteDisabledViewController = remoteDisabledViewController;
  _remoteDisabledViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRemoteTabs;
}

- (void)setPriceCardDataSource:(id<PriceCardDataSource>)priceCardDataSource {
  self.regularTabsViewController.priceCardDataSource = priceCardDataSource;
  _priceCardDataSource = priceCardDataSource;
}

- (id<TabCollectionConsumer>)pinnedTabsConsumer {
  return self.pinnedTabsViewController;
}

- (id<RecentTabsConsumer>)remoteTabsConsumer {
  return self.remoteTabsViewController;
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

- (void)setReauthAgent:(IncognitoReauthSceneAgent*)reauthAgent {
  if (_reauthAgent) {
    [_reauthAgent removeObserver:self];
  }

  _reauthAgent = reauthAgent;

  [_reauthAgent addObserver:self];
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

  [self setInsetForGridViews];
  self.regularTabsViewController.mode = self.tabGridMode;
  self.incognitoTabsViewController.mode = self.tabGridMode;

  self.scrollView.scrollEnabled = (self.tabGridMode == TabGridModeNormal);
  if (mode == TabGridModeSelection)
    [self updateSelectionModeToolbars];
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
- (LegacyGridTransitionLayout*)transitionLayoutForPage:(TabGridPage)page {
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
- (LegacyGridTransitionLayout*)transitionLayoutForRegularTabsPage {
  LegacyGridTransitionLayout* regularTabsTransitionLayout =
      [self.regularTabsViewController transitionLayout];

  if (IsPinnedTabsEnabled()) {
    LegacyGridTransitionLayout* pinnedTabsTransitionLayout =
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
- (LegacyGridTransitionLayout*)
    combineTransitionLayout:(LegacyGridTransitionLayout*)primaryLayout
       withTransitionLayout:(LegacyGridTransitionLayout*)secondaryLayout {
  NSArray<LegacyGridTransitionItem*>* primaryInactiveItems =
      primaryLayout.inactiveItems;
  NSArray<LegacyGridTransitionItem*>* secondaryInactiveItems =
      secondaryLayout.inactiveItems;

  NSArray<LegacyGridTransitionItem*>* inactiveItems =
      [self combineInactiveItems:primaryInactiveItems
               withInactiveItems:secondaryInactiveItems];

  LegacyGridTransitionActiveItem* primaryActiveItem = primaryLayout.activeItem;
  LegacyGridTransitionActiveItem* secondaryActiveItem =
      secondaryLayout.activeItem;

  // Prefer primary active item.
  LegacyGridTransitionActiveItem* activeItem =
      primaryActiveItem ? primaryActiveItem : secondaryActiveItem;

  LegacyGridTransitionItem* primarySelectionItem = primaryLayout.selectionItem;
  LegacyGridTransitionItem* secondarySelectionItem =
      secondaryLayout.selectionItem;

  // Prefer primary selection item.
  LegacyGridTransitionItem* selectionItem =
      primarySelectionItem ? primarySelectionItem : secondarySelectionItem;

  return [LegacyGridTransitionLayout layoutWithInactiveItems:inactiveItems
                                                  activeItem:activeItem
                                               selectionItem:selectionItem];
}

// Combines two arrays of inactive items into one. The `primaryInactiveItems`
// (if any) would be placed in the front of the resulting array, whether the
// `secondaryInactiveItems` would be placed in the back.
- (NSArray<LegacyGridTransitionItem*>*)
    combineInactiveItems:
        (NSArray<LegacyGridTransitionItem*>*)primaryInactiveItems
       withInactiveItems:
           (NSArray<LegacyGridTransitionItem*>*)secondaryInactiveItems {
  if (primaryInactiveItems == nil) {
    primaryInactiveItems = @[];
  }

  return [primaryInactiveItems
      arrayByAddingObjectsFromArray:secondaryInactiveItems];
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

// Returns the corresponding BaseGridViewController for `page`. Returns `nil` if
// page does not have a corresponding BaseGridViewController.
- (BaseGridViewController*)gridViewControllerForPage:(TabGridPage)page {
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
                 : self.incognitoDisabledGridViewController;
    case TabGridPageRegularTabs:
      return self.regularTabsViewController
                 ? self.regularTabsViewController
                 : self.regularDisabledGridViewController;
    case TabGridPageRemoteTabs:
      return self.remoteTabsViewController ? self.remoteTabsViewController
                                           : self.remoteDisabledViewController;
  }
}

- (void)setScrollViewAnimatingContentOffset:
    (BOOL)scrollViewAnimatingContentOffset {
  if (_scrollViewAnimatingContentOffset == scrollViewAnimatingContentOffset)
    return;
  _scrollViewAnimatingContentOffset = scrollViewAnimatingContentOffset;
}

// Adds the scroll view and its content and sets constraints.
- (void)setupScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.pagingEnabled = YES;
  scrollView.delegate = self;
  // Ensures that scroll view does not add additional margins based on safe
  // areas.
  scrollView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  UIStackView* gridsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.incognitoGridContainerViewController.view,
    self.regularGridContainerViewController.view,
    self.remoteGridContainerViewController.view
  ]];
  gridsStack.translatesAutoresizingMaskIntoConstraints = NO;
  gridsStack.distribution = UIStackViewDistributionEqualSpacing;

  [scrollView addSubview:gridsStack];
  [self.view addSubview:scrollView];

  self.scrollView = scrollView;
  self.scrollView.scrollEnabled = YES;
  self.scrollView.accessibilityIdentifier = kTabGridScrollViewIdentifier;
  NSArray* constraints = @[
    [self.incognitoGridContainerViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [self.regularGridContainerViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [self.remoteGridContainerViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],

    [scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [gridsStack.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [gridsStack.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
    [gridsStack.leadingAnchor constraintEqualToAnchor:scrollView.leadingAnchor],
    [gridsStack.trailingAnchor
        constraintEqualToAnchor:scrollView.trailingAnchor],
    [gridsStack.heightAnchor constraintEqualToAnchor:scrollView.heightAnchor],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Setup remote grid.
// TODO(crbug.com/1457146): Move this to the grid itself when specific grid file
// will be created.
- (void)setupRemoteTabsViewController {
  self.remoteTabsViewController.UIDelegate = self;
  // TODO(crbug.com/804589) : Dark style on remote tabs.
  // The styler must be set before the view controller is loaded.
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  styler.tableViewBackgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  self.remoteTabsViewController.overrideUserInterfaceStyle =
      UIUserInterfaceStyleDark;
  self.remoteTabsViewController.styler = styler;
  self.remoteGridContainerViewController.containedViewController =
      self.remoteTabsViewController;
  self.remoteTabsViewController.view.accessibilityElementsHidden =
      _currentPage != TabGridPageRemoteTabs;
}

// Adds a DisabledGridViewController as a contained view controller for the
// remote tabs.
- (void)setupDisabledRemoteTabsViewController {
  self.remoteGridContainerViewController.containedViewController =
      self.remoteDisabledViewController;
  self.remoteDisabledViewController.delegate = self;
  self.remoteDisabledViewController.view.accessibilityElementsHidden =
      _currentPage != TabGridPageRemoteTabs;
}

// Adds the top toolbar and sets constraints.
- (void)setupTopToolbar {
  UIView* topToolbar = self.topToolbar;
  CHECK(topToolbar);

  [self.view addSubview:topToolbar];

  [NSLayoutConstraint activateConstraints:@[
    [topToolbar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [topToolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [topToolbar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]
  ]];
}

// Adds the bottom toolbar and sets constraints.
- (void)setupBottomToolbar {
  UIView* bottomToolbar = self.bottomToolbar;
  CHECK(bottomToolbar);

  [self.view addSubview:bottomToolbar];

  [NSLayoutConstraint activateConstraints:@[
    [bottomToolbar.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  [self.layoutGuideCenter referenceView:bottomToolbar
                              underName:kTabGridBottomToolbarGuide];
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

  BOOL incognitoIsDisabled =
      _pageConfiguration == TabGridPageConfiguration::kIncognitoPageDisabled;
  BOOL regularIsDisabled =
      _pageConfiguration == TabGridPageConfiguration::kIncognitoPageOnly;
  // When current page is a disabled tab page.
  if ((self.currentPage == TabGridPageIncognitoTabs && incognitoIsDisabled) ||
      (self.currentPage == TabGridPageRegularTabs && regularIsDisabled)) {
    [self configureDoneButtonOnDisabledPage];
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
  BaseGridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  // This can be called when the current page is not tied to a grid view
  // controller. If so, return early, because getting the std::set off of a nil
  // gridViewController would return a garbage std::set.
  if (!gridViewController) {
    return;
  }
  const std::set<web::WebStateID> items =
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

// TODO(crbug.com/1457146): Remove this when incognito authentication is take
// into account for button configuration.
- (void)configureNewTabButtonBasedOnContentPermissions {
  BOOL isRecentTabPage = self.currentPage == TabGridPageRemoteTabs;
  BOOL allowedByContentAuthentication =
      !((self.currentPage == TabGridPageIncognitoTabs) &&
        self.incognitoTabsViewController.contentNeedsAuthentication);
  BOOL allowNewTab = !isRecentTabPage && allowedByContentAuthentication;
  [self.bottomToolbar setNewTabButtonEnabled:allowNewTab];
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
  BaseGridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];

  // "Close all" can be called if there is element in regular tab grid or in
  // inactive tabs.
  BOOL enabled =
      gridViewController && (![gridViewController isGridEmpty] ||
                             ![gridViewController isInactiveGridEmpty]);
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  enabled = enabled && !incognitoTabsNeedsAuth && !self.isDragSessionInProgress;

  [self.topToolbar setCloseAllButtonEnabled:enabled];
  [self.bottomToolbar setCloseAllButtonEnabled:enabled];
  [self.bottomToolbar setEditButtonEnabled:enabled];
  [self.topToolbar setEditButtonEnabled:enabled];
}

// Shows the two toolbars and the floating button. Suitable for use in
// animations.
- (void)showToolbars {
  [self.topToolbar show];
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
  BaseGridViewController* currentBaseGridViewController =
      [self gridViewControllerForPage:self.currentPage];
  // This can be called when the current page is not tied to a grid view
  // controller. If so, return early, because getting the std::set off of a nil
  // gridViewController would return a garbage std::set.
  if (!currentBaseGridViewController) {
    return;
  }
  NSUInteger selectedItemsCount =
      currentBaseGridViewController.selectedItemIDsForEditing.size();
  NSUInteger sharableSelectedItemsCount =
      currentBaseGridViewController.selectedShareableItemIDsForEditing.size();
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

  if (currentBaseGridViewController.allItemsSelectedForEditing) {
    [self.topToolbar configureDeselectAllButtonTitle];
  } else {
    [self.topToolbar configureSelectAllButtonTitle];
  }

  [self configureAddToButtonMenuForSelectedItems];
}

// Tells the appropriate delegate to create a new item, and then tells the
// presentation delegate to show the new item.
- (void)openNewTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  // Guard against opening new tabs in a page that is disabled. It is the job
  // of the caller to make sure to not open a new tab in a page that can't
  // perform the action. For example, it is an error to attempt to open a new
  // tab in the icognito page when incognito is disabled by policy.
  CHECK([self canPerformOpenNewTabActionForDestinationPage:page]);

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
      NOTREACHED() << "It is invalid to open a new tab in remote tabs.";
      break;
  }
  self.activePage = page;
  [self.tabPresentationDelegate showActiveTabInPage:page
                                       focusOmnibox:focusOmnibox];
}

// Creates and shows a new regular tab.
- (void)openNewRegularTabForKeyboardCommand {
  [self.handler dismissModalDialogsWithCompletion:nil];
  [self openNewTabInPage:TabGridPageRegularTabs focusOmnibox:YES];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCreateRegularTabKeyboard"));
}

// Creates and shows a new incognito tab.
- (void)openNewIncognitoTabForKeyboardCommand {
  [self.handler dismissModalDialogsWithCompletion:nil];
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
    [self.scrollView addSubview:self.scrimView];
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

// Returns YES if the switcher page is enabled. For example, the page may be
// disabled by policy, in which case NO is returned.
- (BOOL)isPageEnabled:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return _pageConfiguration !=
             TabGridPageConfiguration::kIncognitoPageDisabled;
    case TabGridPageRegularTabs:
      return _pageConfiguration != TabGridPageConfiguration::kIncognitoPageOnly;
    case TabGridPageRemoteTabs:
      return _pageConfiguration != TabGridPageConfiguration::kIncognitoPageOnly;
  }
}

// Returns YES if a new tab action that tagets the `destinationPage` can be
// performed. The _currentPage can be the same page as the `destinationPage`.
- (BOOL)canPerformOpenNewTabActionForDestinationPage:
    (TabGridPage)destinationPage {
  return [self isPageEnabled:destinationPage] &&
         self.currentPage != TabGridPageRemoteTabs;
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

  CGFloat topInset = self.topToolbar.intrinsicContentSize.height;
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

#pragma mark - PinnedTabsViewControllerDelegate

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
             didSelectItemWithID:(web::WebStateID)itemID {
  // Record how long it took to select an item.
  [self reportTabSelectionTime];

  [self.pinnedTabsDelegate selectItemWithID:itemID];

  self.activePage = self.currentPage;
  [self setCurrentIdlePageStatus:NO];

  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO];
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
                   }];
}

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
               didMoveItemWithID:(web::WebStateID)itemID {
  [self setCurrentIdlePageStatus:NO];
}

- (void)pinnedTabsViewController:(BaseGridViewController*)gridViewController
             didRemoveItemWIthID:(web::WebStateID)itemID {
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
  self.dragSessionInProgress = NO;

  [self.topToolbar setSearchButtonEnabled:YES];

  [self configureDoneButtonBasedOnPage:self.currentPage];
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
  [self configureNewTabButtonBasedOnContentPermissions];
}

#pragma mark - BaseGridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  // Check that the current page matches the grid view being interacted with.
  BOOL isOnRegularTabsPage = self.currentPage == TabGridPageRegularTabs;
  BOOL isOnIncognitoTabsPage = self.currentPage == TabGridPageIncognitoTabs;
  BOOL isOnRemoteTabsPage = self.currentPage == TabGridPageRemoteTabs;
  BOOL gridIsRegularTabs = gridViewController == self.regularTabsViewController;
  BOOL gridIsIncognitoTabs =
      gridViewController == self.incognitoTabsViewController;
  if ((isOnRegularTabsPage && !gridIsRegularTabs) ||
      (isOnIncognitoTabsPage && !gridIsIncognitoTabs) || isOnRemoteTabsPage) {
    return;
  }

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

  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
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

- (void)gridViewController:(BaseGridViewController*)gridViewController
         didMoveItemWithID:(web::WebStateID)itemID
                   toIndex:(NSUInteger)destinationIndex {
  [self setCurrentIdlePageStatus:NO];

  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  }
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
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
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWIthID:(web::WebStateID)itemID {
  [self setCurrentIdlePageStatus:NO];
}

- (void)didChangeLastItemVisibilityInGridViewController:
    (BaseGridViewController*)gridViewController {
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
  [self configureButtonsForActiveAndCurrentPage];
}

- (void)gridViewControllerWillBeginDragging:
    (BaseGridViewController*)gridViewController {
}

- (void)gridViewControllerDragSessionWillBegin:
    (BaseGridViewController*)gridViewController {
  self.dragSessionInProgress = YES;

  // Actions on both bars should be disabled during dragging.
  [self.topToolbar setDoneButtonEnabled:NO];
  self.topToolbar.pageControl.userInteractionEnabled = NO;
  [self.bottomToolbar setDoneButtonEnabled:NO];
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
    (BaseGridViewController*)gridViewController {
  self.dragSessionInProgress = NO;

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
    (BaseGridViewController*)gridViewController {
  [self updateToolbarsAppearance];
}

- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController {
  if (IsPinnedTabsEnabled()) {
    self.pinnedTabsViewController.dropAnimationInProgress = YES;
  }
}

- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController {
  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dropAnimationDidEnd];
  }
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController {
  CHECK(IsInactiveTabsEnabled());
  CHECK_EQ(self.currentPage, TabGridPageRegularTabs);
  base::RecordAction(base::UserMetricsAction("MobileTabGridShowInactiveTabs"));
  [self.delegate showInactiveTabs];
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED();
}

#pragma mark - TabGridToolbarsActionWrangler

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
                                         focusOmnibox:NO];
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
  BaseGridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  CHECK(gridViewController);

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

  // Records when the user uses the pageControl to switch pages.
  if (self.currentPage != newPage) {
    [self.mutator pageChanged:newPage
                  interaction:TabSwitcherPageChangeInteraction::kControlDrag];
  }
  [self scrollToPage:newPage animated:YES];
}

- (void)pageControlChangedPageByTap:(id)sender {
  TabGridPage newPage = self.topToolbar.pageControl.selectedPage;

  // Records when the user uses the pageControl to switch pages.
  if (self.currentPage != newPage) {
    [self.mutator pageChanged:newPage
                  interaction:TabSwitcherPageChangeInteraction::kControlTap];
  }
  [self scrollToPage:newPage animated:YES];
}

#pragma mark - DisabledGridViewControllerDelegate

- (void)didTapLinkWithURL:(const GURL&)URL {
  [self.delegate openLinkWithURL:URL];
}

- (bool)isViewControllerSubjectToParentalControls {
  return _isSubjectToParentalControls;
}

#pragma mark - TabGridConsumer

- (void)updateParentalControlStatus:(BOOL)isSubjectToParentalControls {
  _isSubjectToParentalControls = isSubjectToParentalControls;
}

- (void)updateTabGridForIncognitoModeDisabled:(BOOL)isIncognitoModeDisabled {
  BOOL isTabGridUpdated = NO;

  if (isIncognitoModeDisabled &&
      _pageConfiguration == TabGridPageConfiguration::kAllPagesEnabled) {
    _pageConfiguration = TabGridPageConfiguration::kIncognitoPageDisabled;
    isTabGridUpdated = YES;
  } else if (!isIncognitoModeDisabled &&
             _pageConfiguration ==
                 TabGridPageConfiguration::kIncognitoPageDisabled) {
    _pageConfiguration = TabGridPageConfiguration::kAllPagesEnabled;
    isTabGridUpdated = YES;
  }

  if (isTabGridUpdated) {
    [self configureButtonsForActiveAndCurrentPage];
    [self broadcastIncognitoContentVisibility];
  }
}

#pragma mark - IncognitoReauthObserver

- (void)reauthAgent:(IncognitoReauthSceneAgent*)agent
    didUpdateAuthenticationRequirement:(BOOL)isRequired {
  if (isRequired) {
    self.tabGridMode = TabGridModeNormal;
  }
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (UIResponder*)nextResponder {
  UIResponder* nextResponder = [super nextResponder];
  if (self.viewVisible) {
    // Add toolbars to the responder chain.
    // TODO(crbug.com/1457146): Transform toolbars in view controller directly
    // have it in the chain by default instead of adding it manually.
    [self.bottomToolbar respondBeforeResponder:nextResponder];
    [self.topToolbar respondBeforeResponder:self.bottomToolbar];
    return self.topToolbar;
  } else {
    return nextResponder;
  }
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  // On iOS 15+, key commands visible in the app's menu are created in
  // MenuBuilder. Return the key commands that are not already present in the
  // menu.
  return @[
    UIKeyCommand.cr_openNewRegularTab,
    UIKeyCommand.cr_close,
    // TODO(crbug.com/1385469): Move it to the menu builder once we have the
    // strings.
    UIKeyCommand.cr_select2,
    UIKeyCommand.cr_select3,
  ];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (sel_isEqual(action, @selector(keyCommand_openNewTab))) {
    return [self canPerformOpenNewTabActionForDestinationPage:self.currentPage];
  }
  if (sel_isEqual(action, @selector(keyCommand_openNewRegularTab))) {
    return [self
        canPerformOpenNewTabActionForDestinationPage:TabGridPageRegularTabs];
  }
  if (sel_isEqual(action, @selector(keyCommand_openNewIncognitoTab))) {
    return [self
        canPerformOpenNewTabActionForDestinationPage:TabGridPageIncognitoTabs];
  }
  if (sel_isEqual(action, @selector(keyCommand_find))) {
    return self.viewVisible;
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

#pragma mark - GridConsumer

- (void)setPageIdleStatus:(BOOL)status {
  [self setCurrentIdlePageStatus:status];
}

- (void)setActivePageFromPage:(TabGridPage)page {
  self.activePage = page;
}

- (void)prepareForDismissal {
  [self.incognitoTabsViewController prepareForDismissal];
  [self.regularTabsViewController prepareForDismissal];
}

@end
