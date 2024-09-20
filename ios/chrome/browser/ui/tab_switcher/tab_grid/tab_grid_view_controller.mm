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
#import "base/notimplemented.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view.h"
#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_view_delegate.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
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
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_activity_observer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_layout.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

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
  TabGridPage tabGridPage = static_cast<TabGridPage>(page);
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract `page` from the
    // TabGridPageRemoteTabs value.
    tabGridPage = static_cast<TabGridPage>(TabGridPageRemoteTabs - page);
  }
  // With Tab Group Sync, the last page is actually Tab Groups, not Remote Tabs.
  // So do the swap before returning the page.
  if (IsTabGroupSyncEnabled() && tabGridPage == TabGridPageRemoteTabs) {
    tabGridPage = TabGridPageTabGroups;
  }
  return tabGridPage;
}

NSUInteger GetPageIndexFromPage(TabGridPage page) {
  // With Tab Group Sync, the last page is actually Tab Groups, not Remote Tabs.
  // But this method computes the page index out of the enum value, so simulate
  // being Remote Tabs…
  if (IsTabGroupSyncEnabled() && page == TabGridPageTabGroups) {
    page = TabGridPageRemoteTabs;
  }
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract `page` from the highest-
    // index TabGridPage value.
    return static_cast<NSUInteger>(TabGridPageRemoteTabs - page);
  }
  return static_cast<NSUInteger>(page);
}
}  // namespace

@interface TabGridViewController () <GestureInProductHelpViewDelegate,
                                     GridViewControllerDelegate,
                                     PinnedTabsViewControllerDelegate,
                                     RecentTabsTableViewControllerUIDelegate,
                                     TabGroupsPanelViewControllerUIDelegate,
                                     UIGestureRecognizerDelegate,
                                     UIScrollViewAccessibilityDelegate>
// Whether the view is visible. Bookkeeping is based on
// `-contentWillAppearAnimated:` and
// `-contentWillDisappearAnimated methods. Note that the `Did` methods are not
// reliably called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL viewVisible;

// The view controller to display when the recent tabs are disabled.
@property(nonatomic, strong)
    DisabledGridViewController* remoteDisabledViewController;

// Redefined as readwrite
@property(nonatomic, assign, readwrite) TabGridPage activePage;
// Setting the current page doesn't scroll the scroll view; use
// -scrollToPage:animated: for that. Redefined as readwrite.
@property(nonatomic, assign, readwrite) TabGridPage currentPage;

// Other UI components.
@property(nonatomic, weak) UIScrollView* scrollView;
@property(nonatomic, weak) UIView* scrollContentView;
// Scrim view to be presented when the search box in focused with no text.
@property(nonatomic, strong) UIControl* scrimView;
@property(nonatomic, assign) TabGridConfiguration configuration;
// The UIViewController corresponding with `currentPage`.
@property(nonatomic, readonly) UIViewController* currentPageViewController;
// Whether the scroll view is animating its content offset to the current page.
@property(nonatomic, assign, getter=isScrollViewAnimatingContentOffset)
    BOOL scrollViewAnimatingContentOffset;
// Constraints for the pinned tabs view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* pinnedTabsConstraints;
// The configuration for tab grid pages.
@property(nonatomic, assign) TabGridPageConfiguration pageConfiguration;
// Whether there is a search being performed in the tab grid or not.
@property(nonatomic, assign) BOOL isPerformingSearch;
// Pan gesture for when the search results view is scrolled during the search
// mode.
@property(nonatomic, strong) UIPanGestureRecognizer* searchResultPanRecognizer;

@property(nonatomic, assign, getter=isDragSessionInProgress)
    BOOL dragSessionInProgress;

// The timestamp of the user entering the tab grid.
@property(nonatomic, assign) base::TimeTicks tabGridEnterTime;

// The in-product help view to instruct the user to swipe to incognito, and its
// bottom constraint.
@property(nonatomic, strong) GestureInProductHelpView* swipeToIncognitoIPH;
@property(nonatomic, strong)
    NSLayoutConstraint* swipeToIncognitoIPHBottomConstraint;

@end

@implementation TabGridViewController {
  // Searched text.
  NSString* _searchText;
  // Idle page status.
  // Tracks whether the user closed the tab switcher without doing any
  // `TabGridActionType::kInPageAction`s.
  BOOL _idleTabGrid;
  // Whether the user has done anything meaningful when the third page is
  // visible.
  BOOL _idleThirdPage;
  // Whether the user has changed pages since entering the tab grid.
  BOOL _pageChangedSinceEntering;
  // Whether the user has put the app to background since entering tab grid.
  BOOL _backgroundedSinceEntering;
  // Current mode of the TabGrid.
  TabGridMode _mode;
}

