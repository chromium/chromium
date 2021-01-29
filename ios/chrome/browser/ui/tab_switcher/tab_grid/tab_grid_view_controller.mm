// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_empty_state_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_control.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/thumb_strip_plus_sign_button.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_layout.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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
                                     LayoutSwitcher,
                                     UIScrollViewAccessibilityDelegate>
// Whether the view is visible. Bookkeeping is based on |-viewWillAppear:| and
// |-viewWillDisappear methods. Note that the |Did| methods are not reliably
// called (e.g., edge case in multitasking).
@property(nonatomic, assign) BOOL viewVisible;
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
// Bool informing if the confirmation action sheet is displayed.
@property(nonatomic, assign) BOOL closeAllConfirmationDisplayed;
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
// UIView whose background color changes to create a fade-in / fade-out effect
// when revealing / hiding the Thumb Strip.
@property(nonatomic, weak) UIView* foregroundView;
// Button with a plus sign that opens a new tab, located on the right side of
// the thumb strip, shown when the plus sign cell isn't visible.
@property(nonatomic, weak) ThumbStripPlusSignButton* plusSignButton;

// The current state of the tab grid when using the thumb strip.
@property(nonatomic, assign) ViewRevealState currentState;
@end

@implementation TabGridViewController
// TabGridPaging property.
@synthesize activePage = _activePage;

