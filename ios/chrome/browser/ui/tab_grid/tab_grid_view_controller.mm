// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Types of configurations of this view controller.
typedef NS_ENUM(NSUInteger, TabGridConfiguration) {
  TabGridConfigurationBottomToolbar = 1,
  TabGridConfigurationFloatingButton,
};

// User interaction that triggered a page change, if any.
typedef NS_ENUM(NSUInteger, PageChangeInteraction) {
  // There has been no interaction since the last page change.
  PageChangeInteractionNone = 0,
  // The user dragged in the scroll view to change pages.
  PageChangeInteractionScrollDrag,
  // The user tapped a segment of the page control to change pages.
  PageChangeInteractionPageControlTap,
  // The user dragged the page control slider to change pages.
  PageChangeInteractionPageControlDrag,
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
  kMaxValue = kControlDrag,
};

// Convenience function to record a page change interaction.
void RecordPageChangeInteraction(TabSwitcherPageChangeInteraction interaction) {
  UMA_HISTOGRAM_ENUMERATION(kUMATabSwitcherPageChangeInteractionHistogram,
                            interaction);
}

// Computes the page from the offset and width of |scrollView|.
TabGridPage GetPageFromScrollView(UIScrollView* scrollView) {
  CGFloat pageWidth = scrollView.frame.size.width;
  CGFloat offset = scrollView.contentOffset.x;
  NSUInteger page = lround(offset / pageWidth);
  // Fence |page| to valid values; page values of 3 (rounded up from 2.5) are
  // possible, as are large int values if |pageWidth| is somehow very small.
  page = page < TabGridPageIncognitoTabs ? TabGridPageIncognitoTabs : page;
  page = page > TabGridPageRemoteTabs ? TabGridPageRemoteTabs : page;
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract |page| from the highest-
    // index TabGridPage value.
    return static_cast<TabGridPage>(TabGridPageRemoteTabs - page);
  }
  return static_cast<TabGridPage>(page);
}

NSUInteger GetPageIndexFromPage(TabGridPage page) {
  if (UseRTLLayout()) {
    // In RTL, page indexes are inverted, so subtract |page| from the highest-
    // index TabGridPage value.
    return static_cast<NSUInteger>(TabGridPageRemoteTabs - page);
  }
  return static_cast<NSUInteger>(page);
}
}  // namespace

@interface TabGridViewController () <GridViewControllerDelegate,
                                     UIScrollViewAccessibilityDelegate>
// It is programmer error to broadcast incognito content visibility when the
// view is not visible. Bookkeeping is based on |-viewWillAppear:| and
// |-viewWillDisappear methods. Note that the |Did| methods are not reliably
// called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL broadcasting;
// Child view controllers.
@property(nonatomic, strong) GridViewController* regularTabsViewController;
@property(nonatomic, strong) GridViewController* incognitoTabsViewController;
// Array holding the child page view controllers.
@property(nonatomic, strong) NSArray<UIViewController*>* pageViewControllers;
// Other UI components.
@property(nonatomic, weak) UIScrollView* scrollView;
@property(nonatomic, weak) UIView* scrollContentView;
@property(nonatomic, weak) TabGridTopToolbar* topToolbar;
@property(nonatomic, weak) TabGridBottomToolbar* bottomToolbar;
@property(nonatomic, weak) UIBarButtonItem* doneButton;
@property(nonatomic, weak) UIBarButtonItem* closeAllButton;
@property(nonatomic, assign) BOOL undoCloseAllAvailable;
@property(nonatomic, assign) TabGridConfiguration configuration;
// Setting the current page doesn't scroll the scroll view; use
// -scrollToPage:animated: for that.
@property(nonatomic, assign) TabGridPage currentPage;
// The UIViewController corresponding with |currentPage|.
@property(nonatomic, readonly) UIViewController* currentPageViewController;
// The frame of |self.view| when it initially appeared.
@property(nonatomic, assign) CGRect initialFrame;
// Whether the scroll view is animating its content offset to the current page.
@property(nonatomic, assign, getter=isScrollViewAnimatingContentOffset)
    BOOL scrollViewAnimatingContentOffset;