- (instancetype)initWithPageConfiguration:
    (TabGridPageConfiguration)tabGridPageConfiguration {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _pageConfiguration = tabGridPageConfiguration;
    _dragSessionInProgress = NO;

    if (!IsTabGroupSyncEnabled()) {
      // TODO(crbug.com/41390276): This should move to a proper Recent Tabs in
      // Grid coordinator.
      if (_pageConfiguration == TabGridPageConfiguration::kIncognitoPageOnly) {
        _remoteDisabledViewController = [[DisabledGridViewController alloc]
            initWithPage:TabGridPageRemoteTabs];
        _remoteDisabledViewController.delegate = self;
      } else {
        _remoteTabsViewController =
            [[RecentTabsTableViewController alloc] init];
      }
    }
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  [self setupScrollView];

  if (!IsTabGroupSyncEnabled()) {
    if (_pageConfiguration == TabGridPageConfiguration::kIncognitoPageOnly) {
      [self setupDisabledRemoteTabsViewController];
    } else {
      [self setupRemoteTabsViewController];
    }
  }

  [self setupSearchUI];
  [self setupTopToolbar];
  [self setupBottomToolbar];

  if (IsPinnedTabsEnabled()) {
    CHECK(self.pinnedTabsViewController);
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
  if ([self.swipeToIncognitoIPH superview] == self.view) {
    self.swipeToIncognitoIPHBottomConstraint.active = NO;
    self.swipeToIncognitoIPHBottomConstraint =
        [self.swipeToIncognitoIPH.bottomAnchor
            constraintEqualToAnchor:[self shouldUseCompactLayout]
                                        ? self.bottomToolbar.topAnchor
                                        : self.regularTabsViewController.view
                                              .bottomAnchor];

    self.swipeToIncognitoIPHBottomConstraint.active = YES;
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
    if (UseRTLLayout()) {
      offset = 1.0 - offset;
    }
    self.topToolbar.pageControl.sliderPosition = offset;

    TabGridPage page = GetPageFromScrollView(scrollView);
    if (page != self.currentPage) {
      // Records when the user drags the scrollView to switch pages.
      [self.mutator pageChanged:page
                    interaction:TabSwitcherPageChangeInteraction::kScrollDrag];
      self.currentPage = page;
      [self broadcastIncognitoContentVisibility];
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
    [self.mutator
        pageChanged:page
        interaction:TabSwitcherPageChangeInteraction::kAccessibilitySwipe];
    self.currentPage = page;
    [self broadcastIncognitoContentVisibility];
    [self.topToolbar.pageControl setSelectedPage:page animated:YES];
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
  if (!self.isDragSessionInProgress) {
    [self maybeShowSwipeToIncognitoIPH];
  }
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self.tabGridHandler exitTabGrid];
  return YES;
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
      if (IsTabGroupInGridEnabled()) {
        stringID = IDS_IOS_TAB_GRID_REGULAR_TABS_WITH_GROUPS_TITLE;
      } else {
        stringID = IDS_IOS_TAB_GRID_REGULAR_TABS_TITLE;
      }
      break;
    case TabGridPageRemoteTabs:
      stringID = IDS_IOS_TAB_GRID_REMOTE_TABS_TITLE;
      break;
    case TabGridPageTabGroups:
      stringID = IDS_IOS_TAB_GRID_TAB_GROUPS_TITLE;
      break;
  }
  return l10n_util::GetNSString(stringID);
}

#pragma mark - TabGridTransitionLayoutProviding

- (TabGridTransitionLayout*)transitionLayout {
  TabGridTransitionItem* activeCell =
      [self transitionItemForActiveCellWithActivePage:self.activePage];
  return [TabGridTransitionLayout layoutWithActiveCell:activeCell];
}

#pragma mark - Public Methods

- (void)contentWillAppearAnimated:(BOOL)animated {
  _pageChangedSinceEntering = NO;
  _backgroundedSinceEntering = NO;
  [self resetIdlePageStatus];
  self.viewVisible = YES;
  [self.topToolbar.pageControl setSelectedPage:self.currentPage animated:NO];
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
  // Modify Remote Tabs Insets when page appears and during rotation.
  if (self.remoteTabsViewController) {
    [self setInsetForRemoteTabs];
  }
  [self maybeShowSwipeToIncognitoIPH];
}

- (void)contentWillDisappearAnimated:(BOOL)animated {
  [self recordIdlePageStatus];

  [self.regularGridHandler discardSavedClosedItems];

  [self.swipeToIncognitoIPH
      dismissWithReason:IPHDismissalReasonType::kTappedOutsideIPHAndAnchorView];

  // When the view disappears, the toolbar alpha should be set to 0; either as
  // part of the animation, or directly with -hideToolbars.
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForDisappearance];
  } else {
    [self hideToolbars];
  }

  self.viewVisible = NO;

  [self.pinnedTabsViewController contentWillDisappear];
  self.remoteTabsViewController.preventUpdates = YES;

  self.tabGridEnterTime = base::TimeTicks();
}

- (void)dismissModals {
  [self.remoteTabsViewController dismissModals];
}

- (void)setCurrentPageAndPageControl:(TabGridPage)page animated:(BOOL)animated {
  [self updatePageWithCurrentSearchTerms:page];

  if (self.topToolbar.pageControl.selectedPage != page)
    [self.topToolbar.pageControl setSelectedPage:page animated:animated];
  if (self.currentPage != page) {
    [self.mutator pageChanged:page
                  interaction:TabSwitcherPageChangeInteraction::kNone];
    self.currentPage = page;
    [self scrollToPage:page animated:animated];
  }
}