- (instancetype)init {
  if (self = [super init]) {
    _regularTabsViewController = [[GridViewController alloc] init];
    _incognitoTabsViewController = [[GridViewController alloc] init];
    _remoteTabsViewController = [[RecentTabsTableViewController alloc] init];
    _closeAllConfirmationDisplayed = NO;
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
  if (IsThumbStripEnabled()) {
    [self setupThumbStripPlusSignButton];
    [self setupForegroundView];
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
  // Modify Incognito and Regular Tabs Insets
  [self setInsetForGridViews];
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
    [self setInsetForGridViews];
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
  [self arriveAtCurrentPage];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  self.currentPage = GetPageFromScrollView(scrollView);
  self.scrollViewAnimatingContentOffset = NO;
  [self arriveAtCurrentPage];
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
  if (self.activePage != self.currentPage)
    return NO;
  GridViewController* gridViewController =
      [self gridViewControllerForPage:self.activePage];
  return gridViewController == nil ? NO
                                   : gridViewController.selectedCellVisible;
}

- (GridTransitionLayout*)transitionLayout:(TabGridPage)activePage {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:activePage];
  if (!gridViewController)
    return nil;

  GridTransitionLayout* layout = [gridViewController transitionLayout];
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

- (void)contentWillAppearAnimated:(BOOL)animated {
  self.viewVisible = YES;
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

  [self.incognitoTabsViewController contentWillAppearAnimated:animated];
  [self.regularTabsViewController contentWillAppearAnimated:animated];

  if (@available(iOS 13.0, *)) {
    self.remoteTabsViewController.session =
        self.view.window.windowScene.session;
  }

  self.remoteTabsViewController.preventUpdates = NO;
}

- (void)contentDidAppear {
  self.initialFrame = self.view.frame;
  // Modify Remote Tabs Insets when page appears and during rotation.
  [self setInsetForRemoteTabs];
  // Let image sources know the initial appearance is done.
  [self.regularTabsImageDataSource clearPreloadedSnapshots];
  [self.incognitoTabsImageDataSource clearPreloadedSnapshots];
}

- (void)contentWillDisappearAnimated:(BOOL)animated {
  self.undoCloseAllAvailable = NO;
  [self.regularTabsDelegate discardSavedClosedItems];
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
  self.remoteTabsViewController.preventUpdates = YES;
}

- (void)closeAllTabsConfirmationClosed {
  self.closeAllConfirmationDisplayed = NO;
  [self configureButtonsForActiveAndCurrentPage];
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

- (void)setReauthHandler:(id<IncognitoReauthCommands>)reauthHandler {
  if (_reauthHandler == reauthHandler)
    return;
  _reauthHandler = reauthHandler;
  self.incognitoTabsViewController.handler = self.reauthHandler;
}

#pragma mark - TabGridPaging

- (void)setActivePage:(TabGridPage)activePage {
  [self scrollToPage:activePage animated:YES];
  _activePage = activePage;
}

#pragma mark - LayoutSwitcherProvider

- (id<LayoutSwitcher>)layoutSwitcher {
  return self;
}

#pragma mark - LayoutSwitcher
- (void)willTransitionToLayout:(LayoutSwitcherState)nextState
                    completion:
                        (void (^)(BOOL completed, BOOL finished))completion {
  GridViewController* regularViewController =
      [self gridViewControllerForPage:TabGridPageRegularTabs];
  GridViewController* incognitoViewController =
      [self gridViewControllerForPage:TabGridPageIncognitoTabs];

  // Each LayoutSwitcher method calls regular and icognito grid controller's
  // corresponding method. Thus, attaching the completion to only one of the
  // grid view controllers should suffice.
  [regularViewController willTransitionToLayout:nextState
                                     completion:completion];
  [incognitoViewController willTransitionToLayout:nextState completion:nil];
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

- (void)willAnimateViewReveal:(ViewRevealState)currentViewRevealState {
  self.currentState = currentViewRevealState;
  self.scrollView.scrollEnabled = NO;
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
      [self contentWillAppearAnimated:YES];
      break;
    }
    case ViewRevealState::Peeked:
      break;
    case ViewRevealState::Revealed:
      self.plusSignButton.alpha = 0;
      break;
  }
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
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

- (void)didAnimateViewReveal:(ViewRevealState)viewRevealState {
  self.currentState = viewRevealState;
  switch (viewRevealState) {
    case ViewRevealState::Hidden:
      [self.delegate tabGridViewControllerDidDismiss:self];
      break;
    case ViewRevealState::Peeked:
      // No-op.
      break;
    case ViewRevealState::Revealed:
      self.scrollView.scrollEnabled = YES;
      [self setInsetForRemoteTabs];
      break;
  }
}

#pragma mark - Private

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
// and setting its alpha to |opacity|.
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
  if (IsThumbStripEnabled()) {
    bottomInset += self.topToolbar.intrinsicContentSize.height;
  }
  CGFloat topInset =
      IsThumbStripEnabled() ? 0 : self.topToolbar.intrinsicContentSize.height;
  UIEdgeInsets inset = UIEdgeInsetsMake(topInset, 0, bottomInset, 0);
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
  if (!self.viewVisible || !animated) {
    [self.scrollView setContentOffset:targetOffset animated:NO];
    self.currentPage = targetPage;
    // Important updates (e.g., button configurations, incognito visibility) are
    // made at the end of scrolling animations after |self.currentPage| is set.
    // Since this codepath has no animations, updates must be called manually.
    [self arriveAtCurrentPage];
    [self broadcastIncognitoContentVisibility];
    [self configureButtonsForActiveAndCurrentPage];
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

// Updates the state when the scroll view stops scrolling at a given page,
// whether the scroll is from dragging or programmatic.
- (void)arriveAtCurrentPage {
  if (!self.viewVisible) {
    return;
  }
  if (IsThumbStripEnabled()) {
    [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                         focusOmnibox:NO
                                         closeTabGrid:NO];
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
  if (IsThumbStripEnabled()) {
    self.scrollView.scrollEnabled = NO;
  }
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
    styler.cellHighlightColor =
        [UIColor colorNamed:kTableViewRowHighlightColor];
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

  // Sets the leadingButton title during initialization allows the actionSheet
  // to be correctly anchored. See: crbug.com/1140982.
  topToolbar.leadingButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
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

// Adds the foreground view and sets constraints.
- (void)setupForegroundView {
  UIView* foregroundView = [[UIView alloc] init];
  self.foregroundView = foregroundView;
  foregroundView.translatesAutoresizingMaskIntoConstraints = NO;
  foregroundView.userInteractionEnabled = NO;
  foregroundView.backgroundColor = [UIColor blackColor];
  [self.view addSubview:foregroundView];
  AddSameConstraints(foregroundView, self.view);
}

// Adds the thumb strip's plus sign button, which is visible when the plus sign
// cell isn't.
- (void)setupThumbStripPlusSignButton {
  ThumbStripPlusSignButton* plusSignButton =
      [[ThumbStripPlusSignButton alloc] init];
  self.plusSignButton = plusSignButton;
  plusSignButton.translatesAutoresizingMaskIntoConstraints = NO;
  [plusSignButton addTarget:self
                     action:@selector(newTabButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];
  [self.view addSubview:plusSignButton];

  NSArray* constraints = @[
    [plusSignButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [plusSignButton.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [plusSignButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [plusSignButton.widthAnchor constraintEqualToConstant:kPlusSignButtonWidth],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
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

  [self configureNewTabButtonBasedOnContentPermissions];
}

- (void)configureNewTabButtonBasedOnContentPermissions {
  BOOL allowNewTab =
      !((self.currentPage == TabGridPageIncognitoTabs) &&
        self.incognitoTabsViewController.contentNeedsAuthentication);
  [self.bottomToolbar setNewTabButtonEnabled:allowNewTab];
}

- (void)configureDoneButtonBasedOnPage:(TabGridPage)page {
  GridViewController* gridViewController =
      [self gridViewControllerForPage:page];
  if (!gridViewController) {
    NOTREACHED() << "The done button should not be configured based on the "
                    "contents of the recent tabs page.";
  }

  if (!self.closeAllConfirmationDisplayed)
    self.topToolbar.pageControl.userInteractionEnabled = YES;

  // The Done button should have the same behavior as the other buttons on the
  // top Toolbar.
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);

  self.doneButton.enabled =
      !gridViewController.gridEmpty &&
      self.topToolbar.pageControl.userInteractionEnabled &&
      !incognitoTabsNeedsAuth;
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
  BOOL enabled =
      (gridViewController == nil) ? NO : !gridViewController.gridEmpty;
  BOOL incognitoTabsNeedsAuth =
      (self.currentPage == TabGridPageIncognitoTabs &&
       self.incognitoTabsViewController.contentNeedsAuthentication);
  enabled = enabled && !incognitoTabsNeedsAuth;

  self.closeAllButton.enabled = enabled;
  self.closeAllButton.title =
      l10n_util::GetNSString(IDS_IOS_TAB_GRID_CLOSE_ALL_BUTTON);
  // Setting the |accessibilityIdentifier| seems to trigger layout, which causes
  // an infinite loop.
  if (self.closeAllButton.accessibilityIdentifier !=
      kTabGridCloseAllButtonIdentifier) {
    self.closeAllButton.accessibilityIdentifier =
        kTabGridCloseAllButtonIdentifier;
  }
}

// Shows the two toolbars and the floating button. Suitable for use in
// animations.
- (void)showToolbars {
  [self.topToolbar show];
  if (IsThumbStripEnabled()) {
    GridViewController* gridViewController =
        [self gridViewControllerForPage:self.currentPage];
    DCHECK(gridViewController);
    if (gridViewController.fractionVisibleOfLastItem >= 0.999) {
      // Don't show the bottom new tab button because the plus sign cell is
      // visible.
      return;
    }
    self.plusSignButton.alpha =
        1 - gridViewController.fractionVisibleOfLastItem;
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
                                       focusOmnibox:focusOmnibox
                                       closeTabGrid:YES];
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
  // It is programmer error to broadcast incognito content visibility when the
  // view is not visible.
  if (!self.viewVisible)
    return;
  BOOL incognitoContentVisible =
      (self.currentPage == TabGridPageIncognitoTabs &&
       !self.incognitoTabsViewController.gridEmpty);
  [self.handler setIncognitoContentVisible:incognitoContentVisible];
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
  // Check if the tab being selected is already selected.
  BOOL alreadySelected = NO;
  if (gridViewController == self.regularTabsViewController) {
    alreadySelected = [self.regularTabsDelegate isItemWithIDSelected:itemID];
    [self.regularTabsDelegate selectItemWithID:itemID];
    // Record when a regular tab is opened.
    base::RecordAction(base::UserMetricsAction("MobileTabGridOpenRegularTab"));
  } else if (gridViewController == self.incognitoTabsViewController) {
    alreadySelected = [self.incognitoTabsDelegate isItemWithIDSelected:itemID];
    [self.incognitoTabsDelegate selectItemWithID:itemID];
    // Record when an incognito tab is opened.
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridOpenIncognitoTab"));
  }
  self.activePage = self.currentPage;
  // When the tab grid is peeked, selecting an item should not close the grid
  // unless the user has selected an already selected tab.
  BOOL closeTabGrid =
      alreadySelected || self.currentState != ViewRevealState::Peeked;
  [self.tabPresentationDelegate showActiveTabInPage:self.currentPage
                                       focusOmnibox:NO
                                       closeTabGrid:closeTabGrid];
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

- (void)didTapPlusSignInGridViewController:
    (GridViewController*)gridViewController {
  if (gridViewController == self.regularTabsViewController) {
    [self.regularTabsDelegate addNewItem];
    // TODO(crbug.com/1135329): Record when a new regular tab is opened.
  } else if (gridViewController == self.incognitoTabsViewController) {
    [self.incognitoTabsDelegate addNewItem];
    // TODO(crbug.com/1135329): Record when a new incognito tab is opened.
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
  if (count > 0) {
    // Undo is only available when the tab grid is empty.
    self.undoCloseAllAvailable = NO;
  }
  [self configureButtonsForActiveAndCurrentPage];
  if (gridViewController == self.regularTabsViewController) {
    self.topToolbar.pageControl.regularTabCount = count;
    crash_keys::SetRegularTabCount(count);
  } else if (gridViewController == self.incognitoTabsViewController) {
    crash_keys::SetIncognitoTabCount(count);

    // No assumption is made as to the state of the UI. This method can be
    // called with an incognito view controller and a current page that is not
    // the incognito tabs.
    if (count == 0 && self.currentPage == TabGridPageIncognitoTabs) {
      // Show the regular tabs to the user if the last incognito tab is closed.
      self.activePage = TabGridPageRegularTabs;
      if (self.viewLoaded && self.viewVisible) {
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

- (void)didChangeLastItemVisibilityInGridViewController:
    (GridViewController*)gridViewController {
  CGFloat lastItemVisiblity = gridViewController.fractionVisibleOfLastItem;
  self.plusSignButton.alpha = 1 - lastItemVisiblity;
  self.plusSignButton.plusSignImage.transform =
      lastItemVisiblity < 1
          ? CGAffineTransformMakeTranslation(
                lastItemVisiblity * kScrollThresholdForPlusSignButtonHide, 0)
          : CGAffineTransformIdentity;
}

- (void)gridViewController:(GridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
  [self configureButtonsForActiveAndCurrentPage];
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
                                         focusOmnibox:NO
                                         closeTabGrid:YES];
    // Record when users exit the tab grid to return to the current foreground
    // tab.
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
  }
}

// Shows an action sheet that asks for confirmation when 'Close All' button is
// tapped.
- (void)closeAllButtonTappedShowConfirmation {
  // Sets the action sheet anchor on the leading button of the top
  // toolbar in order to avoid alignment issues when changing the device
  // orientation to landscape in multi window mode.
  UIBarButtonItem* buttonAnchor = self.topToolbar.leadingButton;
  self.closeAllConfirmationDisplayed = YES;
  self.topToolbar.pageControl.userInteractionEnabled = NO;
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate
          showCloseAllConfirmationActionSheetWithAnchor:buttonAnchor];
      break;
    case TabGridPageRegularTabs:
      [self.regularTabsDelegate
          showCloseAllConfirmationActionSheetWithAnchor:buttonAnchor];
      break;
    case TabGridPageRemoteTabs:
      NOTREACHED() << "It is invalid to call close all tabs on remote tabs.";
      break;
  }
}

- (void)closeAllButtonTapped:(id)sender {
  if (IsCloseAllTabsConfirmationEnabled()) {
    [self closeAllButtonTappedShowConfirmation];
    return;
  }
  switch (self.currentPage) {
    case TabGridPageIncognitoTabs:
      [self.incognitoTabsDelegate closeAllItems];
      break;
    case TabGridPageRegularTabs:
      DCHECK_EQ(self.undoCloseAllAvailable,
                self.regularTabsViewController.gridEmpty);
      if (self.undoCloseAllAvailable) {
        [self.regularTabsDelegate undoCloseAllItems];
        self.undoCloseAllAvailable = NO;
      } else {
        [self.regularTabsDelegate saveAndCloseAllItems];
        self.undoCloseAllAvailable = YES;
      }
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
                   action:@selector(openNewRegularTabForKeyboardCommand)];
  newWindowShortcut.discoverabilityTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_NEW_TAB);
  UIKeyCommand* newIncognitoWindowShortcut = [UIKeyCommand
      keyCommandWithInput:@"n"
            modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                   action:@selector(openNewIncognitoTabForKeyboardCommand)];
  newIncognitoWindowShortcut.discoverabilityTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
  UIKeyCommand* newTabShortcut = [UIKeyCommand
      keyCommandWithInput:@"t"
            modifierFlags:UIKeyModifierCommand
                   action:@selector(openNewTabInCurrentPageForKeyboardCommand)];
  newTabShortcut.discoverabilityTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_NEW_TAB);
  return @[ newWindowShortcut, newIncognitoWindowShortcut, newTabShortcut ];
}

@end