@property(nonatomic, assign) PageChangeInteraction pageChangeInteraction;
@end

@implementation TabGridViewController
// TabGridPaging property.
@synthesize activePage = _activePage;

- (instancetype)init {
  if (self = [super init]) {
    _regularTabsViewController = [[GridViewController alloc] init];
    _incognitoTabsViewController = [[GridViewController alloc] init];
    _remoteTabsViewController = [[RecentTabsTableViewController alloc] init];
    _pageViewControllers = @[
      _incognitoTabsViewController, _regularTabsViewController,
      _remoteTabsViewController
    ];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  [self setupScrollView];
  [self setupIncognitoTabsViewController];
  [self setupRegularTabsViewController];
  [self setupRemoteTabsViewController];
  [self setupTopToolbar];
  [self setupBottomToolbar];

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

- (void)viewWillAppear:(BOOL)animated {
  self.broadcasting = YES;
  [self.topToolbar.pageControl setSelectedPage:self.currentPage animated:YES];
  [self configureViewControllerForCurrentSizeClassesAndPage];
  // The toolbars should be hidden (alpha 0.0) before the tab appears, so that
  // they can be animated in. They can't be set to 0.0 here, because if
  // |animated| is YES, this method is being called inside the animation block.
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForAppearance];
  } else {
    [self showToolbars];
  }
  [self broadcastIncognitoContentVisibility];
  [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.initialFrame = self.view.frame;
  // Modify Remote Tabs Insets when page appears and during rotation.
  [self setInsetForRemoteTabs];
  // Let image sources know the initial appearance is done.
  [self.regularTabsImageDataSource clearPreloadedSnapshots];
  [self.incognitoTabsImageDataSource clearPreloadedSnapshots];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Modify Incognito and Regular Tabs Insets
  [self setInsetForGridViews];
}

- (void)viewWillDisappear:(BOOL)animated {
  self.undoCloseAllAvailable = NO;
  [self.regularTabsDelegate discardSavedClosedItems];
  // When the view disappears, the toolbar alpha should be set to 0; either as
  // part of the animation, or directly with -hideToolbars.
  if (animated && self.transitionCoordinator) {
    [self animateToolbarsForDisappearance];
  } else {
    [self hideToolbars];
  }
  self.broadcasting = NO;
  [super viewWillDisappear:animated];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  auto animate = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    // Sync the scroll view offset to the  current page value. SInce this is
    // already inside an animation block, the scrolling doesn't need to be
    // animated.
    [self scrollToPage:_currentPage animated:NO];
    [self configureViewControllerForCurrentSizeClassesAndPage];
    [self setInsetForRemoteTabs];
  };
  [coordinator animateAlongsideTransition:animate completion:nil];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)didReceiveMemoryWarning {
  [self.regularTabsImageDataSource clearPreloadedSnapshots];
  [self.incognitoTabsImageDataSource clearPreloadedSnapshots];
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
      [self recordActionSwitchingToPage:_currentPage];
    }
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  // Disable the page control when the user drags on the scroll view since
  // tapping on the page control during scrolling can result in erratic
  // scrolling.
  self.topToolbar.pageControl.userInteractionEnabled = NO;
  self.pageChangeInteraction = PageChangeInteractionScrollDrag;
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // Re-enable the page control since the user isn't dragging anymore.
  self.topToolbar.pageControl.userInteractionEnabled = YES;
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // Mark the interaction as ended, so that scrolls that don't change page don't
  // cause other interactions to be mislabeled.
  self.pageChangeInteraction = PageChangeInteractionNone;

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
  self.currentPage = GetPageFromScrollView(scrollView);
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

#pragma mark - GridTransitionStateProviding properties