// Sets the current search terms on `page`. This allows the content to update
// while the page is still hidden before the page change animation begins.
- (void)updatePageWithCurrentSearchTerms:(TabGridPage)page {
  if (_mode != TabGridMode::kSearch ||
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
    [self.regularGridHandler searchItemsWithText:searchTerms];
  } else {
    self.remoteTabsViewController.searchTerms = searchTerms;
  }
}

- (void)updateActivePageToCurrent {
  TabGridPage newActivePage = self.currentPage;

  if (self.currentPage == TabGridPageRemoteTabs ||
      self.currentPage == TabGridPageTabGroups) {
    _idleThirdPage = YES;
    newActivePage = self.activePage;
  }

  [self.mutator pageChanged:newActivePage
                interaction:TabSwitcherPageChangeInteraction::kNone];
  self.activePage = newActivePage;
}

#pragma mark - Public Properties

- (void)setIncognitoTabsViewController:
    (IncognitoGridViewController*)incognitoTabsViewController {
  _incognitoTabsViewController = incognitoTabsViewController;
  _incognitoTabsViewController.delegate = self;
  _incognitoTabsViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageIncognitoTabs;
}

- (void)setIncognitoDisabledGridViewController:
    (UIViewController*)incognitoDisabledGridViewController {
  _incognitoDisabledGridViewController = incognitoDisabledGridViewController;
  _incognitoDisabledGridViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageIncognitoTabs;
}

- (void)setRegularTabsViewController:
    (RegularGridViewController*)regularTabsViewController {
  _regularTabsViewController = regularTabsViewController;
  _regularTabsViewController.delegate = self;
  _regularTabsViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRegularTabs;
}

- (void)setRegularDisabledGridViewController:
    (UIViewController*)regularDisabledGridViewController {
  _regularDisabledGridViewController = regularDisabledGridViewController;
  _regularDisabledGridViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageRegularTabs;
}

- (void)setTabGroupsPanelViewController:
    (TabGroupsPanelViewController*)tabGroupsPanelViewController {
  _tabGroupsPanelViewController = tabGroupsPanelViewController;
  _tabGroupsPanelViewController.UIDelegate = self;
  _tabGroupsPanelViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageTabGroups;
}

- (void)setTabGroupsDisabledGridViewController:
    (UIViewController*)tabGroupsDisabledGridViewController {
  _tabGroupsDisabledGridViewController = tabGroupsDisabledGridViewController;
  _tabGroupsDisabledGridViewController.view.accessibilityElementsHidden =
      self.currentPage != TabGridPageTabGroups;
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

- (id<RecentTabsConsumer>)remoteTabsConsumer {
  return self.remoteTabsViewController;
}

#pragma mark - Private

// Records the idle page status for the current `currentPage`.
- (void)recordIdlePageStatus {
  if (!self.viewVisible) {
    return;
  }

  switch (self.currentPage) {
    case TabGridPage::TabGridPageIncognitoTabs:
      base::UmaHistogramBoolean(
          kUMATabSwitcherIdleIncognitoTabGridPageHistogram, _idleTabGrid);
      break;
    case TabGridPage::TabGridPageRegularTabs:
      base::UmaHistogramBoolean(kUMATabSwitcherIdleRegularTabGridPageHistogram,
                                _idleTabGrid);
      break;
    case TabGridPage::TabGridPageRemoteTabs:
      base::UmaHistogramBoolean(kUMATabSwitcherIdleRecentTabsHistogram,
                                _idleThirdPage);
      break;
    case TabGridPage::TabGridPageTabGroups:
      base::UmaHistogramBoolean(kUMATabSwitcherIdleTabGroupsHistogram,
                                _idleThirdPage);
      break;
  }
}

// Resets idle page status.
- (void)resetIdlePageStatus {
  _idleTabGrid = YES;
  // `_idleThirdPage` is set to 'YES' if the "Done" button has been tapped from
  // the third page or if the page has changed.
  _idleThirdPage = NO;
}

// Sets the proper insets for the Remote Tabs ViewController to accommodate for
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

// Sets the proper insets for the Grid ViewControllers to accommodate for the
// safe area and toolbars.
- (void)setInsetForGridViews {
  // Sync the scroll view offset to the current page value if the scroll view
  // isn't scrolling. Don't animate this.
  if (!self.scrollViewAnimatingContentOffset && !self.scrollView.dragging &&
      !self.scrollView.decelerating) {
    [self scrollToPage:self.currentPage animated:NO];
  }

  self.incognitoTabsViewController.contentInsets =
      [self calculateInsetsForGridView];
  self.regularTabsViewController.contentInsets =
      [self calculateInsetsForRegularGridView];
  self.tabGroupsPanelViewController.contentInsets =
      [self calculateInsetsForGridView];
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
    case TabGridPageTabGroups:
      return nil;
  }
}

