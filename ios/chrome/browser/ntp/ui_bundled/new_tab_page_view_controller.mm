// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_view_controller.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_collection_view.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace {
// Animation time for the shift up/down animations to focus/defocus omnibox.
const CGFloat kShiftTilesUpAnimationDuration = 0.1;
// The minimum height of the feed container.
const CGFloat kFeedContainerMinimumHeight = 1000;
// Added height to the feed container so that it doesn't end abruptly on
// overscroll.
const CGFloat kFeedContainerExtraHeight = 500;
}  // namespace

@interface NewTabPageViewController () <UICollectionViewDelegate,
                                        UIGestureRecognizerDelegate>

// The overscroll actions controller managing accelerators over the toolbar.
@property(nonatomic, strong)
    OverscrollActionsController* overscrollActionsController;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
// With Web Channels enabled, also determines if the feed header is stuck to the
// top.
@property(nonatomic, assign, getter=isScrolledIntoFeed) BOOL scrolledIntoFeed;

// Whether or not the fake omnibox is pinned to the top of the NTP. Redefined
// to make readwrite.
@property(nonatomic, assign) BOOL isFakeboxPinned;

// Array of constraints used to pin the fake Omnibox header into the top of the
// view.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* fakeOmniboxConstraints;

// Constraint that pins the fake Omnibox to the top of the view. A subset of
// `fakeOmniboxConstraints`.
@property(nonatomic, strong) NSLayoutConstraint* headerTopAnchor;

// Array of constraints used to pin the feed header to the top of the NTP. Only
// applicable with Web Channels enabled.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedHeaderConstraints;

// Constraint for the height of the container view surrounding the feed.
@property(nonatomic, strong) NSLayoutConstraint* feedContainerHeightConstraint;

// `YES` if the NTP starting content offset should be set to a previous scroll
// state (when navigating away and back), and `NO` if it should be the top of
// the NTP.
@property(nonatomic, assign) BOOL hasSavedOffsetFromPreviousScrollState;

// The content offset saved from a previous scroll state in the NTP. If this is
// set, `hasSavedOffsetFromPreviousScrollState` should be YES.
@property(nonatomic, assign) CGFloat savedScrollOffset;

// The scroll position when a scrolling event starts.
@property(nonatomic, assign) int scrollStartPosition;

// Whether the omnibox should be focused once the collection view appears.
@property(nonatomic, assign) BOOL shouldFocusFakebox;

// Array of all view controllers above the feed.
@property(nonatomic, strong)
    NSMutableArray<UIViewController*>* viewControllersAboveFeed;

// Identity disc shown in the NTP.
// TODO(crbug.com/40165977): Remove once the Feed header properly supports
// ContentSuggestions.
@property(nonatomic, weak) UIButton* identityDiscButton;

// Tap gesture recognizer when the omnibox is focused.
@property(nonatomic, strong) UITapGestureRecognizer* tapGestureRecognizer;

// Animator for the `shiftTilesUpToFocusOmnibox` animation.
@property(nonatomic, strong) UIViewPropertyAnimator* animator;

// When the omnibox is focused, this value represents the shift distance of the
// collection needed to pin the omnibox to the top. It is 0 if the omnibox has
// not been moved when focused (i.e. the collection was already scrolled to
// top).
@property(nonatomic, assign, readwrite) CGFloat collectionShiftingOffset;

// `YES` if the collection is scrolled to the point where the omnibox is stuck
// to the top of the NTP. Used to lock this position in place on various frame
// changes.
@property(nonatomic, assign, readwrite) BOOL scrolledToMinimumHeight;

// If YES the animations of the fake omnibox triggered when the collection is
// scrolled (expansion) are disabled. This is used for the fake omnibox focus
// animations so the constraints aren't changed while the ntp is scrolled.
@property(nonatomic, assign) BOOL disableScrollAnimation;

// `YES` if the fakebox header should be animated on scroll.
@property(nonatomic, assign) BOOL shouldAnimateHeader;

// Keeps track of how long the shift down animation has taken. Used to update
// the Content Suggestions header as the animation progresses.
@property(nonatomic, assign) CFTimeInterval shiftTileStartTime;

// YES if `-viewDidLoad:` has finished executing. This is used to ensure that
// constraints are not set before the views have been added to view hierarchy.
@property(nonatomic, assign) BOOL viewDidFinishLoading;

// YES if the NTP is in the middle of animating an omnibox focus.
@property(nonatomic, assign) BOOL isAnimatingOmniboxFocus;

// `YES` when notifications indicate the omnibox is focused.
@property(nonatomic, assign) BOOL omniboxFocused;

// When set to YES, the scroll position wont be updated.
@property(nonatomic, assign) BOOL inhibitScrollPositionUpdates;

// YES if there is a currently running "shift down" / omnibox defocus animation
// running.
@property(nonatomic, assign) BOOL shiftDownInProgress;

@end

@implementation NewTabPageViewController {
  // Background gradient when Modular Home is enabled.
  GradientView* _backgroundGradientView;
  // Container view surrounding the feed.
  UIView* _feedContainer;
  // YES if the view is in the process of appearing, but viewDidAppear hasn't
  // finished yet.
  BOOL _appearing;
  // Layout Guide for NTP modules.
  UILayoutGuide* _moduleLayoutGuide;
  // Constraint controlling the width of modules on the NTP.
  NSLayoutConstraint* _moduleWidth;
}