- (BOOL)isSelectedCellVisible {
  if (self.activePage != self.currentPage)
    return NO;
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.activePage];
  return gridViewController == nil ? NO
                                   : gridViewController.selectedCellVisible;
}

- (GridTransitionLayout*)layoutForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.activePage];
  if (!gridViewController)
    return nil;

  GridTransitionLayout* layout = [gridViewController transitionLayout];
  layout.frameChanged = !CGRectEqualToRect(self.view.frame, self.initialFrame);
  return layout;
}

- (UIView*)proxyContainerForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  return self.view;
}

- (UIView*)proxyPositionForTransitionContext:
    (id<UIViewControllerContextTransitioning>)context {
  return self.scrollView;
}

#pragma mark - Public Methods

- (void)prepareForAppearance {
  int gridSize = [self approximateVisibleGridCount];
  switch (self.activePage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsImageDataSource
          preloadSnapshotsForVisibleGridSize:gridSize];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsImageDataSource
          preloadSnapshotsForVisibleGridSize:gridSize];
      break;
    case TabGridPageRemoteTabs:
      // Nothing to do.
      break;
  }
}

#pragma mark - Public Properties

- (id<GridConsumer>)regularTabsConsumer {
  return self.regularTabsViewController;
}

- (void)setRegularTabsImageDataSource:
    (id<GridImageDataSource>)regularTabsImageDataSource {
  self.regularTabsViewController.imageDataSource = regularTabsImageDataSource;
  _regularTabsImageDataSource = regularTabsImageDataSource;
}

- (id<GridConsumer>)incognitoTabsConsumer {
  return self.incognitoTabsViewController;
}

- (void)setIncognitoTabsImageDataSource:
    (id<GridImageDataSource>)incognitoTabsImageDataSource {
  self.incognitoTabsViewController.imageDataSource =
      incognitoTabsImageDataSource;
  _incognitoTabsImageDataSource = incognitoTabsImageDataSource;
}

- (id<RecentTabsConsumer>)remoteTabsConsumer {
  return self.remoteTabsViewController;
}

#pragma mark - TabGridPaging

- (void)setActivePage:(TabGridPage)activePage {
  [self scrollToPage:activePage animated:YES];
  _activePage = activePage;
}

#pragma mark - Private

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
  // covers the view's bounds.
  DCHECK(!CGRectIsEmpty(UIEdgeInsetsInsetRect(
      self.remoteTabsViewController.tableView.bounds,
      self.remoteTabsViewController.tableView.safeAreaInsets)));
  self.remoteTabsViewController.additionalSafeAreaInsets = additionalSafeArea;
}

// Sets the proper insets for the Grid ViewControllers to accomodate for the
// safe area and toolbar.
- (void)setInsetForGridViews {
  // Sync the scroll view offset to the current page value if the scroll view
  // isn't scrolling. Don't animate this.
  if (!self.scrollView.dragging && !self.scrollView.decelerating) {
    [self scrollToPage:self.currentPage animated:NO];
  }
  // The content inset of the tab grids must be modified so that the toolbars
  // do not obscure the tabs. This may change depending on orientation.
  CGFloat bottomInset = self.configuration == TabGridConfigurationBottomToolbar
                            ? self.bottomToolbar.intrinsicContentSize.height
                            : 0;
  UIEdgeInsets inset = UIEdgeInsetsMake(
      self.topToolbar.intrinsicContentSize.height, 0, bottomInset, 0);
  inset.left = self.scrollView.safeAreaInsets.left;
  inset.right = self.scrollView.safeAreaInsets.right;
  inset.top += self.scrollView.safeAreaInsets.top;
  inset.bottom += self.scrollView.safeAreaInsets.bottom;
  self.incognitoTabsViewController.gridView.contentInset = inset;
  self.regularTabsViewController.gridView.contentInset = inset;
}