- (void)setActivePage:(TabGridPage)activePage {
  [self scrollToPage:activePage animated:YES];
  [self.activityObserver updateLastActiveTabPage:activePage];
  if (activePage != _activePage) {
    // Usually, an active page change is a result of an in-page action happening
    // on a previously non-active page.
    [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
  }
  _activePage = activePage;
}

- (void)setCurrentPage:(TabGridPage)currentPage {
  // Record the idle metric if the previous page was the third panel.
  if (_currentPage != currentPage) {
    [self tabGridDidPerformAction:TabGridActionType::kChangePage];
    if (_currentPage == TabGridPageRemoteTabs ||
        _currentPage == TabGridPageTabGroups) {
      _idleThirdPage = YES;
      [self recordIdlePageStatus];
      _idleThirdPage = NO;
    }
  }

  // Original current page is about to not be visible. Disable it from being
  // focused by VoiceOver.
  self.currentPageViewController.view.accessibilityElementsHidden = YES;
  UIViewController* previousPageVC = self.currentPageViewController;
  _currentPage = currentPage;
  self.currentPageViewController.view.accessibilityElementsHidden = NO;

  if (_mode == TabGridMode::kSearch) {
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
  // Dismiss IPH if not on regular page.
  if (currentPage != TabGridPage::TabGridPageRegularTabs) {
    [self.swipeToIncognitoIPH
        dismissWithReason:IPHDismissalReasonType::
                              kTappedOutsideIPHAndAnchorView];
  }
  if (IsPinnedTabsEnabled()) {
    const BOOL pinnedTabsAvailable =
        currentPage == TabGridPage::TabGridPageRegularTabs &&
        _mode == TabGridMode::kNormal;
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
      }
    }
  }

  // TODO(crbug.com/41406890) : This is a workaround because TabRestoreService
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
    case TabGridPage::TabGridPageTabGroups:
      return self.tabGroupsPanelViewController
                 ? self.tabGroupsPanelViewController
                 : self.tabGroupsDisabledGridViewController;
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

  [self addChildViewController:self.incognitoGridContainerViewController];
  [self addChildViewController:self.regularGridContainerViewController];
  UIViewController* thirdPanelGridContainerViewController =
      IsTabGroupSyncEnabled() ? self.tabGroupsGridContainerViewController
                              : self.remoteGridContainerViewController;
  [self addChildViewController:thirdPanelGridContainerViewController];
  UIStackView* gridsStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.incognitoGridContainerViewController.view,
    self.regularGridContainerViewController.view,
    thirdPanelGridContainerViewController.view
  ]];
  gridsStack.translatesAutoresizingMaskIntoConstraints = NO;
  gridsStack.distribution = UIStackViewDistributionEqualSpacing;

  [scrollView addSubview:gridsStack];
  [self.view addSubview:scrollView];
  [self.incognitoGridContainerViewController
      didMoveToParentViewController:self];
  [self.regularGridContainerViewController didMoveToParentViewController:self];
  [thirdPanelGridContainerViewController didMoveToParentViewController:self];

  self.scrollView = scrollView;
  self.scrollView.scrollEnabled = YES;
  self.scrollView.accessibilityIdentifier = kTabGridScrollViewIdentifier;
  NSArray* constraints = @[
    [self.incognitoGridContainerViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [self.regularGridContainerViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],
    [thirdPanelGridContainerViewController.view.widthAnchor
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
// TODO(crbug.com/40273478): Move this to the grid itself when specific grid
// file will be created.
- (void)setupRemoteTabsViewController {
  CHECK(!IsTabGroupSyncEnabled());
  self.remoteTabsViewController.UIDelegate = self;
  // TODO(crbug.com/41366321) : Dark style on remote tabs.
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
  CHECK(!IsTabGroupSyncEnabled());
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

  [self addChildViewController:pinnedTabsViewController];
  [self.view addSubview:pinnedTabsViewController.view];
  [pinnedTabsViewController didMoveToParentViewController:self];

  [self updatePinnedTabsViewControllerConstraints];
}

- (void)configureViewControllerForCurrentSizeClassesAndPage {
  self.configuration = TabGridConfigurationFloatingButton;
  if ([self shouldUseCompactLayout] || _mode == TabGridMode::kSelection) {
    // The bottom toolbar configuration is applied when the UI is narrow but
    // vertically long or the selection mode is enabled.
    self.configuration = TabGridConfigurationBottomToolbar;
  }
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

// Tells the appropriate delegate to create a new item, and then tells the
// presentation delegate to show the new item.
- (void)openNewTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  // Guard against opening new tabs in a page that is disabled. It is the job
  // of the caller to make sure to not open a new tab in a page that can't
  // perform the action. For example, it is an error to attempt to open a new
  // tab in the incognito page when incognito is disabled by policy.
  CHECK([self canPerformOpenNewTabActionForDestinationPage:page]);

  switch (page) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsViewController prepareForDismissal];
      [self.incognitoGridHandler addNewItem];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsViewController prepareForDismissal];
      [self.regularGridHandler addNewItem];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED_IN_MIGRATION()
          << "It is invalid to open a new tab in Recent Tabs.";
      break;
    case TabGridPageTabGroups:
      NOTREACHED_IN_MIGRATION()
          << "It is invalid to open a new tab in Tab Groups.";
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
      NOTREACHED_IN_MIGRATION()
          << "It is invalid to open a new tab from Recent Tabs.";
      break;
    case TabGridPageTabGroups:
      NOTREACHED_IN_MIGRATION()
          << "It is invalid to open a new tab from Tab Groups.";
      break;
  }
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
                     action:@selector(quitSearchMode)
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
  self.scrimView.hidden = NO;
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
        strongSelf.scrimView.alpha = 1.0f;
      }
      completion:^(BOOL finished) {
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
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
      }
      completion:^(BOOL finished) {
        TabGridViewController* strongSelf = weakSelf;
        if (!strongSelf)
          return;
        strongSelf.scrimView.hidden = YES;
        strongSelf.currentPageViewController.accessibilityElementsHidden = NO;
      }];
}