// Properties synthesized from NewTabPageConsumer.
@synthesize mostVisitedVisible = _mostVisitedVisible;
@synthesize magicStackVisible = _magicStackVisible;

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _viewControllersAboveFeed = [[NSMutableArray alloc] init];

    _tapGestureRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(unfocusOmnibox)];

    _collectionShiftingOffset = 0;
    _shouldAnimateHeader = YES;
    _focusAccessibilityOmniboxWhenViewAppears = YES;
    _inhibitScrollPositionUpdates = NO;
    _shiftTileStartTime = -1;
    _appearing = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  DCHECK(self.feedWrapperViewController);

  self.view.accessibilityIdentifier = kNTPViewIdentifier;

  // TODO(crbug.com/40799579): Remove this when bug is fixed.
  [self.feedWrapperViewController loadViewIfNeeded];
  [self.contentSuggestionsViewController loadViewIfNeeded];

  // Prevent the NTP from spilling behind the toolbar and tab strip.
  self.view.clipsToBounds = YES;

  // TODO(crbug.com/40251609): The contentCollectionView width might be narrower
  // than the ContentSuggestions view. This causes elements to be hidden. A
  // gesture recognizer is added to allow these elements to be interactable.
  UITapGestureRecognizer* singleTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSingleTapInView:)];
  singleTapRecognizer.delegate = self;
  [self.view addGestureRecognizer:singleTapRecognizer];
    _backgroundGradientView = [[GradientView alloc]
        initWithTopColor:[UIColor colorNamed:kSecondaryBackgroundColor]
             bottomColor:[UIColor colorNamed:kPrimaryBackgroundColor]];
    _backgroundGradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_backgroundGradientView];
    AddSameConstraints(_backgroundGradientView, self.view);
  [self updateModularHomeBackgroundColorForUserInterfaceStyle:
            self.traitCollection.userInterfaceStyle];
  self.view.backgroundColor = [UIColor colorNamed:@"ntp_background_color"];

  [self registerNotifications];

  [self layoutContentInParentCollectionView];

  self.identityDiscButton = [self.headerViewController identityDiscButton];
  DCHECK(self.identityDiscButton);

  self.viewDidFinishLoading = YES;

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
      UITraitUserInterfaceStyle.self, UITraitHorizontalSizeClass.self,
      UITraitPreferredContentSizeCategory.self
    ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  _appearing = YES;

  self.headerViewController.view.alpha = 1;
  self.headerViewController.showing = YES;

  [self updateNTPLayout];

  // Scroll to the top before coming into view to minimize sudden visual jerking
  // for startup instances showing the NTP.
  if (!self.viewDidAppear && !self.hasSavedOffsetFromPreviousScrollState) {
    [self setContentOffsetToTop];
  }

  if (self.focusAccessibilityOmniboxWhenViewAppears && !self.omniboxFocused) {
    [self.headerViewController focusAccessibilityOnOmnibox];
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  // `-feedLayoutDidEndUpdates` handles the need to either scroll to the top of
  // go back to a previous scroll state when the feed is enabled. This handles
  // the instance when the feed is not enabled.
  // `-viewWillAppear:` is not the suitable place for this as long as the user
  // can open a new tab while an NTP is currently visible. `-viewWillAppear:` is
  // called before the offset can be saved, so `-setContentOffsetToTop` will
  // reset any scrolled position.
  // It is NOT safe to reset `hasSavedOffsetFromPreviousScrollState` to NO here
  // because -updateHeightAboveFeedAndScrollToTopIfNeeded calls from async
  // updates to the Content Suggestions (i.e. MVT, Doodle) can happen after
  // this.
  if (!self.feedVisible) {
    if (self.hasSavedOffsetFromPreviousScrollState) {
      [self setContentOffset:self.savedScrollOffset];
    } else {
      [self setContentOffsetToTop];
    }
  }

  // Updates omnibox to ensure that the dimensions are correct when navigating
  // back to the NTP.
  [self updateFakeOmniboxForScrollPosition];

  if (self.feedVisible) {
    [self updateFeedInsetsForMinimumHeight];
  } else {
    [self setMinimumHeight];
  }

  [self.helpHandler
      presentInProductHelpWithType:InProductHelpType::kDiscoverFeedMenu];

  if (IsHomeCustomizationEnabled() && !IsFirstRunRecent(base::Days(3))) {
    [self.helpHandler
        presentInProductHelpWithType:InProductHelpType::kHomeCustomizationMenu];
  }

  // Scrolls NTP into feed initially if `shouldScrollIntoFeed`.
  if (self.shouldScrollIntoFeed) {
    [self scrollIntoFeed];
    self.shouldScrollIntoFeed = NO;
  }

  [self updateFeedSigninPromoIsVisible];

  // Since this VC is shared across web states, the stickiness might have
  // changed in another tab. This ensures that the sticky elements are correct
  // whenever an NTP reappears.
  [self handleStickyElementsForScrollPosition:[self scrollPosition] force:YES];

  if (self.shouldFocusFakebox) {
    self.shouldFocusFakebox = NO;
    __weak __typeof(self) weakSelf = self;
    // Since a focus was requested before the view appeared, the shift up to
    // focus should be performed without animation so that the NTP appears and
    // is immediately ready to focus the omnibox. The actual focus animation
    // will still happen.
    [UIView performWithoutAnimation:^{
      [weakSelf shiftTilesUpToFocusOmnibox];
    }];
  }

  self.viewDidAppear = YES;
  _appearing = NO;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  self.headerViewController.showing = NO;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateModuleWidth];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak NewTabPageViewController* weakSelf = self;

  CGFloat yOffsetBeforeRotation = [self scrollPosition];
  CGFloat heightAboveFeedBeforeRotation = [self heightAboveFeed];

  void (^alongsideBlock)(id<UIViewControllerTransitionCoordinatorContext>) = ^(
      id<UIViewControllerTransitionCoordinatorContext> context) {
    [self updateModuleWidth];
    [weakSelf handleStickyElementsForScrollPosition:[weakSelf scrollPosition]
                                              force:YES];

    CGFloat heightAboveFeedDifference =
        [weakSelf heightAboveFeed] - heightAboveFeedBeforeRotation;

    // Rotating the device can change the content suggestions height. This
    // ensures that it is adjusted if necessary.
    if (yOffsetBeforeRotation < 0) {
      weakSelf.collectionView.contentOffset =
          CGPointMake(0, yOffsetBeforeRotation - heightAboveFeedDifference);
      [weakSelf updateNTPLayout];
    }
    [weakSelf.view setNeedsLayout];
    [weakSelf.view layoutIfNeeded];

    // Pinned offset is different based on the orientation, so we reevaluate the
    // minimum scroll position upon device rotation.
    CGFloat pinnedOffsetY = [weakSelf pinnedOffsetY];
    if (weakSelf.omniboxFocused && [weakSelf scrollPosition] < pinnedOffsetY) {
      weakSelf.collectionView.contentOffset = CGPointMake(0, pinnedOffsetY);
    }
    if (!weakSelf.feedVisible) {
      [weakSelf setMinimumHeight];
    }
  };
  [coordinator
      animateAlongsideTransition:alongsideBlock
                      completion:^(
                          id<UIViewControllerTransitionCoordinatorContext>) {
                        [self updateNTPLayout];
                        if (self.feedVisible) {
                          [self updateFeedInsetsForMinimumHeight];
                        }
                        [self updateFeedContainerHeight];
                      }];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

#pragma mark - Public

- (void)focusOmnibox {
  // Do nothing if the omnibox is already focused or is in the middle of a
  // focus. This prevents `collectionShiftingOffset` from being reset to close
  // to 0, which would result in the defocus animation not returning to the top
  // of the NTP if that was the original position.
  // This is relevant beacuse the omnibox logic signals the NTP to focus the
  // omnibox when it becomes the keyboard first responder, but that happens
  // during the NTP focus animation, which results in -focusOmnibox being called
  // twice.
  if (self.omniboxFocused || self.isAnimatingOmniboxFocus) {
    return;
  }

  // If the feed is meant to be visible and its contents have not loaded yet,
  // then any omnibox focus animations (i.e. opening app from search widget
  // action) needs to wait until it is ready. viewDidAppear: currently serves as
  // this proxy as there is no specific signal given from the feed that its
  // contents have loaded.
  if (self.feedVisible && _appearing) {
    self.shouldFocusFakebox = YES;
  } else {
    [self shiftTilesUpToFocusOmnibox];
  }
}

- (void)layoutContentInParentCollectionView {
  DCHECK(self.feedWrapperViewController);

  // Ensure the view is loaded so we can set the accessibility identifier.
  [self.feedWrapperViewController loadViewIfNeeded];
  self.collectionView.accessibilityIdentifier = kNTPCollectionViewIdentifier;

  if (self.feedVisible) {
    _feedContainer = [[UIView alloc] initWithFrame:CGRectZero];
    _feedContainer.userInteractionEnabled = YES;
    _feedContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _feedContainer.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    // Add corner radius to the top border.
    _feedContainer.clipsToBounds = YES;
    _feedContainer.layer.cornerRadius = kHomeModuleContainerCornerRadius;
    _feedContainer.layer.maskedCorners =
        kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
    _feedContainer.layer.masksToBounds = YES;
    _feedContainer.layer.zPosition = -CGFLOAT_MAX;
    [self.collectionView insertSubview:_feedContainer atIndex:0];
  }

  // Configures the feed and wrapper in the view hierarchy.
  UIView* feedView = self.feedWrapperViewController.view;
  [self.feedWrapperViewController willMoveToParentViewController:self];
  [self addChildViewController:self.feedWrapperViewController];
  [self.view addSubview:feedView];
  [self.feedWrapperViewController didMoveToParentViewController:self];
  feedView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(feedView, self.view);

  // Configures the content suggestions in the view hierarchy.
  // TODO(crbug.com/40799579): Remove this when issue is fixed.
  if (self.contentSuggestionsViewController.parentViewController) {
    [self.contentSuggestionsViewController willMoveToParentViewController:nil];
    [self.contentSuggestionsViewController.view removeFromSuperview];
    [self.contentSuggestionsViewController removeFromParentViewController];
    [self.feedMetricsRecorder
        recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                     kContentSuggestionsReset];
  }

  // Adds the feed top section to the view hierarchy if it exists.
  if (self.feedTopSectionViewController) {
    [self addViewControllerAboveFeed:self.feedTopSectionViewController];
  }

  // Configures the feed header in the view hierarchy if it is visible. Add it
  // in the order that guarantees it is behind `headerViewController` and in
  // front of all other views.
  if (self.feedHeaderViewController) {
    [self addViewControllerAboveFeed:self.feedHeaderViewController];
  }

  if (!IsHomeCustomizationEnabled() || self.magicStackVisible) {
    [self addViewControllerAboveFeed:self.magicStackCollectionView];
  }

  if (!ShouldPutMostVisitedSitesInMagicStack() &&
      (!IsHomeCustomizationEnabled() || self.mostVisitedVisible)) {
    [self addViewControllerAboveFeed:self.contentSuggestionsViewController];
  }

  [self addViewControllerAboveFeed:self.headerViewController];

  DCHECK(
      [self.headerViewController.view isDescendantOfView:self.containerView]);
  self.headerViewController.view.translatesAutoresizingMaskIntoConstraints = NO;

  // The view controllers have to be added in reverse order, so the array is
  // then reversed to reflect the visible order.
  self.viewControllersAboveFeed =
      [[[self.viewControllersAboveFeed reverseObjectEnumerator] allObjects]
          mutableCopy];

  // TODO(crbug.com/40165977): The contentCollectionView width might be
  // narrower than the ContentSuggestions view. This causes elements to be
  // hidden, so we set clipsToBounds to ensure that they remain visible. The
  // collection view changes, so we must set this property each time it does.
  self.collectionView.clipsToBounds = NO;

  [self.overscrollActionsController invalidate];

  // Only re-configure `overscrollActionsController`.
  if (self.overscrollActionsController) {
    [self configureOverscrollActionsController];
  }

  // Update NTP collection view constraints to ensure the layout adapts to
  // changes in feed visibility.
  [self applyCollectionViewConstraints];

  // Force relayout so that the views added in this method are rendered ASAP,
  // ensuring it is showing in the new tab animation.
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];

  // If the feed is not visible, we control the delegate ourself (since it is
  // otherwise controlled by the feed service).
  if (!self.feedVisible) {
    self.feedWrapperViewController.contentCollectionView.delegate = self;
    [self setMinimumHeight];
  }

  [self updateAccessibilityElements];
}