// Returns the corresponding GridViewController for |page|. Returns |nil| if
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
  // Original current page is about to not be visible. Disable it from being
  // focused by VoiceOver.
  self.currentPageViewController.view.accessibilityElementsHidden = YES;
  _currentPage = currentPage;
  self.currentPageViewController.view.accessibilityElementsHidden = NO;
  // Force VoiceOver to update its accessibility element tree immediately.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  nil);
}

// Sets the value of |currentPage|, adjusting the position of the scroll view
// to match. If |animated| is YES, the scroll view change may animate; if it is
// NO, it will never animate.
- (void)scrollToPage:(TabGridPage)targetPage animated:(BOOL)animated {
  // This method should never early return if |targetPage| == |_currentPage|;
  // the ivar may have been set before the scroll view could be updated. Calling
  // this method should always update the scroll view's offset if possible.

  // When VoiceOver is running, the animation can cause state to get out of
  // sync. If the user swipes right during the animation, the VoiceOver cursor
  // goes to the old page, instead of the new page. See crbug.com/978673 for
  // more details.
  if (UIAccessibilityIsVoiceOverRunning()) {
    animated = NO;
  }

  // If the view isn't loaded yet, just do bookkeeping on |currentPage|.
  if (!self.viewLoaded) {
    self.currentPage = targetPage;
    return;
  }

  CGFloat pageWidth = self.scrollView.frame.size.width;
  NSUInteger pageIndex = GetPageIndexFromPage(targetPage);
  CGPoint targetOffset = CGPointMake(pageIndex * pageWidth, 0);

  // If the view is visible and |animated| is YES, animate the change.
  // Otherwise don't.
  if (self.view.window == nil || !animated) {
    [self.scrollView setContentOffset:targetOffset animated:NO];
    self.currentPage = targetPage;
  } else {
    // Only set |scrollViewAnimatingContentOffset| to YES if there's an actual
    // change in the contentOffset, as |-scrollViewDidEndScrollingAnimation:| is
    // never called if the animation does not occur.
    if (!CGPointEqualToPoint(self.scrollView.contentOffset, targetOffset)) {
      self.scrollViewAnimatingContentOffset = YES;
      [self.scrollView setContentOffset:targetOffset animated:YES];
      // |self.currentPage| is set in scrollViewDidEndScrollingAnimation:
    } else {
      self.currentPage = targetPage;
    }
  }

  // TODO(crbug.com/872303) : This is a workaround because TabRestoreService
  // does not notify observers when entries are removed. When close all tabs
  // removes entries, the remote tabs page in the tab grid are not updated. This
  // ensures that the table is updated whenever scrolling to it.
  if (targetPage == TabGridPageRemoteTabs) {
    [self.remoteTabsViewController loadModel];
    [self.remoteTabsViewController.tableView reloadData];
  }
}