// Updates the appearance of the toolbars based on the scroll position of the
// currently active Grid.
- (void)updateToolbarsAppearance {
  BOOL gridScrolledToTop;
  BOOL gridScrolledToBottom;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      gridScrolledToTop = self.incognitoTabsViewController.scrolledToTop;
      gridScrolledToBottom = self.incognitoTabsViewController.scrolledToBottom;
      break;
    case TabGridPageRegularTabs:
      gridScrolledToTop = self.regularTabsViewController.scrolledToTop;
      gridScrolledToBottom = self.regularTabsViewController.scrolledToBottom;
      break;
    case TabGridPageRemoteTabs:
      gridScrolledToTop = self.remoteTabsViewController.scrolledToTop;
      gridScrolledToBottom = self.remoteTabsViewController.scrolledToBottom;
      break;
    case TabGridPage::TabGridPageTabGroups:
      gridScrolledToTop = self.tabGroupsPanelViewController.scrolledToTop;
      gridScrolledToBottom = self.tabGroupsPanelViewController.scrolledToBottom;
      break;
  }
  [self.topToolbar setScrollViewScrolledToEdge:gridScrolledToTop];
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
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      return _pageConfiguration != TabGridPageConfiguration::kIncognitoPageOnly;
  }
}

// Returns YES if a new tab action that targets the `destinationPage` can be
// performed. The _currentPage can be the same page as the `destinationPage`.
- (BOOL)canPerformOpenNewTabActionForDestinationPage:
    (TabGridPage)destinationPage {
  return [self isPageEnabled:destinationPage] &&
         self.currentPage != TabGridPageRemoteTabs &&
         self.currentPage != TabGridPageTabGroups;
}

// Returns transition layout for the provided `page`.
- (TabGridTransitionItem*)transitionItemForActiveCellWithActivePage:
    (TabGridPage)activePage {
  switch (activePage) {
    case TabGridPageIncognitoTabs:
      return [self.incognitoTabsViewController transitionItemForActiveCell];
    case TabGridPageRegularTabs:
      return [self transitionItemForRegularActiveCell];
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      return nil;
  }
}

// Returns transition layout provider for the regular tabs page.
- (TabGridTransitionItem*)transitionItemForRegularActiveCell {
  if (IsPinnedTabsEnabled() && self.pinnedTabsViewController.hasSelectedCell) {
    return [self.pinnedTabsViewController transitionItemForActiveCell];
  }

  return [self.regularTabsViewController transitionItemForActiveCell];
}

// Quit search mode.
- (void)quitSearchMode {
  [self.mutator quitSearchMode];
}