- (void)willUpdateSnapshot {
  [self.overscrollActionsController clear];
}

- (BOOL)isNTPScrolledToTop {
  return [self scrollPosition] <= -[self heightAboveFeed];
}

- (void)updateNTPLayout {
  [self updateFeedInsetsForContentAbove];
  if (self.feedVisible) {
    [self updateFeedInsetsForMinimumHeight];
  }

  // Reload data to ensure the Most Visited tiles and fake omnibox are correctly
  // positioned, in particular during a rotation while a ViewController is
  // presented in front of the NTP.
  [self updateFakeOmniboxOnNewWidth:self.collectionView.bounds.size.width];
  // Ensure initial fake omnibox layout.
  [self updateFakeOmniboxForScrollPosition];
}

- (void)updateHeightAboveFeed {
  if (self.viewDidFinishLoading) {
    CGFloat oldHeightAboveFeed = self.collectionView.contentInset.top;
    CGFloat oldOffset = self.collectionView.contentOffset.y;
    [self updateFeedInsetsForContentAbove];
    CGFloat newHeightAboveFeed = self.collectionView.contentInset.top;
    CGFloat change = newHeightAboveFeed - oldHeightAboveFeed;
    // Offset the change by subtracting it from the content offset, in order to
    // visually keep the same scroll position, but don't allow an offset that
    // is lower than the top.
    [self setContentOffset:MAX(oldOffset - change, -newHeightAboveFeed)];
  }
}

- (void)resetViewHierarchy {
  if (_feedContainer) {
    [_feedContainer removeFromSuperview];
    _feedContainer = nil;
  }

  [self removeFromViewHierarchy:self.feedWrapperViewController];
  [self removeFromViewHierarchy:self.magicStackCollectionView];
  if (!ShouldPutMostVisitedSitesInMagicStack()) {
    [self removeFromViewHierarchy:self.contentSuggestionsViewController];
  }

  for (UIViewController* viewController in self.viewControllersAboveFeed) {
    [self removeFromViewHierarchy:viewController];
  }
  [self.viewControllersAboveFeed removeAllObjects];
}

- (void)resetStateUponReload {
  self.hasSavedOffsetFromPreviousScrollState = NO;
}

- (void)setContentOffsetToTop {
  // There are many instances during NTP startup where the NTP layout is reset
  // (e.g. calling -updateNTPLayout), which involves resetting the scroll
  // offset. Some come from mutliple layout calls from the BVC, some come from
  // an ambifuous source (likely the Feed). Particularly, the mediator's
  // -setContentOffsetForWebState: call happens late in the cycle, which can
  // clash with an already focused omnibox state. That call to reset the content
  // offset to the top is important since the MVTiles and Google doodle are aync
  // fetched/displayed, thus needed a reset. However, in the instance where the
  // omnibox is focused, it is more important to keep that focused state and not
  // show a "double" omibox state.
  // TODO(crbug.com/40241297): Replace the -setContentOffsetForWebState: call
  // with calls directly from all async updates to the NTP.
  if (self.omniboxFocused) {
    return;
  }
  [self setContentOffset:-[self heightAboveFeed]];
  // TODO(crbug.com/40252945): Constraint updating should not be necessary since
  // scrollViewDidScroll: calls this if needed.
  [self setInitialFakeOmniboxConstraints];
  // Reset here since none of the view lifecycle callbacks (e.g.
  // viewDidDisappear) can be reliably used (it seems) (i.e. switching between
  // NTPs where there is saved scroll state in the destination tab). If the
  // content offset is being set to the top, it is safe to assume this can be
  // set to NO. Being called before setSavedContentOffset: is no problem since
  // then it will be subsequently overriden to YES.
  self.hasSavedOffsetFromPreviousScrollState = NO;
}

- (CGFloat)heightAboveFeed {
  CGFloat heightAboveFeed = 0;
  for (UIViewController* viewController in self.viewControllersAboveFeed) {
    heightAboveFeed += viewController.view.frame.size.height;

    // If the current view controller represents a module, account for the
    // vertical spacing between modules.
    if (IsHomeCustomizationEnabled() &&
        (viewController == self.magicStackCollectionView ||
         viewController == self.contentSuggestionsViewController ||
         viewController == self.feedHeaderViewController)) {
      heightAboveFeed += kSpaceBetweenModules;
    }
  }
  if (!IsHomeCustomizationEnabled()) {
    heightAboveFeed += kBottomMagicStackPadding;
    if (!self.contentSuggestionsViewController) {
      heightAboveFeed += content_suggestions::HeaderBottomPadding();
    }
  }
  return heightAboveFeed;
}

- (void)setContentOffsetToTopOfFeedOrLess:(CGFloat)contentOffset {
  if (contentOffset < [self offsetWhenScrolledIntoFeed]) {
    [self setContentOffset:contentOffset];
  } else {
    [self scrollIntoFeed];
  }
}

- (void)updateFeedInsetsForMinimumHeight {
  DCHECK(self.feedVisible);
  CGFloat minimumNTPHeight = self.collectionView.bounds.size.height;
  minimumNTPHeight -= [self feedHeaderHeight];
  if ([self shouldPinFakeOmnibox]) {
    minimumNTPHeight -= ([self.headerViewController headerHeight] +
                         ntp_header::kScrolledToTopOmniboxBottomMargin);
  }

  if (self.collectionView.contentSize.height > minimumNTPHeight) {
    self.collectionView.contentInset =
        UIEdgeInsetsMake(self.collectionView.contentInset.top, 0, 0, 0);
  } else {
    CGFloat bottomInset =
        minimumNTPHeight - self.collectionView.contentSize.height;
    self.collectionView.contentInset = UIEdgeInsetsMake(
        self.collectionView.contentInset.top, 0, bottomInset, 0);
  }
}

- (void)updateScrollPositionForFeedTopSectionClosed {
  if (self.isFakeboxPinned) {
    [self setContentOffset:[self scrollPosition] + [self feedTopSectionHeight]];
  }
}

- (void)feedLayoutDidEndUpdatesWithType:(FeedLayoutUpdateType)type {
  if (_feedContainer) {
    // Feed content gets added to the top of the subview array, so after content
    // loads the feed container needs to be sent to the back so that it isn't
    // in front of the new content and doesn't intercept taps / interactions
    // that are meant for the feed content.
    [self.collectionView sendSubviewToBack:_feedContainer];
  }
  [self updateFeedInsetsForMinimumHeight];
  // Updating insets can influence contentOffset, so update saved scroll state
  // after it. This handles what the starting offset be with the feed enabled,
  // `-viewWillAppear:` handles when the feed is not enabled.
  // It is NOT safe to reset `hasSavedOffsetFromPreviousScrollState` to NO here
  // because -updateHeightAboveFeedAndScrollToTopIfNeeded calls from async
  // updates to the Content Suggestions (i.e. MVT, Doodle) can happen after
  // this.
  if (self.hasSavedOffsetFromPreviousScrollState) {
    [self setContentOffset:self.savedScrollOffset];
  }

  [self updateFeedContainerHeight];
}