- (UIViewController*)currentPageViewController {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      return self.incognitoTabsViewController;
    case TabGridPageRegularTabs:
      return self.regularTabsViewController;
    case TabGridPageRemoteTabs:
      return self.remoteTabsViewController;
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
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageIncognitoTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridIncognitoTabsEmptyStateIdentifier;
  viewController.theme = GridThemeDark;
  viewController.delegate = self;
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
  [self addChildViewController:viewController];
  [contentView addSubview:viewController.view];
  [viewController didMoveToParentViewController:self];
  viewController.emptyStateView =
      [[TabGridEmptyStateView alloc] initWithPage:TabGridPageRegularTabs];
  viewController.emptyStateView.accessibilityIdentifier =
      kTabGridRegularTabsEmptyStateIdentifier;
  viewController.theme = GridThemeLight;
  viewController.delegate = self;
  NSArray* constraints = @[
    [viewController.view.topAnchor
        constraintEqualToAnchor:contentView.topAnchor],
    [viewController.view.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor],
    [viewController.view.leadingAnchor
        constraintEqualToAnchor:self.incognitoTabsViewController.view
                                    .trailingAnchor],
    [viewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

// Adds the remote tabs view controller as a contained view controller, and
// sets constraints.
- (void)setupRemoteTabsViewController {
  // TODO(crbug.com/804589) : Dark style on remote tabs.
  // The styler must be set before the view controller is loaded.
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  styler.tableViewBackgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  // To make using the compile guards easier, use a separate method.
  [self setupRemoteTabsViewControllerForDarkModeWithStyler:styler];
  self.remoteTabsViewController.styler = styler;

  UIView* contentView = self.scrollContentView;
  RecentTabsTableViewController* viewController = self.remoteTabsViewController;
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

// The iOS 13 compile guards are much easier to use in a separate function that
// can be returned from.
- (void)setupRemoteTabsViewControllerForDarkModeWithStyler:
    (ChromeTableViewStyler*)styler {
  // For iOS 13, setting the overrideUserInterfaceStyle to dark forces the use
  // of dark mode colors for all the colors in this view. However, this
  // override is not available on pre-iOS 13 devices, so the dark mode colors
  // must be provided manually.
  if (@available(iOS 13, *)) {
    styler.cellHighlightColor = [UIColor colorNamed:kTableViewRowHighlightColor];
    self.remoteTabsViewController.overrideUserInterfaceStyle =
        UIUserInterfaceStyleDark;
    return;
  }
  styler.cellHighlightColor =
      [UIColor colorNamed:kTableViewRowHighlightDarkColor];
  styler.cellTitleColor = UIColorFromRGB(kGridDarkThemeCellTitleColor);
  styler.headerFooterTitleColor = UIColorFromRGB(kGridDarkThemeCellTitleColor);
  styler.cellDetailColor = UIColorFromRGB(kGridDarkThemeCellDetailColor,
                                          kGridDarkThemeCellDetailAlpha);
  styler.headerFooterDetailColor = UIColorFromRGB(
      kGridDarkThemeCellDetailColor, kGridDarkThemeCellDetailAlpha);
  styler.tintColor = UIColorFromRGB(kGridDarkThemeCellTintColor);
  styler.solidButtonTextColor =
      UIColorFromRGB(kGridDarkThemeCellSolidButtonTextColor);
}

// Adds the top toolbar and sets constraints.
- (void)setupTopToolbar {
  TabGridTopToolbar* topToolbar = [[TabGridTopToolbar alloc] init];
  self.topToolbar = topToolbar;
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topToolbar];

  topToolbar.leadingButton.target = self;
  topToolbar.leadingButton.action = @selector(closeAllButtonTapped:);
  topToolbar.trailingButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);
  topToolbar.trailingButton.accessibilityIdentifier =
      kTabGridDoneButtonIdentifier;
  topToolbar.trailingButton.target = self;
  topToolbar.trailingButton.action = @selector(doneButtonTapped:);

  // Configure and initialize the page control.
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedValue:)
                   forControlEvents:UIControlEventValueChanged];
  [topToolbar.pageControl addTarget:self
                             action:@selector(pageControlChangedPage:)
                   forControlEvents:UIControlEventTouchUpInside];

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

  bottomToolbar.leadingButton.target = self;
  bottomToolbar.leadingButton.action = @selector(closeAllButtonTapped:);
  bottomToolbar.trailingButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_DONE_BUTTON);
  bottomToolbar.trailingButton.accessibilityIdentifier =
      kTabGridDoneButtonIdentifier;
  bottomToolbar.trailingButton.target = self;
  bottomToolbar.trailingButton.action = @selector(doneButtonTapped:);

  [bottomToolbar setNewTabButtonTarget:self
                                action:@selector(newTabButtonTapped:)];
}