// Optionally presents a full screen IPH that instructs the user to right swipe
// to view the incognito tab grid. If the delegate determines that the user
// supposed to see this tip, and the IPH fits on the current screen both
// contextually and visually, then it initializes `swipeToIncognitoIPH` and
// presents a GestureInProductHelpView. Otherwise, it keeps
// `swipeToIncognitoIPH` to `nil` and no gestural tip is shown.
- (void)maybeShowSwipeToIncognitoIPH {
  // Return if the regular tabs are visible.
  if (!self.viewVisible || self.currentPage != TabGridPageRegularTabs) {
    return;
  }
  // Check whether the user should see the IPH.
  if (![self.delegate tabGridIsUserEligibleForSwipeToIncognitoIPH]) {
    return;
  }
  // Return if the IPH has already been presented.
  if (self.swipeToIncognitoIPH) {
    return;
  }

  // Create the view.
  UIView* regularGridView = self.regularTabsViewController.view;
  CGSize expectedSize = CGSize();
  CGFloat expectedHeight =
      regularGridView.frame.size.height - self.topToolbar.bounds.size.height;
  expectedHeight -=
      self.view.window.windowScene.statusBarManager.statusBarFrame.size.height;
  if ([self shouldUseCompactLayout]) {
    expectedHeight -= self.bottomToolbar.bounds.size.height;
  }
  expectedSize.height = expectedHeight;
  CGFloat safeAreaInsetForArrowDirection =
      UseRTLLayout() ? regularGridView.safeAreaInsets.right
                     : regularGridView.safeAreaInsets.left;
  expectedSize.width =
      regularGridView.frame.size.width - safeAreaInsetForArrowDirection;

  int stringID = IDS_IOS_SWIPE_RIGHT_TO_INCOGNITO_IPH;
  int voiceOverAnnouncementStringID =
      IDS_IOS_SWIPE_RIGHT_TO_INCOGNITO_IPH_VOICEOVER;
  UISwipeGestureRecognizerDirection swipeDirection =
      UISwipeGestureRecognizerDirectionRight;
  if (UseRTLLayout()) {
    stringID = IDS_IOS_SWIPE_LEFT_TO_INCOGNITO_IPH;
    voiceOverAnnouncementStringID =
        IDS_IOS_SWIPE_LEFT_TO_INCOGNITO_IPH_VOICEOVER;
    swipeDirection = UISwipeGestureRecognizerDirectionLeft;
  }
  GestureInProductHelpView* gestureIPHView = [[GestureInProductHelpView alloc]
               initWithText:l10n_util::GetNSString(stringID)
         bubbleBoundingSize:expectedSize
             swipeDirection:swipeDirection
      voiceOverAnnouncement:l10n_util::GetNSString(
                                voiceOverAnnouncementStringID)];
  [gestureIPHView setTranslatesAutoresizingMaskIntoConstraints:NO];

  // Return if the view does NOT fit in the regular tab grid.
  CGSize smallestPossibleSizeOfIPH = [gestureIPHView
      systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  if (smallestPossibleSizeOfIPH.width > expectedSize.width ||
      smallestPossibleSizeOfIPH.height > expectedSize.height) {
    return;
  }
  if (![self.delegate tabGridShouldPresentSwipeToIncognitoIPH]) {
    return;
  }
  gestureIPHView.delegate = self;
  self.swipeToIncognitoIPH = gestureIPHView;
  [self.view addSubview:self.swipeToIncognitoIPH];
  self.swipeToIncognitoIPHBottomConstraint = [gestureIPHView.bottomAnchor
      constraintEqualToAnchor:[self shouldUseCompactLayout]
                                  ? self.bottomToolbar.topAnchor
                                  : regularGridView.bottomAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [gestureIPHView.leadingAnchor
        constraintEqualToAnchor:regularGridView.leadingAnchor],
    [gestureIPHView.trailingAnchor
        constraintEqualToAnchor:regularGridView.trailingAnchor],
    [gestureIPHView.topAnchor
        constraintEqualToAnchor:self.topToolbar.bottomAnchor],
    self.swipeToIncognitoIPHBottomConstraint
  ]];
  [self.swipeToIncognitoIPH startAnimation];
}

// Called when a drag will begin.
- (void)dragSessionWillBegin {
  self.dragSessionInProgress = YES;
  [self.mutator dragAndDropSessionStarted];

  // Actions on both bars should be disabled during dragging.
  self.topToolbar.pageControl.userInteractionEnabled = NO;
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
  _searchText = searchBar.text;
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
  if ([_searchText isEqualToString:searchText]) {
    // It seems that in some cases, the keyboard is triggered twice in the same
    // runloop. This is a tentative fix to avoid trigger duplicate updates. See
    // crbug.com/336515391.
    return;
  }
  _searchText = searchText;
  searchBar.searchTextField.accessibilityIdentifier =
      [kTabGridSearchTextFieldIdentifierPrefix
          stringByAppendingString:searchText];
  [self updateScrimVisibilityForText:searchText];
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      self.incognitoTabsViewController.searchText = searchText;
      [self updateSearchGrid:self.incognitoGridHandler
              withSearchText:searchText];
      break;
    case TabGridPageRegularTabs:
      self.regularTabsViewController.searchText = searchText;
      [self updateSearchGrid:self.regularGridHandler withSearchText:searchText];
      break;
    case TabGridPageRemoteTabs:
      self.remoteTabsViewController.searchTerms = searchText;
      break;
    case TabGridPage::TabGridPageTabGroups:
      NOTREACHED_IN_MIGRATION() << "Tab Groups doesn't support searching";
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
  if (_mode != TabGridMode::kSearch) {
    return;
  }
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

// Calculates the proper insets for a Tab Grid panel to accommodate for the safe
// area and toolbar.
- (UIEdgeInsets)calculateInsetsForGridView {
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
// accommodate for the safe area and toolbars. It differs from
// `calculateInsetsForGridView` when there is the Pinned Tabs tray to account
// for as well.
- (UIEdgeInsets)calculateInsetsForRegularGridView {
  UIEdgeInsets inset = [self calculateInsetsForGridView];

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

#pragma mark - TabGroupsPanelViewControllerUIDelegate

- (void)tabGroupsPanelViewControllerDidScroll:
    (TabGroupsPanelViewController*)tabGroupsPanelViewController {
  [self updateToolbarsAppearance];
}

#pragma mark - PinnedTabsViewControllerDelegate

- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
             didSelectItemWithID:(web::WebStateID)itemID {
  base::RecordAction(base::UserMetricsAction("MobileTabGridPinnedTabSelected"));
  // Record how long it took to select an item.
  [self reportTabSelectionTime];

  [self.regularGridHandler selectItemWithID:itemID
                                     pinned:YES
                     isFirstActionOnTabGrid:[self status]];

  self.activePage = self.currentPage;
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];

  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO];
}

- (void)pinnedTabsViewControllerVisibilityDidChange:
    (PinnedTabsViewController*)pinnedTabsViewController {
  UIEdgeInsets insets = [self calculateInsetsForRegularGridView];
  [UIView animateWithDuration:kPinnedViewInsetAnimationTime
                   animations:^{
                     self.regularTabsViewController.contentInsets = insets;
                   }];
}

- (void)pinnedTabsViewControllerDidMoveItem:
    (PinnedTabsViewController*)pinnedTabsViewController {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)pinnedTabsViewController:(BaseGridViewController*)gridViewController
             didRemoveItemWithID:(web::WebStateID)itemID {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)pinnedViewControllerDropAnimationWillBegin:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.regularTabsViewController.dropAnimationInProgress = YES;
}

- (void)pinnedViewControllerDropAnimationDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.regularTabsViewController.dropAnimationInProgress = NO;
}

- (void)pinnedViewControllerDragSessionWillBegin:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.dragSessionInProgress = YES;
  [self.mutator dragAndDropSessionStarted];
}

- (void)pinnedViewControllerDragSessionDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController {
  self.dragSessionInProgress = NO;
  [self.mutator dragAndDropSessionEnded];
}

- (void)pinnedViewControllerDidRequestContextMenu:
    (PinnedTabsViewController*)pinnedTabsViewController {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  // Check that the current page matches the grid view being interacted with.
  BOOL isOnRegularTabsPage = self.currentPage == TabGridPageRegularTabs;
  BOOL isOnIncognitoTabsPage = self.currentPage == TabGridPageIncognitoTabs;
  BOOL isOnThirdPanel = self.currentPage == TabGridPageRemoteTabs ||
                        self.currentPage == TabGridPageTabGroups;
  BOOL gridIsRegularTabs = gridViewController == self.regularTabsViewController;
  BOOL gridIsIncognitoTabs =
      gridViewController == self.incognitoTabsViewController;
  if ((isOnRegularTabsPage && !gridIsRegularTabs) ||
      (isOnIncognitoTabsPage && !gridIsIncognitoTabs) || isOnThirdPanel) {
    return;
  }

  if (_mode == TabGridMode::kSelection) {
    return;
  }

  id<GridCommands> tabsDelegate;
  if (gridViewController == self.regularTabsViewController) {
    tabsDelegate = self.regularGridHandler;
    base::RecordAction(base::UserMetricsAction("MobileTabGridOpenRegularTab"));
    if (_mode == TabGridMode::kSearch) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenRegularTabSearchResult"));
    }
  } else if (gridViewController == self.incognitoTabsViewController) {
    tabsDelegate = self.incognitoGridHandler;
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenIncognitoTab"));
    if (_mode == TabGridMode::kSearch) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenIncognitoTabSearchResult"));
    }
  }
  // Record how long it took to select an item.
  [self reportTabSelectionTime];

  // Check if the tab being selected is already selected.
  BOOL alreadySelected = [tabsDelegate isItemWithIDSelected:itemID];

  [tabsDelegate selectItemWithID:itemID
                          pinned:NO
          isFirstActionOnTabGrid:[self status]];

  if (!alreadySelected) {
    [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
  }

  if (_mode == TabGridMode::kSearch) {
    if (![tabsDelegate isItemWithIDSelected:itemID]) {
      // That can happen when the search result that was selected is from
      // another window. In that case don't change the active page for this
      // window and don't show the tab group view.
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridOpenTabGroupSearchResultInAnotherWindow"));
      return;
    } else {
      // Make sure that the keyboard is dismissed before starting the transition
      // to the selected tab.
      [self.view endEditing:YES];
    }
  }

  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group {
  // Check that the current page matches the grid view being interacted with.
  BOOL isOnRegularTabsPage = self.currentPage == TabGridPageRegularTabs;
  BOOL isOnIncognitoTabsPage = self.currentPage == TabGridPageIncognitoTabs;
  BOOL isOnThirdPanel = self.currentPage == TabGridPageRemoteTabs ||
                        self.currentPage == TabGridPageTabGroups;
  BOOL gridIsRegularTabs = gridViewController == self.regularTabsViewController;
  BOOL gridIsIncognitoTabs =
      gridViewController == self.incognitoTabsViewController;
  if ((isOnRegularTabsPage && !gridIsRegularTabs) ||
      (isOnIncognitoTabsPage && !gridIsIncognitoTabs) || isOnThirdPanel) {
    return;
  }

  if (_mode == TabGridMode::kSelection) {
    return;
  }

  id<GridCommands> tabsDelegate;
  if (gridViewController == self.regularTabsViewController) {
    tabsDelegate = self.regularGridHandler;
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenRegularTabGroup"));
    if (_mode == TabGridMode::kSearch) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridOpenRegularTabGroupSearchResult"));
    }
  } else if (gridViewController == self.incognitoTabsViewController) {
    tabsDelegate = self.incognitoGridHandler;
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenIncognitoTabGroup"));
    if (_mode == TabGridMode::kSearch) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridOpenIncognitoTabGroupSearchResult"));
    }
  }

  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];

  [tabsDelegate selectTabGroup:group];

  if (_mode == TabGridMode::kSearch) {
    // Make sure that the keyboard is dismissed.
    [self.view endEditing:YES];
  }
}