- (void)invalidate {
  _viewControllersAboveFeed = nil;
  [self.overscrollActionsController invalidate];
  self.overscrollActionsController = nil;
  self.NTPContentDelegate = nil;
  self.contentSuggestionsViewController = nil;
  self.feedMetricsRecorder = nil;
  self.feedHeaderViewController = nil;
  self.feedWrapperViewController = nil;
  self.mutator = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (UILayoutGuide*)moduleLayoutGuide {
  if (!_moduleLayoutGuide) {
    _moduleLayoutGuide = [[UILayoutGuide alloc] init];
    UIView* view = self.view;
    [view addLayoutGuide:_moduleLayoutGuide];
    [NSLayoutConstraint activateConstraints:@[
      [_moduleLayoutGuide.centerXAnchor
          constraintEqualToAnchor:view.centerXAnchor],
      [_moduleLayoutGuide.topAnchor constraintEqualToAnchor:view.topAnchor],
      [_moduleLayoutGuide.bottomAnchor
          constraintEqualToAnchor:view.bottomAnchor],
    ]];
  }
  return _moduleLayoutGuide;
}

#pragma mark - NewTabPageConsumer

- (void)restoreScrollPosition:(CGFloat)scrollPosition {
  [self.view layoutIfNeeded];
  if (scrollPosition > -[self heightAboveFeed]) {
    [self setSavedContentOffset:scrollPosition];
  } else {
    // Remove this if NTPs are ever scoped back to the WebState.
    [self setContentOffsetToTop];

    // Refresh NTP content if there is is no saved scrolled state or when a new
    // NTP is opened. Since the same NTP is being shared across tabs, this
    // ensures that new content is being fetched.
    [self.NTPContentDelegate refreshNTPContent];
  }
}

- (void)restoreScrollPositionToTopOfFeed {
  [self setSavedContentOffset:[self offsetWhenScrolledIntoFeed]];
}

- (CGFloat)scrollPosition {
  return self.collectionView.contentOffset.y;
}

- (CGFloat)pinnedOffsetY {
  return [self.headerViewController pinnedOffsetY] - [self heightAboveFeed];
}

- (void)omniboxDidBecomeFirstResponder {
  self.omniboxFocused = YES;
  self.headerViewController.view.alpha = 0.01;
}

- (void)omniboxWillResignFirstResponder {
  self.omniboxFocused = NO;
  if ([self isFakeboxPinned]) {
    // Return early to allow the omnibox defocus animation to show.
    return;
  }

  [self omniboxDidResignFirstResponder];
}

- (void)omniboxDidResignFirstResponder {
  if (![self.headerViewController isShowing] && !self.scrolledToMinimumHeight) {
    return;
  }

  // Do not trigger defocus animation if the user is already navigating away
  // from the NTP.
  if (self.NTPVisible) {
    [self.headerViewController omniboxDidResignFirstResponder];
    [self shiftTilesDownForOmniboxDefocus];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // If `feedWrapperViewController` is nil, then the NTP is either being created
  // or updated and is not ready to handle scroll events. Doing so could cause
  // unexpected behavior, such as breaking the layout or causing crashes.
  if (!self.feedWrapperViewController) {
    return;
  }
  // Scroll events might still be queued for a previous scroll view which was
  // now replaced. In these cases, ignore the scroll event.
  if (scrollView != self.collectionView) {
    return;
  }
  [self.overscrollActionsController scrollViewDidScroll:scrollView];
  [self updateFakeOmniboxForScrollPosition];

  [self updateScrolledToMinimumHeight];

  CGFloat scrollPosition = scrollView.contentOffset.y;
  [self handleStickyElementsForScrollPosition:scrollPosition force:NO];

  if (self.viewDidAppear) {
    [self updateFeedSigninPromoIsVisible];
  }

  [self updateScrollPositionToSave];

  // The feed model callbacks don't always reliably tell us that the content has
  // paginated, so check if the container should be extended.
  if (self.collectionView.contentSize.height >
      self.feedContainerHeightConstraint.constant) {
    [self updateFeedContainerHeight];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  // Scroll events might still be queued for a previous scroll view which was
  // now replaced. In these cases, ignore the scroll event.
  if (scrollView != self.collectionView) {
    return;
  }

  if (!self.overscrollActionsController) {
    [self configureOverscrollActionsController];
  }

  // User has interacted with the surface, so it is safe to assume that a saved
  // scroll position can now be overriden.
  self.hasSavedOffsetFromPreviousScrollState = NO;
  [self.overscrollActionsController scrollViewWillBeginDragging:scrollView];
  self.scrollStartPosition = scrollView.contentOffset.y;
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  // Scroll events might still be queued for a previous scroll view which was
  // now replaced. In these cases, ignore the scroll event.
  if (scrollView != self.collectionView) {
    return;
  }
  [self.overscrollActionsController
      scrollViewWillEndDragging:scrollView
                   withVelocity:velocity
            targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  // Scroll events might still be queued for a previous scroll view which was
  // now replaced. In these cases, ignore the scroll event.
  if (scrollView != self.collectionView) {
    return;
  }
  [self.overscrollActionsController scrollViewDidEndDragging:scrollView
                                              willDecelerate:decelerate];
  if (self.feedVisible) {
    [self.feedMetricsRecorder recordFeedScrolled:scrollView.contentOffset.y -
                                                 self.scrollStartPosition];
  }
}

- (void)scrollViewDidScrollToTop:(UIScrollView*)scrollView {
  // TODO(crbug.com/40710989): Handle scrolling.
}

- (void)scrollViewWillBeginDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/40710989): Handle scrolling.
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  // TODO(crbug.com/40710989): Handle scrolling.
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  // TODO(crbug.com/40710989): Handle scrolling.
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  // Scroll events might still be queued for a previous scroll view which was
  // now replaced. In these cases, ignore the scroll event.
  if (scrollView != self.collectionView) {
    return YES;
  }
  // User has tapped the status bar to scroll to the top.
  // Prevent scrolling back to pre-focus state, making sure we don't have
  // two scrolling animations running at the same time.
  self.collectionShiftingOffset = 0;
  // Reset here since none of the view lifecycle callbacks are called reliably
  // to be able to be used (it seems) (i.e. switching between NTPs where there
  // is saved scroll state in the destination tab). If the content offset is
  // being set to the top, it is safe to assume this can be set to NO. Being
  // called before setSavedContentOffset: is no problem since then it will be
  // subsequently overriden to YES.
  self.hasSavedOffsetFromPreviousScrollState = NO;
  // Unfocus omnibox without scrolling back.
  [self unfocusOmnibox];
  return YES;
}

#pragma mark - UIGestureRecognizerDelegate

// TODO(crbug.com/40165977): Remove once the Feed header properly supports
// ContentSuggestions.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  // Ignore all touches inside the Feed CollectionView, which includes
  // ContentSuggestions.
  UIView* viewToIgnoreTouches = self.collectionView;
  CGRect ignoreBoundsInView =
      [viewToIgnoreTouches convertRect:viewToIgnoreTouches.bounds
                                toView:self.view];
  return !(CGRectContainsPoint(ignoreBoundsInView,
                               [touch locationInView:self.view]));
}

#pragma mark - Scrolling Animations

- (void)shiftTilesUpToFocusOmnibox {
  // Add gesture recognizer to collection view when the omnibox is focused.
  [self.view addGestureRecognizer:self.tapGestureRecognizer];

  // Stop any existing focus/defocus animation.
  if (self.animator.running) {
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionStart];
    self.animator = nil;
  }

  if (self.collectionView.decelerating) {
    // Stop the scrolling if the scroll view is decelerating to prevent the
    // focus to be immediately lost.
    [self.collectionView setContentOffset:self.collectionView.contentOffset
                                 animated:NO];
  }

  self.shouldAnimateHeader = YES;
  CGFloat pinnedOffsetBeforeAnimation = [self pinnedOffsetY];
  if (CGSizeEqualToSize(self.collectionView.contentSize, CGSizeZero)) {
    [self.collectionView layoutIfNeeded];
  }

  if (!self.scrolledToMinimumHeight) {
    // Save the scroll position prior to the animation to allow the user to
    // return to it on defocus.
    self.collectionShiftingOffset =
        MAX(-[self heightAboveFeed],
            AlignValueToPixel([self.headerViewController pinnedOffsetY] -
                              [self adjustedOffset].y));
  }

  // If the fake omnibox is already at the final position, just focus it and
  // return early.
  if ([self shouldSkipScrollToFocusOmnibox]) {
    self.shouldAnimateHeader = NO;
    if (!self.scrolledToMinimumHeight) {
      // Scroll up to pinned position if it is not pinned already, but don't
      // wait for it to finish to focus the omnibox.
      __weak __typeof(self) weakSelf = self;
      [UIView animateWithDuration:kMaterialDuration6
          animations:^{
            weakSelf.collectionView.contentOffset =
                CGPoint(0, pinnedOffsetBeforeAnimation);
            [weakSelf resetFakeOmniboxConstraints];
          }];
    }
    [self.headerViewController
        completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
            UIViewAnimatingPositionEnd];
    [self.NTPContentDelegate focusOmnibox];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock shiftOmniboxToTop = ^{
    __typeof(weakSelf) strongSelf = weakSelf;
    // Changing the contentOffset of the collection results in a
    // scroll and a change in the constraints of the header.
    strongSelf.collectionView.contentOffset =
        CGPointMake(0, [strongSelf pinnedOffsetY]);
    // Layout the header for the constraints to be animated.
    [strongSelf.headerViewController layoutHeader];
  };

  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kShiftTilesUpAnimationDuration
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              NewTabPageViewController* strongSelf = weakSelf;
              if (!strongSelf) {
                return;
              }

              if (strongSelf.collectionView.contentOffset.y <
                  [strongSelf pinnedOffsetY]) {
                self.disableScrollAnimation = YES;
                [strongSelf.headerViewController expandHeaderForFocus];
                shiftOmniboxToTop();
                [strongSelf.headerViewController
                    completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
                        UIViewAnimatingPositionEnd];
                [strongSelf.NTPContentDelegate focusOmnibox];
              }
            }];

  [self.animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    NewTabPageViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if (finalPosition == UIViewAnimatingPositionEnd) {
      // Content suggestion headers can be updated during the scroll, causing
      // `pinnedOffsetY` to be invalid. When this happens during the animation,
      // the tiles are not scrolled to the top causing the omnibox to be hidden
      // by the `PrimaryToolbarView`. In that state, the omnibox's popup and the
      // keyboard are still visible.
      // If the animation is not interrupted and `pinnedOffsetY` changed
      // during the animation, shift the omnibox to the top at the end of the
      // animation.
      if ([strongSelf pinnedOffsetY] != pinnedOffsetBeforeAnimation &&
          strongSelf.collectionView.contentOffset.y <
              [strongSelf pinnedOffsetY]) {
        shiftOmniboxToTop();
      }
      strongSelf.shouldAnimateHeader = NO;
    }

    strongSelf.scrolledToMinimumHeight = YES;
    strongSelf.disableScrollAnimation = NO;
    [strongSelf.headerViewController
        completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:finalPosition];
    strongSelf.isAnimatingOmniboxFocus = NO;
  }];

  self.animator.interruptible = YES;
  self.isAnimatingOmniboxFocus = YES;
  [self.animator startAnimation];
}