- (void)configureViewControllerForCurrentSizeClassesAndPage {
  self.configuration = TabGridConfigurationFloatingButton;
  if (self.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassRegular &&
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassCompact) {
    // The only bottom toolbar configuration is when the UI is narrow but
    // vertically long.
    self.configuration = TabGridConfigurationBottomToolbar;
  }
  switch (self.configuration) {
    case TabGridConfigurationBottomToolbar:
      self.doneButton = self.bottomToolbar.trailingButton;
      self.closeAllButton = self.bottomToolbar.leadingButton;
      break;
    case TabGridConfigurationFloatingButton:
      self.doneButton = self.topToolbar.trailingButton;
      self.closeAllButton = self.topToolbar.leadingButton;
      break;
  }
  [self configureButtonsForActiveAndCurrentPage];
}

- (void)configureButtonsForActiveAndCurrentPage {
  self.bottomToolbar.page = self.currentPage;
  if (self.currentPage == TabGridPageRemoteTabs) {
    [self configureDoneButtonBasedOnPage:self.activePage];
  } else {
    [self configureDoneButtonBasedOnPage:self.currentPage];
  }
  [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
}

- (void)configureDoneButtonBasedOnPage:(TabGridPage)page {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:page];
  if (!gridViewController) {
    NOTREACHED() << "The done button should not be configured based on the "
                    "contents of the recent tabs page.";
  }
  self.doneButton.enabled = !gridViewController.gridEmpty;
}

- (void)configureCloseAllButtonForCurrentPageAndUndoAvailability {
  if (self.undoCloseAllAvailable &&
      self.currentPage == TabGridPageRegularTabs) {
    // Setup closeAllButton as undo button.
    self.closeAllButton.enabled = YES;
    self.closeAllButton.title =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_UNDO_CLOSE_ALL_BUTTON);
    self.closeAllButton.accessibilityIdentifier =
        kTabGridUndoCloseAllButtonIdentifier;
    return;
  }
  // Otherwise setup as a Close All button.
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.currentPage];
  self.closeAllButton.enabled =
      gridViewController == nil ? NO : !gridViewController.gridEmpty;
  self.closeAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
  self.closeAllButton.accessibilityIdentifier =
      kTabGridCloseAllButtonIdentifier;
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

// Records when the user switches between incognito and regular pages in the tab
// grid. Switching to a different TabGridPage can either be driven by dragging
// the scrollView or tapping on the pageControl.
- (void)recordActionSwitchingToPage:(TabGridPage)page {
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
      break;
  }
  switch (self.pageChangeInteraction) {
    case PageChangeInteractionNone:
      // This shouldn't happen, but in case it does happen in release, track it.
      NOTREACHED() << "Recorded a page change with no interaction.";
      RecordPageChangeInteraction(TabSwitcherPageChangeInteraction::kNone);
      break;
    case PageChangeInteractionScrollDrag:
      RecordPageChangeInteraction(
          TabSwitcherPageChangeInteraction::kScrollDrag);
      break;
    case PageChangeInteractionPageControlTap:
      RecordPageChangeInteraction(
          TabSwitcherPageChangeInteraction::kControlTap);
      break;
    case PageChangeInteractionPageControlDrag:
      RecordPageChangeInteraction(
          TabSwitcherPageChangeInteraction::kControlDrag);
      break;
  }
  self.pageChangeInteraction = PageChangeInteractionNone;
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
                                       focusOmnibox:focusOmnibox];
}

// Creates and shows a new regular tab.
- (void)openNewRegularTabForKeyboardCommand {
  [self openNewTabInPage:TabGridPageRegularTabs focusOmnibox:YES];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCreateRegularTabKeyboard"));
}

// Creates and shows a new incognito tab.
- (void)openNewIncognitoTabForKeyboardCommand {
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
      // Tabs cannot be opened with ⌘-t from the remote tabs page.
      break;
  }
}

// Broadcasts whether incognito tabs are showing.
- (void)broadcastIncognitoContentVisibility {
  if (!self.broadcasting)
    return;
  BOOL incognitoContentVisible =
      (self.currentPage == TabGridPageIncognitoTabs &&
       !self.incognitoTabsViewController.gridEmpty);
  [self.dispatcher setIncognitoContentVisible:incognitoContentVisible];
}