// TODO(crbug.com/40273478): Remove once inactive tabs do not depends on it
// anymore.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  // No-op
}

- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWithID:(web::WebStateID)itemID {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)gridViewControllerDragSessionWillBeginForTab:
    (BaseGridViewController*)gridViewController {
  [self dragSessionWillBegin];
  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dragSessionEnabled:YES];
  }
}

- (void)gridViewControllerDragSessionWillBeginForTabGroup:
    (BaseGridViewController*)gridViewController {
  [self dragSessionWillBegin];
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  self.dragSessionInProgress = NO;
  [self.mutator dragAndDropSessionEnded];
  self.topToolbar.pageControl.userInteractionEnabled = YES;

  if (IsPinnedTabsEnabled()) {
    [self.pinnedTabsViewController dragSessionEnabled:NO];
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
  if (self.currentPage != TabGridPageRegularTabs) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileTabGridShowInactiveTabs"));
  [self.delegate showInactiveTabs];
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_IN_MIGRATION();
}

- (void)gridViewControllerDidRequestContextMenu:
    (BaseGridViewController*)gridViewController {
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
  // The searchBar must relinquish its status as first responder to become
  // interactable again.
  [self.topToolbar unfocusSearchBar];
}

- (void)gridViewControllerDropSessionDidEnter:
    (BaseGridViewController*)gridViewController {
  [self.mutator dragAndDropSessionStarted];
}

- (void)gridViewControllerDropSessionDidExit:
    (BaseGridViewController*)gridViewController {
  [self.mutator dragAndDropSessionEnded];
}

#pragma mark - TabGridToolbarsMainTabGridDelegate

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
    [self broadcastIncognitoContentVisibility];
  }
}

- (void)setMode:(TabGridMode)mode {
  if (_mode == mode) {
    return;
  }
  [self tabGridDidPerformAction:TabGridActionType::kInPageAction];
  if (self.swipeToIncognitoIPH) {
    [self.swipeToIncognitoIPH
        dismissWithReason:IPHDismissalReasonType::
                              kTappedOutsideIPHAndAnchorView];
  }

  TabGridMode previousMode = _mode;
  _mode = mode;

  if (previousMode == TabGridMode::kSearch) {
    self.remoteTabsViewController.searchTerms = nil;
    self.regularTabsViewController.searchText = nil;
    self.incognitoTabsViewController.searchText = nil;
    [self.regularGridHandler resetToAllItems];
    [self.incognitoGridHandler resetToAllItems];
    [self hideScrim];
  }

  [self setInsetForGridViews];
  self.scrollView.scrollEnabled = (_mode == TabGridMode::kNormal);
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
    // TODO(crbug.com/40273478): Transform toolbars in view controller directly
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
    // TODO(crbug.com/40246790): Move it to the menu builder once we have the
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
  return [super canPerformAction:action withSender:sender];
}

- (void)validateCommand:(UICommand*)command {
  if (command.action == @selector(keyCommand_find)) {
    command.discoverabilityTitle =
        l10n_util::GetNSStringWithFixup(IDS_IOS_KEYBOARD_SEARCH_TABS);
  } else {
    // TODO(crbug.com/40246790): Add string for change pane's functions.
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

- (void)setActivePageFromPage:(TabGridPage)page {
  self.activePage = page;
}

- (void)prepareForDismissal {
  [self.incognitoTabsViewController prepareForDismissal];
  [self.regularTabsViewController prepareForDismissal];
}

#pragma mark - GestureInProductHelpViewDelegate

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
            didDismissWithReason:(IPHDismissalReasonType)reason {
  [self.delegate tabGridDidDismissSwipeToIncognitoIPHWithReason:reason];
}

- (void)gestureInProductHelpView:(GestureInProductHelpView*)view
    shouldHandleSwipeInDirection:(UISwipeGestureRecognizerDirection)direction {
  [self.mutator pageChanged:TabGridPageIncognitoTabs
                interaction:TabSwitcherPageChangeInteraction::kScrollDrag];
  [self setCurrentPageAndPageControl:TabGridPageIncognitoTabs animated:YES];
}

#pragma mark - TabGridIdleStatusHandler

- (BOOL)status {
  return _idleTabGrid && !_pageChangedSinceEntering &&
         !_backgroundedSinceEntering;
}

- (void)tabGridDidPerformAction:(TabGridActionType)type {
  if (self.viewVisible) {
    switch (type) {
      case TabGridActionType::kInPageAction:
        _idleTabGrid = NO;
        break;
      case TabGridActionType::kChangePage:
        _pageChangedSinceEntering = YES;
        break;
      case TabGridActionType::kBackground:
        _backgroundedSinceEntering = YES;
        break;
    }
  }
}

@end