#pragma mark - Private

// Returns YES if scroll should be skipped when focusing the omnibox.
- (BOOL)shouldSkipScrollToFocusOmnibox {
  return self.scrolledToMinimumHeight || IsSplitToolbarMode(self);
}

// Returns the collection view containing all NTP content.
- (UICollectionView*)collectionView {
  return self.feedWrapperViewController.contentCollectionView;
}

// Returns the height of the fake omnibox to stick to the top of the NTP.
- (CGFloat)stickyOmniboxHeight {
  return content_suggestions::FakeToolbarHeight();
}

// Sets the feed collection contentOffset from the saved state to `offset` to
// set the initial scroll position.
- (void)setSavedContentOffset:(CGFloat)offset {
  self.hasSavedOffsetFromPreviousScrollState = YES;
  self.savedScrollOffset = offset;
  [self setContentOffset:offset];
}

// Configures overscroll actions controller.
- (void)configureOverscrollActionsController {
  // Ensure the feed's scroll view exists to prevent crashing the overscroll
  // controller.
  if (!self.collectionView) {
    return;
  }
  // Overscroll action does not work well with content offset, so set this
  // to never and internally offset the UI to account for safe area insets.
  self.collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;

  self.overscrollActionsController = [[OverscrollActionsController alloc]
      initWithScrollView:self.collectionView];
  [self.overscrollActionsController
      setStyle:OverscrollStyle::NTP_NON_INCOGNITO];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  [self updateOverscrollActionsState];
}

// Enables or disables overscroll actions.
- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

// Either signals to the omnibox to cancel its focused state or just update the
// NTP state for an unfocused state.
- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    [self.NTPContentDelegate cancelOmniboxEdit];
  } else {
    [self omniboxDidResignFirstResponder];
  }
}

// Shifts tiles down when defocusing the omnibox.
- (void)shiftTilesDownForOmniboxDefocus {
  if (self.shiftDownInProgress) {
    return;
  }
  self.shiftDownInProgress = YES;
  if (IsSplitToolbarMode(self)) {
    [self.NTPContentDelegate onFakeboxBlur];
  }

  [self.view removeGestureRecognizer:self.tapGestureRecognizer];

  self.shouldAnimateHeader = YES;

  if (self.animator.running) {
    [self.animator stopAnimation:NO];
    [self.animator finishAnimationAtPosition:UIViewAnimatingPositionStart];
    self.animator = nil;
  }

  if (self.collectionShiftingOffset == 0 || self.collectionView.dragging) {
    self.collectionShiftingOffset = 0;
    [self updateFakeOmniboxForScrollPosition];
    self.shiftDownInProgress = NO;
    return;
  }

  // Use a simple animation to scroll back into position.
  CGFloat yOffset = MAX([self pinnedOffsetY] - self.collectionShiftingOffset,
                        -[self heightAboveFeed]);
  self.headerViewController.view.alpha = 1;
  __weak __typeof(self) weakSelf = self;
  self.inhibitScrollPositionUpdates = YES;
  self.headerViewController.allowFontScaleAnimation = YES;
  [self updateFakeOmniboxForScrollPosition];
  [self.headerViewController layoutHeader];
  self.animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kMaterialDuration6
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
              weakSelf.collectionView.contentOffset = CGPoint(0, yOffset);
              [weakSelf.headerViewController layoutHeader];
            }];
  [self.animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    weakSelf.inhibitScrollPositionUpdates = NO;
    weakSelf.collectionShiftingOffset = 0;
    weakSelf.headerViewController.view.alpha = 1;
    weakSelf.collectionView.contentOffset = CGPoint(0, yOffset);
    weakSelf.scrolledToMinimumHeight = NO;
    weakSelf.headerViewController.allowFontScaleAnimation = NO;
    weakSelf.shiftDownInProgress = NO;
  }];
  self.animator.interruptible = YES;
  [self.animator startAnimation];
}

// Pins the fake omnibox to the top of the NTP.
- (void)pinFakeOmniboxToTop {
  self.isFakeboxPinned = YES;
  [self stickFakeOmniboxToTop];
}

// Resets the fake omnibox to its original position.
- (void)resetFakeOmniboxConstraints {
  self.isFakeboxPinned = NO;
  [self setInitialFakeOmniboxConstraints];
}

// Lets this view own the fake omnibox and sticks it to the top of the NTP.
- (void)stickFakeOmniboxToTop {
  // If `self.headerViewController` is nil after removing it from the view
  // hierarchy it means its no longer owned by anyone (e.g. The coordinator
  // might have been stopped.) and we shouldn't try to add it again.
  if (!self.headerViewController) {
    return;
  }

  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];

  self.headerTopAnchor = [self.headerViewController.view.bottomAnchor
      constraintEqualToAnchor:self.feedWrapperViewController.view
                                  .safeAreaLayoutGuide.topAnchor
                     constant:[self stickyOmniboxHeight]];
  // This issue fundamentally comes down to the topAnchor being set just once
  // and if it is set in landscape mode, it never is updated upon rotation.
  // And landscape is when it doesn't matter.
  self.fakeOmniboxConstraints = @[
    self.headerTopAnchor,
    [self.headerViewController.view.leadingAnchor
        constraintEqualToAnchor:self.feedWrapperViewController.view
                                    .leadingAnchor],
    [self.headerViewController.view.trailingAnchor
        constraintEqualToAnchor:self.feedWrapperViewController.view
                                    .trailingAnchor],
  ];
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Gives content suggestions collection view ownership of the fake omnibox for
// the width animation.
- (void)setInitialFakeOmniboxConstraints {
  [NSLayoutConstraint deactivateConstraints:self.fakeOmniboxConstraints];

  if (IsHomeCustomizationEnabled()) {
    // If all modules are disabled, the fake omnibox doesn't need additional
    // constraints.
    if ([self.viewControllersAboveFeed lastObject] ==
        self.headerViewController) {
      self.fakeOmniboxConstraints = @[];
    } else {
      // Otherwise, anchor the header to the module below it.
      NSInteger headerIndex = [self.viewControllersAboveFeed
          indexOfObject:self.headerViewController];
      UIView* viewBelowHeader =
          [self.viewControllersAboveFeed objectAtIndex:(headerIndex + 1)].view;
      self.fakeOmniboxConstraints = @[
        [viewBelowHeader.topAnchor
            constraintEqualToAnchor:self.headerViewController.view.bottomAnchor
                           constant:kSpaceBetweenModules],
      ];
    }
  } else {
    if (self.contentSuggestionsViewController) {
      self.fakeOmniboxConstraints = @[
        [self.contentSuggestionsViewController.view.topAnchor
            constraintEqualToAnchor:self.headerViewController.view
                                        .bottomAnchor],
      ];
    } else {
      // If `contentSuggestionsViewController` is nil, that means MVTs are in
      // the Magic Stack.
      self.fakeOmniboxConstraints = @[
        [self.magicStackCollectionView.view.topAnchor
            constraintEqualToAnchor:self.headerViewController.view.bottomAnchor
                           constant:content_suggestions::HeaderBottomPadding()],
      ];
    }
  }
  [NSLayoutConstraint activateConstraints:self.fakeOmniboxConstraints];
}

// Update the header for a new width size depending on if the change needs to be
// animated.
- (void)updateFakeOmniboxOnNewWidth:(CGFloat)width {
  if (self.shouldAnimateHeader) {
    // We check -superview here because in certain scenarios (such as when the
    // VC is rotated underneath another presented VC), in a
    // UICollectionViewController -viewSafeAreaInsetsDidChange the VC.view has
    // updated safeAreaInsets, but VC.collectionView does not until a layer
    // -viewDidLayoutSubviews.  Since self.collectionView and it's superview
    // should always have the same safeArea, this should be safe.
    UIEdgeInsets insets = self.collectionView.superview.safeAreaInsets;
    [self.headerViewController
        updateFakeOmniboxForOffset:[self adjustedOffset].y
                       screenWidth:width
                    safeAreaInsets:insets
            animateScrollAnimation:!self.disableScrollAnimation];
  } else {
    [self.headerViewController updateFakeOmniboxForWidth:width];
  }
}