// Returns the approximate number of grid cells that will be visible on this
// device.
- (int)approximateVisibleGridCount {
  if (IsRegularXRegularSizeClass(self)) {
    // A 12" iPad Pro can show 30 cells in the tab grid.
    return 30;
  }
  if (IsCompactWidth(self)) {
    // A portrait phone shows up to four rows of two cells.
    return 8;
  }
  // A landscape phone shows up to three rows of four cells.
  return 12;
}

// Sets both the current page and page control's selected page to |page|.
// Animation is used if |animated| is YES.
- (void)setCurrentPageAndPageControl:(TabGridPage)page animated:(BOOL)animated {
  if (self.topToolbar.pageControl.selectedPage != page)
    [self.topToolbar.pageControl setSelectedPage:page animated:animated];
  if (self.currentPage != page) {
    [self scrollToPage:page animated:animated];
  }
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  // Update the model with the tab selection, but don't have the grid view
  // controller display the new selection, since there will be a transition
  // away from it immediately afterwards.
  gridViewController.showsSelectionUpdates = NO;
  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate selectItemWithID:itemID];
    // Record when a regular tab is opened.
    base::RecordAction(base::UserMetricsAction("MobileTabGridOpenRegularTab"));
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate selectItemWithID:itemID];
    // Record when an incognito tab is opened.
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenIncognitoTab"));
  }
  self.activePage = self.currentPage;
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO];
  gridViewController.showsSelectionUpdates = YES;
}

- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID {
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

- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex {
  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate moveItemWithID:itemID toIndex:destinationIndex];
  }
}

- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  [self configureButtonsForActiveAndCurrentPage];
  if (gridViewController == self.regularTabsViewController) {
    self.topToolbar.pageControl.regularTabCount = count;
    breakpad_helper::SetRegularTabCount(count);
  } else if (gridViewController == self.incognitoTabsViewController) {
    breakpad_helper::SetIncognitoTabCount(count);

    // No assumption is made as to the state of the UI. This method can be
    // called with an incognito view controller and a current page that is not
    // the incognito tabs.
    if (count == 0 && self.currentPage == TabGridPageIncognitoTabs) {
      // Show the regular tabs to the user if the last incognito tab is closed.
      self.activePage = TabGridPageRegularTabs;
      if (self.viewLoaded && self.view.window) {
        // Visibly scroll to the regular tabs panel after a slight delay when
        // the user is already in the tab switcher.
        // Per crbug.com/980844, if the user has VoiceOver enabled, don't delay
        // and just animate immediately; delaying the scrolling will cause
        // VoiceOver to focus the text on the Incognito page.
        __weak TabGridViewController* weakSelf = self;
        auto scrollToRegularTabs = ^{
          [weakSelf setCurrentPageAndPageControl:TabGridPageRegularTabs
                                        animated:YES];
        };
        if (UIAccessibilityIsVoiceOverRunning()) {
          scrollToRegularTabs();
        } else {
          base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
              kTabGridScrollAnimationDelayInMilliseconds);
          base::PostDelayedTask(FROM_HERE, {web::WebThread::UI},
                                base::BindOnce(scrollToRegularTabs), delay);
        }
      } else {
        // Directly show the regular tab page without animation if
        // the user was not already in tab switcher.
        [self setCurrentPageAndPageControl:TabGridPageRegularTabs animated:NO];
      }
    }
  }

  [self broadcastIncognitoContentVisibility];
}

#pragma mark - Control actions

- (void)doneButtonTapped:(id)sender {
  TabGridPage newActivePage = self.currentPage;
  if (self.currentPage == TabGridPageRemoteTabs) {
    newActivePage = self.activePage;
  }
  self.activePage = newActivePage;
  // Holding the done button down when it is enabled could result in done tap
  // being triggered on release after tabs have been closed and the button
  // disabled. Ensure that action is only taken on a valid state.
  if (![[self gridViewControllerForPage:newActivePage] isGridEmpty]) {
    [self.tabPresentationDelegate showActiveTabInPage:newActivePage
                                         focusOmnibox:NO];
    // Record when users exit the tab grid to return to the current foreground
    // tab.
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
  }
}

- (void)closeAllButtonTapped:(id)sender {
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridCloseAllIncognitoTabs"));
      [self.incognitoTabsDelegate closeAllItems];
      break;
    case TabGridPageRegularTabs:
      DCHECK_EQ(self.undoCloseAllAvailable,
                self.regularTabsViewController.gridEmpty);
      if (self.undoCloseAllAvailable) {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));
        [self.regularTabsDelegate undoCloseAllItems];
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));
        [self.regularTabsDelegate saveAndCloseAllItems];
      }
      self.undoCloseAllAvailable = !self.undoCloseAllAvailable;
      [self configureCloseAllButtonForCurrentPageAndUndoAvailability];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to call close all tabs on remote tabs.";
      break;
  }
}

- (void)newTabButtonTapped:(id)sender {
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

- (void)pageControlChangedValue:(id)sender {
  // Map the page control slider position (in the range 0.0-1.0) to an
  // x-offset for the scroll view.
  CGFloat offset = self.topToolbar.pageControl.sliderPosition;
  // In RTL, flip the offset.
  if (UseRTLLayout())
    offset = 1.0 - offset;

  self.pageChangeInteraction = PageChangeInteractionPageControlDrag;

  // Total space available for the scroll view to scroll (horizontally).
  CGFloat offsetWidth =
      self.scrollView.contentSize.width - self.scrollView.frame.size.width;
  CGPoint contentOffset = self.scrollView.contentOffset;
  // Find the final offset by using |offset| as a fraction of the available
  // scroll width.
  contentOffset.x = offsetWidth * offset;
  self.scrollView.contentOffset = contentOffset;
}

- (void)pageControlChangedPage:(id)sender {
  TabGridPage newPage = self.topToolbar.pageControl.selectedPage;
  // If the user has dragged the page control, -pageControlChangedPage: will be
  // called after the calls to -pageControlChangedValue:, so only set the
  // interaction here if one hasn't already been set.
  if (self.pageChangeInteraction == PageChangeInteractionNone)
    self.pageChangeInteraction = PageChangeInteractionPageControlTap;

  TabGridPage currentPage = self.currentPage;
  [self scrollToPage:newPage animated:YES];
  // Records when the user uses the pageControl to switch pages.
  if (currentPage != newPage)
    [self recordActionSwitchingToPage:newPage];
  // Regardless of whether the page changed, mark the interaction as done.
  self.pageChangeInteraction = PageChangeInteractionNone;
}

#pragma mark - UIResponder

- (NSArray*)keyCommands {
  UIKeyCommand* newWindowShortcut = [UIKeyCommand
       keyCommandWithInput:@"n"
             modifierFlags:UIKeyModifierCommand
                    action:@selector(openNewRegularTabForKeyboardCommand)
      discoverabilityTitle:l10n_util::GetNSStringWithFixup(
                               IDS_IOS_TOOLS_MENU_NEW_TAB)];
  UIKeyCommand* newIncognitoWindowShortcut = [UIKeyCommand
       keyCommandWithInput:@"n"
             modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                    action:@selector(openNewIncognitoTabForKeyboardCommand)
      discoverabilityTitle:l10n_util::GetNSStringWithFixup(
                               IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)];
  UIKeyCommand* newTabShortcut = [UIKeyCommand
       keyCommandWithInput:@"t"
             modifierFlags:UIKeyModifierCommand
                    action:@selector(openNewTabInCurrentPageForKeyboardCommand)
      discoverabilityTitle:l10n_util::GetNSStringWithFixup(
                               IDS_IOS_TOOLS_MENU_NEW_TAB)];
  return @[ newWindowShortcut, newIncognitoWindowShortcut, newTabShortcut ];
}

@end