// Update the header state for a change in scroll position. This could mean
// unfocusing the omnibox and/or updating its shape if `shouldAnimateHeader` is
// YES.
- (void)updateFakeOmniboxForScrollPosition {
  // Unfocus the omnibox when the scroll view is scrolled by the user (but not
  // when a scroll is triggered by layout/UIKit).
  if (self.omniboxFocused && !self.shouldAnimateHeader &&
      self.collectionView.dragging) {
    [self unfocusOmnibox];
  }

  if (self.shouldAnimateHeader) {
    UIEdgeInsets insets = self.collectionView.safeAreaInsets;
    [self.headerViewController
        updateFakeOmniboxForOffset:[self adjustedOffset].y
                       screenWidth:self.collectionView.frame.size.width
                    safeAreaInsets:insets
            animateScrollAnimation:!self.disableScrollAnimation];
  }
}

// Sets an top inset to the feed collection view to fit the content above it.
- (void)updateFeedInsetsForContentAbove {
  // Setting the contentInset will cause a scroll, which will call
  // scrollViewDidScroll which calls updateScrolledToMinimumHeight. So no need
  // to call here.
  self.collectionView.contentInset = UIEdgeInsetsMake(
      [self heightAboveFeed], 0, self.collectionView.contentInset.bottom, 0);
}

// Checks whether the feed top section is visible and updates the
// `NTPContentDelegate`.
// TODO(crbug.com/40843602): This function currently checks the visibility of
// the entire feed top section, but it should only check the visibility of the
// promo within it.
- (void)updateFeedSigninPromoIsVisible {
  if (!self.feedTopSectionViewController) {
    return;
  }

  // The y-position where NTP content starts being visible.
  CGFloat visibleContentStartingPoint =
      [self scrollPosition] + self.view.frame.size.height;

  // Signin promo is logged as visible when at least the top 2/3 or bottom 1/3
  // of it can be seen. This is not logged if the user focuses the omnibox since
  // the suggestion sheet covers the NTP content.
  BOOL isFeedSigninPromoVisible =
      (visibleContentStartingPoint > -([self feedTopSectionHeight] * 2) / 3 &&
       ([self scrollPosition] <
        -([self stickyOmniboxHeight] + [self feedTopSectionHeight] / 3))) &&
      !self.omniboxFocused;

  [self.NTPContentDelegate
      signinPromoHasChangedVisibility:isFeedSigninPromoVisible];
}

// TODO(crbug.com/40251609): Remove once the Feed header properly supports
// ContentSuggestions.
- (void)handleSingleTapInView:(UITapGestureRecognizer*)recognizer {
  CGPoint location = [recognizer locationInView:[recognizer.view superview]];
  CGRect discBoundsInView =
      [self.identityDiscButton convertRect:self.identityDiscButton.bounds
                                    toView:self.view];
  if (CGRectContainsPoint(discBoundsInView, location)) {
    [self.identityDiscButton
        sendActionsForControlEvents:UIControlEventTouchUpInside];
  } else {
    [self unfocusOmnibox];
  }

  if (IsHomeCustomizationEnabled()) {
    CGRect customizationMenuBounds =
        [[self.headerViewController customizationMenuButton]
            convertRect:[self.headerViewController customizationMenuButton]
                            .bounds
                 toView:self.view];

    if (CGRectContainsPoint(customizationMenuBounds, location)) {
      [[self.headerViewController customizationMenuButton]
          sendActionsForControlEvents:UIControlEventTouchUpInside];
    }
  }
}

// Handles the pinning of the sticky elements to the top of the NTP. This
// includes the fake omnibox and if Web Channels is enabled, the feed header. If
// `force` is YES, the sticky elements will always be set based on the scroll
// position. If `force` is NO, the sticky elements will only based on
// `isScrolledIntoFeed` to prevent pinning them multiple times.
- (void)handleStickyElementsForScrollPosition:(CGFloat)scrollPosition
                                        force:(BOOL)force {
  // Handles the sticky omnibox. Does not stick for iPads.
  CGFloat offsetToStickOmnibox = [self offsetToStickOmnibox];
  if ([self shouldPinFakeOmnibox]) {
    if (scrollPosition >= offsetToStickOmnibox &&
        (!self.isFakeboxPinned || force)) {
      [self pinFakeOmniboxToTop];
    } else if (scrollPosition < offsetToStickOmnibox &&
               (self.isFakeboxPinned || force)) {
      [self resetFakeOmniboxConstraints];
    }
  } else if (self.isFakeboxPinned) {
    [self resetFakeOmniboxConstraints];
  }
}

// Registers notifications for certain actions on the NTP.
- (void)registerNotifications {
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(deviceOrientationDidChange)
                 name:UIDeviceOrientationDidChangeNotification
               object:nil];
}

// Handles device rotation.
- (void)deviceOrientationDidChange {
  if (self.viewDidAppear && self.feedVisible) {
    [self.feedMetricsRecorder
        recordDeviceOrientationChanged:[[UIDevice currentDevice] orientation]];
  }
}

// The Discover Feed component seems to add an unwanted width constraint
// (<= 360) in some circumstances, including multiwindow on iPad. This
// cleans up the width constraints so proper constraints can be added.
- (void)cleanUpCollectionViewConstraints {
  auto* collectionWidthAnchor = self.collectionView.widthAnchor;
  auto predicate =
      [NSPredicate predicateWithBlock:^BOOL(NSLayoutConstraint* constraint,
                                            NSDictionary* bindings) {
        return constraint.firstAnchor == collectionWidthAnchor;
      }];
  auto collectionWidthConstraints =
      [self.collectionView.constraints filteredArrayUsingPredicate:predicate];
  [NSLayoutConstraint deactivateConstraints:collectionWidthConstraints];
}

// Applies constraints to the NTP collection view, along with the constraints
// for the content suggestions within it.
- (void)applyCollectionViewConstraints {
  UIView* contentSuggestionsView = self.contentSuggestionsViewController.view;
  contentSuggestionsView.translatesAutoresizingMaskIntoConstraints = NO;
  self.magicStackCollectionView.view.translatesAutoresizingMaskIntoConstraints =
      NO;

  if (self.feedHeaderViewController) {
    [self cleanUpCollectionViewConstraints];

    // When the feed is turned off, do not constrain the width of the empty
    // collection view, in order to allow vertical scrolling gestures to
    // happen on the side margins. The width of the feed header is controlled
    // by the collectionView's contentLayoutGuide.
    if (self.feedWrapperViewController.feedViewController) {
      [self.collectionView.widthAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.widthAnchor]
          .active = YES;
    }

    [NSLayoutConstraint activateConstraints:@[
      // Apply parent collection view constraints.
      [self.collectionView.centerXAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.centerXAnchor],

      // Apply feed header constraints.
      [self.feedHeaderViewController.view.centerXAnchor
          constraintEqualToAnchor:self.collectionView.frameLayoutGuide
                                      .centerXAnchor],
      [self.feedHeaderViewController.view.widthAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.widthAnchor],
    ]];
    if (!IsHomeCustomizationEnabled()) {
      // If Feed top section is enabled, the header bottom anchor should be set
      // to its top anchor instead of the feed collection's top anchor.
      UIView* bottomView = self.collectionView;
      if (self.feedTopSectionViewController) {
        bottomView = self.feedTopSectionViewController.view;
      }
      [NSLayoutConstraint activateConstraints:@[
        [self.feedHeaderViewController.view.topAnchor
            constraintEqualToAnchor:self.magicStackCollectionView.view
                                        .bottomAnchor
                           constant:kBottomMagicStackPadding],
        [bottomView.topAnchor
            constraintEqualToAnchor:self.feedHeaderViewController.view
                                        .bottomAnchor],
      ]];
    }
    if (self.feedTopSectionViewController) {
      [NSLayoutConstraint activateConstraints:@[
        [self.feedTopSectionViewController.view.centerXAnchor
            constraintEqualToAnchor:self.collectionView.centerXAnchor],
        [self.feedTopSectionViewController.view.widthAnchor
            constraintEqualToAnchor:self.collectionView.widthAnchor],
        [self.feedTopSectionViewController.view.topAnchor
            constraintEqualToAnchor:self.feedHeaderViewController.view
                                        .bottomAnchor],
        [self.collectionView.topAnchor
            constraintEqualToAnchor:self.feedTopSectionViewController.view
                                        .bottomAnchor],
      ]];
    }
  } else {
    if (!IsHomeCustomizationEnabled()) {
      [NSLayoutConstraint activateConstraints:@[
        [self.collectionView.topAnchor
            constraintEqualToAnchor:self.magicStackCollectionView.view
                                        .bottomAnchor],
      ]];
    }
  }
  if (IsHomeCustomizationEnabled()) {
    UIView* lastView = [self.viewControllersAboveFeed lastObject].view;
    [NSLayoutConstraint activateConstraints:@[
      [self.collectionView.topAnchor
          constraintEqualToAnchor:lastView.bottomAnchor],
    ]];
  }

  if (_feedContainer) {
    [NSLayoutConstraint activateConstraints:@[
      [_feedContainer.widthAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.widthAnchor],
      [_feedContainer.centerXAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.centerXAnchor],
      [_feedContainer.topAnchor
          constraintEqualToAnchor:self.feedHeaderViewController.view.topAnchor],
    ]];
    [self updateFeedContainerHeight];
  }

  [NSLayoutConstraint activateConstraints:@[
    [[self containerView].safeAreaLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.headerViewController.view.leadingAnchor],
    [[self containerView].safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.headerViewController.view.trailingAnchor],
  ]];
  if (self.contentSuggestionsViewController &&
      (!IsHomeCustomizationEnabled() || self.mostVisitedVisible)) {
    [NSLayoutConstraint activateConstraints:@[
      [self.contentSuggestionsViewController.view.leadingAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.leadingAnchor],
      [self.contentSuggestionsViewController.view.trailingAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.trailingAnchor],
    ]];
  }
  if (!IsHomeCustomizationEnabled() || self.magicStackVisible) {
    [NSLayoutConstraint activateConstraints:@[
      [self.magicStackCollectionView.view.leadingAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.leadingAnchor],
      [self.magicStackCollectionView.view.trailingAnchor
          constraintEqualToAnchor:self.moduleLayoutGuide.trailingAnchor],
    ]];
  }
  if (!ShouldPutMostVisitedSitesInMagicStack()) {
    if (!IsHomeCustomizationEnabled()) {
      [NSLayoutConstraint activateConstraints:@[
        [self.magicStackCollectionView.view.topAnchor
            constraintEqualToAnchor:self.contentSuggestionsViewController.view
                                        .bottomAnchor],
      ]];
    }
  }

  // Anchor each module except the one directly below the header, since it will
  // dynamically update its top anchor when the fake omnibox is pinned.
  if (IsHomeCustomizationEnabled() &&
      [self.viewControllersAboveFeed lastObject] != self.headerViewController) {
    // Start with the bottom module's index, which is either the feed header if
    // enabled, or the last object of the module array if not.
    NSUInteger startIndex =
        self.feedHeaderViewController
            ? [self.viewControllersAboveFeed
                  indexOfObject:self.feedHeaderViewController]
            : self.viewControllersAboveFeed.count - 1;

    // While the current module's index is not the view directly below the
    // header, anchor to the module above it.
    NSUInteger headerIndex =
        [self.viewControllersAboveFeed indexOfObject:self.headerViewController];
    for (NSUInteger index = startIndex; index > headerIndex + 1; --index) {
      UIView* view = self.viewControllersAboveFeed[index].view;
      UIView* viewAbove = self.viewControllersAboveFeed[index - 1].view;
      [NSLayoutConstraint activateConstraints:@[
        [view.topAnchor constraintEqualToAnchor:viewAbove.bottomAnchor
                                       constant:kSpaceBetweenModules],
      ]];
    }
  }

  [self setInitialFakeOmniboxConstraints];
}

// Sets minimum height for the NTP collection view, allowing it to scroll enough
// to focus the omnibox.
- (void)setMinimumHeight {
  CGFloat minimumNTPHeight = [self minimumNTPHeight] - [self heightAboveFeed];
  self.collectionView.contentSize =
      CGSizeMake(self.collectionView.frame.size.width, minimumNTPHeight);
}

// Sets the content offset to the top of the feed.
- (void)scrollIntoFeed {
  [self setContentOffset:[self offsetWhenScrolledIntoFeed]];
}

// Returns y-offset compensated for any content insets that might be set for the
// content above the feed.
- (CGPoint)adjustedOffset {
  CGPoint adjustedOffset = self.collectionView.contentOffset;
  adjustedOffset.y += [self heightAboveFeed];
  return adjustedOffset;
}

// Background gradient view will be used when in dark mode, the assigned
// background color to this view's otherwise.
- (void)updateModularHomeBackgroundColorForUserInterfaceStyle:
    (UIUserInterfaceStyle)style {
  _backgroundGradientView.hidden = style == UIUserInterfaceStyleLight;
}

// Signal to the ViewController that the height above the feed needs to be
// recalculated and thus also likely needs to be scrolled up to accommodate for
// the new height. Nothing may happen if the ViewController determines that the
// current scroll state should not change.
- (void)updateHeightAboveFeedAndScrollToTopIfNeeded {
  if (self.viewDidFinishLoading &&
      !self.hasSavedOffsetFromPreviousScrollState) {
    // Do not scroll to the top if there is a saved scroll state. Also,
    // `-setContentOffsetToTop` potentially updates constaints, and if
    // viewDidLoad has not finished, some views may not in the view hierarchy
    // yet.
    [self updateFeedInsetsForContentAbove];
    [self setContentOffsetToTop];
  }
}

// Updates the accessibilityElements used by VoiceOver / Switch Control to
// iterate through on-screen elements. The feed collectionView does not seem to
// include non-feed items in its `accessibilityElements` so they are added here.
- (void)updateAccessibilityElements {
  NSMutableArray* elements = [[NSMutableArray alloc] init];
  for (UIViewController* viewController in self.viewControllersAboveFeed) {
    [elements addObject:viewController.view];
  }
  [elements addObject:self.collectionView];
  self.view.accessibilityElements = elements;
}

// Calculate the scroll position that should be saved in the NTP state and
// update the mutator.
- (void)updateScrollPositionToSave {
  if (self.inhibitScrollPositionUpdates) {
    return;
  }
  CGFloat scrollPositionToSave = [self scrollPosition];
  scrollPositionToSave -= self.collectionShiftingOffset;
  self.mutator.scrollPositionToSave = scrollPositionToSave;
}

// Updates the feed container's height constraint.
- (void)updateFeedContainerHeight {
  if (!_feedContainer) {
    return;
  }
  self.feedContainerHeightConstraint.active = NO;
  // Container either takes the actual height of all feed components, or a
  // minimum value of `kFeedContainerMinimumHeight` if the content hasn't
  // loaded.
  CGFloat containerHeight =
      std::max((self.collectionView.contentSize.height +
                [self feedHeaderHeight] + [self feedTopSectionHeight]),
               kFeedContainerMinimumHeight) +
      kFeedContainerExtraHeight;
  self.feedContainerHeightConstraint =
      [_feedContainer.heightAnchor constraintEqualToConstant:containerHeight];
  self.feedContainerHeightConstraint.active = YES;
}

// Updates the width constraint of `moduleLayoutGuide`.
- (void)updateModuleWidth {
  CGFloat oldWidth = _moduleWidth.constant;
  CGFloat widthMultiplier = (100 - kHomeModuleMinimumPadding) / 100;
  CGFloat width = MIN(self.view.frame.size.width * widthMultiplier,
                      kDiscoverFeedContentMaxWidth);

  BOOL existingConstraintUpdated = NO;
  if (!_moduleWidth) {
    _moduleWidth =
        [self.moduleLayoutGuide.widthAnchor constraintEqualToConstant:width];
    _moduleWidth.active = YES;
  } else {
    _moduleWidth.constant = width;
    existingConstraintUpdated = YES;
  }
  if (width != oldWidth) {
    [self.view layoutIfNeeded];
    if (existingConstraintUpdated) {
      [self.magicStackCollectionView moduleWidthDidUpdate];
    }
  }
}

#pragma mark - Helpers

- (UIViewController*)contentSuggestionsViewController {
  return _contentSuggestionsViewController;
}

- (CGFloat)minimumNTPHeight {
  CGFloat collectionViewHeight = self.collectionView.bounds.size.height;
  CGFloat headerHeight = [self.headerViewController headerHeight];

  // The minimum height for the collection view content should be the height
  // of the header plus the height of the collection view minus the height of
  // the NTP bottom bar. This allows the Most Visited cells to be scrolled up
  // to the top of the screen. Also computes the total NTP scrolling height
  // for Discover infinite feed.
  CGFloat minimumHeight = collectionViewHeight + headerHeight;
  if (!IsRegularXRegularSizeClass(self.collectionView)) {
    minimumHeight -= self.collectionView.contentInset.bottom;
    if (IsSplitToolbarMode(self)) {
      minimumHeight -= [self stickyOmniboxHeight];
    } else {
      // Add in half of the margin between the fakebox and the rest of the
      // content suggestions, to ensure there is enough height to fully
      // finish the fakebox to omnibox transition.
      minimumHeight += content_suggestions::HeaderBottomPadding() / 2;
    }
  }

  return minimumHeight;
}

// Height of the feed header, returns 0 if it is not visible.
- (CGFloat)feedHeaderHeight {
  return self.feedHeaderViewController
             ? self.feedHeaderViewController.view.frame.size.height
             : 0;
}

// Height of the feed top section, returns 0 if not visible.
- (CGFloat)feedTopSectionHeight {
  return self.feedTopSectionViewController
             ? self.feedTopSectionViewController.view.frame.size.height
             : 0;
}

// The y-position content offset for when the user has completely scrolled into
// the Feed.
- (CGFloat)offsetWhenScrolledIntoFeed {
  CGFloat offset = -[self feedHeaderHeight];
  if ([self shouldPinFakeOmnibox]) {
    offset -= [self stickyOmniboxHeight];
  }
  return offset;
}

// The y-position content offset for when the fake omnibox
// should stick to the top of the NTP.
- (CGFloat)offsetToStickOmnibox {
  return AlignValueToPixel(-([self heightAboveFeed] -
                             [self.headerViewController headerHeight] +
                             [self stickyOmniboxHeight]));
}

// Whether the collection view has attained its minimum height.
// The fake omnibox never actually disappears; the NTP just scrolls enough so
// that it's hidden behind the real one when it's focused. When the NTP hasn't
// fully loaded yet, there isn't enough height to scroll it behind the real
// omnibox, so they would both show.
- (BOOL)collectionViewHasLoaded {
  return self.collectionView.contentSize.height > 0;
}

// TODO(crbug.com/40799579): Temporary fix to compensate for the view hierarchy
// sometimes breaking. Use DCHECKs to investigate what exactly is broken and
// find a fix.
- (void)verifyNTPViewHierarchy {
  // The view hierarchy with the feed enabled should be: self.view ->
  // self.feedWrapperViewController.view ->
  // self.feedWrapperViewController.feedViewController.view ->
  // self.collectionView -> self.contentSuggestionsViewController.view.
  if (self.contentSuggestionsViewController) {
    if (![self.collectionView.subviews
            containsObject:self.contentSuggestionsViewController.view]) {
      // Remove child VC from old parent.
      [self.contentSuggestionsViewController
          willMoveToParentViewController:nil];
      [self.contentSuggestionsViewController removeFromParentViewController];
      [self.contentSuggestionsViewController.view removeFromSuperview];
      [self.contentSuggestionsViewController didMoveToParentViewController:nil];

      if (!IsHomeCustomizationEnabled() || self.mostVisitedVisible) {
        // Add child VC to new parent.
        [self.contentSuggestionsViewController
            willMoveToParentViewController:self.feedWrapperViewController
                                               .feedViewController];
        [self.feedWrapperViewController.feedViewController
            addChildViewController:self.contentSuggestionsViewController];
        [self.collectionView
            addSubview:self.contentSuggestionsViewController.view];
        [self.contentSuggestionsViewController
            didMoveToParentViewController:self.feedWrapperViewController
                                              .feedViewController];

        [self.feedMetricsRecorder
            recordBrokenNTPHierarchy:BrokenNTPHierarchyRelationship::
                                         kContentSuggestionsParent];
      }
    }
  }

  [self ensureView:self.headerViewController.view
             isSubviewOf:self.collectionView
      withRelationshipID:BrokenNTPHierarchyRelationship::
                             kContentSuggestionsHeaderParent];

  [self ensureView:self.feedHeaderViewController.view
             isSubviewOf:self.collectionView
      withRelationshipID:BrokenNTPHierarchyRelationship::kFeedHeaderParent];
  [self ensureView:self.collectionView
             isSubviewOf:self.feedWrapperViewController.feedViewController.view
      withRelationshipID:BrokenNTPHierarchyRelationship::kELMCollectionParent];
  [self ensureView:self.feedWrapperViewController.feedViewController.view
             isSubviewOf:self.feedWrapperViewController.view
      withRelationshipID:BrokenNTPHierarchyRelationship::kDiscoverFeedParent];
  [self ensureView:self.feedWrapperViewController.view
             isSubviewOf:self.view
      withRelationshipID:BrokenNTPHierarchyRelationship::
                             kDiscoverFeedWrapperParent];
}

// Ensures that `subView` is a descendent of `parentView`. If not, logs a DCHECK
// and adds the subview. Includes `relationshipID` for metrics recorder to log
// which part of the view hierarchy was broken.
// TODO(crbug.com/40799579): Remove this once bug is fixed.
- (void)ensureView:(UIView*)subView
           isSubviewOf:(UIView*)parentView
    withRelationshipID:(BrokenNTPHierarchyRelationship)relationship {
  if (![parentView.subviews containsObject:subView]) {
    DCHECK([parentView.subviews containsObject:subView]);
    [subView removeFromSuperview];
    [parentView addSubview:subView];
    [self.feedMetricsRecorder recordBrokenNTPHierarchy:relationship];
  }
}

// Checks if the collection view is scrolled at least to the minimum height and
// updates property.
- (void)updateScrolledToMinimumHeight {
  CGFloat scrollPosition = [self scrollPosition];
  CGFloat minimumHeightOffset = AlignValueToPixel([self pinnedOffsetY]);

  self.scrolledToMinimumHeight = scrollPosition >= minimumHeightOffset;
}

// Adds `viewController` as a child of `parentViewController` and adds
// `viewController`'s view as a subview of `self.collectionView`.
- (void)addViewControllerAboveFeed:(UIViewController*)viewController {
  // Gets the current parent view controller based on feed visibility.
  UIViewController* parentViewController =
      self.feedVisible ? self.feedWrapperViewController.feedViewController
                       : self.feedWrapperViewController;

  // Adds view controller and its view as children of the parent view
  // controller.
  [viewController willMoveToParentViewController:parentViewController];
  [parentViewController addChildViewController:viewController];
  [self.collectionView addSubview:viewController.view];
  [viewController didMoveToParentViewController:parentViewController];

  // Adds view controller to array of view controllers above feed.
  [self.viewControllersAboveFeed addObject:viewController];
}

// Removes `viewController` and its corresponding view from the view hierarchy.
- (void)removeFromViewHierarchy:(UIViewController*)viewController {
  [viewController willMoveToParentViewController:nil];
  [viewController.view removeFromSuperview];
  [viewController removeFromParentViewController];
  [viewController didMoveToParentViewController:nil];
}

// Whether the fake omnibox gets pinned to the top, or becomes the real primary
// toolbar. The former is for narrower devices like portait iPhones, and the
// latter is for wider devices like iPads and landscape iPhones.
- (BOOL)shouldPinFakeOmnibox {
  return !IsRegularXRegularSizeClass(self) && IsSplitToolbarMode(self);
}

// Modifies the view controller depending on which UITrait was changed.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    [self updateModularHomeBackgroundColorForUserInterfaceStyle:
              self.traitCollection.userInterfaceStyle];
  }

  if (previousTraitCollection.horizontalSizeClass !=
      self.traitCollection.horizontalSizeClass) {
    // Update header constant to cover rotation instances. When the omnibox is
    // pinned to the top, the fake omnibox is the one shown only in portrait
    // mode, so if the NTP is opened in landscape mode, a rotation to portrait
    // mode needs to update the top anchor constant based on the correct header
    // height.
    self.headerTopAnchor.constant =
        -([self stickyOmniboxHeight] + [self feedHeaderHeight]);
    [self.contentSuggestionsViewController.view setNeedsLayout];
    [self.contentSuggestionsViewController.view layoutIfNeeded];
    [self updateOverscrollActionsState];
    [self updateHeightAboveFeed];
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateFakeOmniboxForScrollPosition];
    [self updateOverscrollActionsState];
    // Subviews will receive traitCollectionDidChange after this call, so the
    // only way to ensure that the scrollview isn't scrolled up too far is to
    // circle back afterwards and adjust if needed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^{
          [self updateHeightAboveFeed];
        }));
  }
}

#pragma mark - Getters

// Returns the container view of the NTP content, depending on prefs and flags.
- (UIView*)containerView {
  UIView* containerView;
  if (self.feedVisible) {
    // TODO(crbug.com/40799579): Remove this when the bug is fixed.
    if (IsNTPViewHierarchyRepairEnabled()) {
      [self verifyNTPViewHierarchy];
    }
    containerView = self.feedWrapperViewController.feedViewController.view;
  } else {
    containerView = self.view;
  }
  return containerView;
}

#pragma mark - Setters

// Sets whether or not the NTP is scrolled into the feed and notifies the
// content suggestions layout to avoid it changing the omnibox frame when this
// view controls its position.
- (void)setIsScrolledIntoFeed:(BOOL)scrolledIntoFeed {
  _scrolledIntoFeed = scrolledIntoFeed;
}

// Sets the y content offset of the NTP collection view.
- (void)setContentOffset:(CGFloat)offset {
  UICollectionView* collectionView = self.collectionView;
  if (!self.feedVisible) {
    // When the feed is not visible, enforce a max scroll position so that it
    // doesn't end up scrolled down when no content is there. When the feed is
    // visible, its content might load after the content offset is restored.
    CGFloat maxOffset = collectionView.contentSize.height +
                        collectionView.contentInset.bottom -
                        collectionView.bounds.size.height;
    offset = MIN(maxOffset, offset);
  }
  collectionView.contentOffset = CGPointMake(0, offset);
  self.scrolledIntoFeed = offset > [self offsetWhenScrolledIntoFeed];
  [self handleStickyElementsForScrollPosition:offset force:YES];
  [self updateScrollPositionToSave];
}

@end
