// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_view_controller.h"

#import <algorithm>
#import <optional>

#import "base/apple/foundation_util.h"
#import "base/cancelable_callback.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/banner_promo_view.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/toolbar_progress_bar.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_view.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_element_with_background.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_height_delegate.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The name of the default NTP background color asset.
NSString* const kNTPBackgroundColor = @"ntp_background_color";

// Spacing between buttons in the toolbar's horizontal stack view.
constexpr CGFloat kStackViewSpacing = 9;

// Spacing between buttons for the legacy toolbar button design.
constexpr CGFloat kLegacyStackViewSpacing = 14;

// Outside margin for the legacy toolbar layout.
constexpr CGFloat kLegacyOutsideMargin = 10;

// Duration of the banner promo slide animation.
const base::TimeDelta kBannerPromoAnimationDuration = base::Seconds(0.5);

// The minimum scale factor for a button.
constexpr CGFloat kButtonMinScale = 0.37;

// Duration of standard toolbar transition animations (fade, scale).
constexpr CGFloat kAnimationDuration = 0.2;

// Margin between the location bar and the tab group indicator view.
constexpr CGFloat kLocationBarToTabGroupMargin = 6;

// The margin for the leading/trailing edges of the stack view.
// Regular-Regular (iPad) size class margin.
constexpr CGFloat kStackViewMarginRegularRegular = 16;
// iPhone landscape margin.
constexpr CGFloat kStackViewMarginLandscape = 46;
// iPhone portrait margin.
constexpr CGFloat kStackViewMarginPortrait = 9;
constexpr CGFloat kGlassStackViewMarginPortrait = 10;

// The margin between the stack views and the location bar.
// Regular-Regular (iPad) size class margin.
constexpr CGFloat kLocationBarStackViewMarginRegularRegular = 40;
// iPhone portrait margin.
constexpr CGFloat kLocationBarStackViewMarginPortrait = 9;
constexpr CGFloat kGlassLocationBarStackViewMarginPortrait = 8;
// iPhone landscape margin.
constexpr CGFloat kLocationBarStackViewMarginLandscape = 18;

// Maximum allowed width for the location bar.
constexpr CGFloat kLocationBarMaxWidth = 600;

// The threshold for the fullscreen progress of a collapsed toolbar.
constexpr CGFloat kFullscreenCollapsedThreshold = 0.05;

// The threshold for fullscreen progress above which buttons are fully visible
// and unscaled.
constexpr CGFloat kFullscreenProgressThreshold = 0.99;

// The progress threshold for the fullscreen transition when expanding (progress
// above this value is fully normal).
constexpr CGFloat kGlassFullscreenExpandedThreshold = 0.8;

// The progress threshold for the fullscreen transition when collapsing
// (progress below this value is fully fullscreen).
constexpr CGFloat kGlassFullscreenCollapsedThreshold = 0.2;

// The progress threshold below which glass toolbar buttons stop moving.
constexpr CGFloat kGlassButtonStillProgressThreshold = 0.8;

// The progress threshold above which glass toolbar buttons move linearly with
// the background.
constexpr CGFloat kGlassButtonLinearProgressThreshold = 0.9;

// The progress threshold below which glass toolbar buttons background alpha is
// 0.
constexpr CGFloat kGlassButtonBackgroundMinProgressThreshold = 0.5;

// The progress thresholds between which glass toolbar buttons fade in.
constexpr CGFloat kGlassButtonMinAlphaProgressThreshold = 0.4;
constexpr CGFloat kGlassButtonMaxAlphaProgressThreshold = 0.7;

// Timing to finish the animation of the progress bar before hiding it.
const base::TimeDelta kProgressBarEndAnimationDuration =
    base::Milliseconds(250);

// Location bar expanded height for the glass effect view.
constexpr CGFloat kGlassLocationBarExpandedHeight = 44;

// Shadow radius for the glass effect container.
constexpr CGFloat kGlassShadowRadius = 7;

// Vertical shadow offset for the glass effect container.
constexpr CGFloat kGlassShadowOffsetY = 7;

// Shadow opacity for the glass effect container (9% Black).
constexpr CGFloat kGlassShadowOpacity = 0.09;

// Dark mode background opacity for the glass effect container (25% Black).
constexpr CGFloat kGlassContainerDarkBackgroundAlpha = 0.25;

// The scale factor for the glass effect container when in fullscreen.
constexpr CGFloat kGlassFullscreenScaleFactor = 0.8;

// The maximum width of the collapsed location bar in fullscreen.
constexpr CGFloat kMaxCollapsedLocationBarWidth = 350.0;

// The minimum width of the collapsed location bar in fullscreen. Guards against
// the collapsed pill degenerating when the text-only location bar reports no
// content.
constexpr CGFloat kMinCollapsedLocationBarWidth = 50.0;

// The background alpha is linear between
// kGlassButtonBackgroundMinProgressThreshold and 1, 0 below.
CGFloat ButtonBackgroundAlphaForProgress(CGFloat progress) {
  return progress > kFullscreenProgressThreshold
             ? 1
             : std::clamp<CGFloat>(
                   (progress - kGlassButtonBackgroundMinProgressThreshold) /
                       (1.0 - kGlassButtonBackgroundMinProgressThreshold),
                   0.0, 1.0);
}

// The button alpha is linear between kGlassButtonMinAlphaProgressThreshold and
// kGlassButtonMaxAlphaProgressThreshold, 0 below and 1 above.
CGFloat ButtonAlphaForProgress(CGFloat progress) {
  return std::clamp<CGFloat>(
      (progress - kGlassButtonMinAlphaProgressThreshold) /
          (kGlassButtonMaxAlphaProgressThreshold -
           kGlassButtonMinAlphaProgressThreshold),
      0.0, 1.0);
}

}  // namespace

@interface ToolbarViewController () <TabGroupIndicatorViewDelegate,
                                     ToolbarViewDelegate,
                                     UIContextMenuInteractionDelegate>
@end

@implementation ToolbarViewController {
  ToolbarButton* _backButton;
  UIMenu* _backButtonMenu;
  ToolbarButton* _forwardButton;
  UIMenu* _forwardButtonMenu;
  ToolbarButton* _reloadButton;
  ToolbarButton* _stopButton;
  ToolbarButton* _shareButton;
  ToolbarButton* _assistantButton;
  UIMenu* _assistantButtonMenu;
  ToolbarTabGridBadgeButton* _tabGridButton;
  UIMenu* _tabGridButtonMenu;
  ToolbarButton* _toolsMenuButton;

  // Button taking the full size of the toolbar. Exits fullscreen mode to expand
  // the toolbar when tapped.
  UIButton* _collapsedToolbarButton;

  // Page load progress bar on the edge of the toolbar.
  ToolbarProgressBar* _progressBar;
  // Container for the `_progressBar`.
  UIView* _progressBarContainer;

  // The inner separator line for the toolbar. Positioned at the top edge of the
  // bottom toolbar or bottom edge of the top toolbar to separate the toolbar
  // content from the web page content. Visible when the toolbar has the omnibox
  // or when the tab group indicator is visible.
  UIView* _innerSeparator;

  // The outer separator line for the bottom toolbar. Positioned at the bottom
  // edge of the bottom toolbar to separate it from external system elements
  // like the keyboard when the bottom toolbar is elevated.
  UIView* _outerSeparator;

  // Whether the location indicator is currently active (toolbar above
  // keyboard).
  BOOL _locationIndicatorActive;

  // The centerX constraint used to center the location bar container on the
  // screen/window when the keyboard is visible.
  NSLayoutConstraint* _locationBarKeyboardCenterXConstraint;

  // Closure to cancel hiding the progress bar when a new page load starts.
  base::CancelableOnceClosure _hideProgressBarClosure;

  // Dynamic container for the `_backButton` and `_forwardButton` Toolbar
  // navigation buttons in the `_leadingStackView`.
  UIView<ToolbarElementWithBackground>* _navigationButtonsContainer;
  // The stack views that hold the buttons on the leading side.
  UIStackView* _leadingStackView;
  // The container for the location bar, which is transparent.
  UIView* _locationBarContainer;
  // The background for the location bar, which is a pill-shaped view.
  UIView* _locationBarBackground;
  // Content view for the location bar that clips subviews to the pill shape
  // without clipping the shadow on `_locationBarBackground`.
  UIView* _locationBarContentView;
  // The location bar.
  UIViewController* _locationBarViewController;
  // The layout guide constrained to the steady view of the location bar.
  UILayoutGuide* _steadyViewLayoutGuide;
  // The text-only location bar in this toolbar (used for fullscreen animation).
  UIViewController* _textOnlyLocationBarViewController;
  // The target for the fake omnibox, which replaces the location bar when the
  // location bar is not visible.
  UIView* _fakeOmniboxTarget;
  // The stack views that hold the buttons on the trailing side.
  UIStackView* _trailingStackView;

  // Array containing all the buttons in the toolbar.
  NSArray<UIView<ToolbarElementWithBackground>*>* _allButtons;

  // Container view with shadow for the glass effect. Adding a shadow on the
  // VisualEffectView containing the glass effect directly doesn't work, so add
  // a container with a shadow.
  UIView* _glassBackgroundContainer;

  // The visual effect view for the glass effect background when glass toolbar
  // is enabled.
  UIVisualEffectView* _glassBackgroundView;

  // A overlay on top of the glass effect, below any other subview.
  UIView* _glassBackgroundOverlay;

  // Height constraint for the glass effect view container.
  NSLayoutConstraint* _glassBackgroundHeightConstraint;
  // Leading constraint for the glass effect view container.
  NSLayoutConstraint* _glassBackgroundLeadingConstraint;
  // Trailing constraint for the glass effect view container.
  NSLayoutConstraint* _glassBackgroundTrailingConstraint;
  // Constraint the glass background by its bottom anchor.
  NSLayoutConstraint* _glassBackgroundBottomConstraint;
  // Constraint the glass background by its top anchor.
  NSLayoutConstraint* _glassBackgroundTopConstraint;

  // The tab group indicator view.
  TabGroupIndicatorView* _tabGroupIndicatorView;

  // The location bar height constraint.
  NSLayoutConstraint* _locationBarHeightConstraint;
  // The constraint for the bottom padding of the toolbar.
  NSLayoutConstraint* _locationBarBottomPaddingConstraint;
  // The constraint for the top padding of the toolbar when collapsed above the
  // keyboard.
  NSLayoutConstraint* _locationBarTopConstraint;

  // Constraints for the tabGroupIndicator.
  NSLayoutConstraint* _tabGroupIndicatorActiveToolbarConstraint;
  NSLayoutConstraint* _tabGroupIndicatorInactiveToolbarConstraint;

  // Constraints for different modes.
  NSArray<NSLayoutConstraint*>* _portraitOrientationConstraints;
  NSArray<NSLayoutConstraint*>* _landscapeOrientationConstraints;
  NSArray<NSLayoutConstraint*>* _regularRegularConstraints;

  // The constraints for the leading/trailing margins of the stacks.
  NSLayoutConstraint* _leadingStackLeadingConstraint;
  NSLayoutConstraint* _trailingStackTrailingConstraint;

  // Whether this toolbar is in the top position.
  BOOL _topPosition;

  // Whether this toolbar contains the omnibox.
  BOOL _hasOmnibox;

  // Whether this toolbar is in incognito mode.
  BOOL _incognito;

  // Whether the visible page is the NTP.
  BOOL _NTPVisible;

  // Whether the NTP is showing the Start Surface.
  BOOL _isStartSurface;

  // Whether the visible page is loading.
  BOOL _isLoading;

  // Used to record the latest fullscreen progress.
  CGFloat _fullscreenProgress;

  // YES when the last "settled" state was Fullscreen and NO when the last
  // "settled" state was not Fullscreen.
  BOOL _isFullscreen;

  // Used to record the scroll progress to show and hide the toolbar.
  CGFloat _NTPScrollProgress;

  // Background and container for the banner promo.
  UIView* _bannerPromoBackground;
  // The banner promo view.
  BannerPromoView* _bannerPromo;
  // Whether the banner promo is displayed.
  BOOL _bannerPromoVisible;
  // Constraint for the banner promo background height.
  NSLayoutConstraint* _bannerPromoBackgroundHeightConstraint;
  // Constraint for the banner promo background top.
  NSLayoutConstraint* _bannerPromoBackgroundTopConstraint;
  // Constraints for the banner promo in split toolbar mode (where the banner is
  // above the toolbar).
  NSArray<NSLayoutConstraint*>* _bannerPromoAboveConstraints;
  // Constraints for the banner promo in non-split toolbar mode (where the
  // banner is below the toolbar).
  NSArray<NSLayoutConstraint*>* _bannerPromoBelowConstraints;

  // Constraints for the text-only location bar relative to the glass background
  // view.
  NSLayoutConstraint* _textOnlyLeadingConstraint;
  NSLayoutConstraint* _textOnlyTrailingConstraint;
  NSLayoutConstraint* _textOnlyTopConstraint;
  NSLayoutConstraint* _textOnlyBottomConstraint;

  // Compressed width of the text-only location bar, see
  // `-locationBarCollapsedWidth`. Measuring it runs an Auto Layout pass, while
  // the fullscreen progress is updated on every frame of a transition, so the
  // result is memoized for the duration of a layout cycle.
  std::optional<CGFloat> _locationBarCollapsedWidth;
}

- (instancetype)initInIncognito:(BOOL)incognito topPosition:(BOOL)topPosition {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _incognito = incognito;
    _topPosition = topPosition;
    _fullscreenProgress = 1.0;
  }
  return self;
}

#pragma mark - Public

- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  if (_tabGroupIndicatorView == view) {
    return;
  }
  _tabGroupIndicatorView = view;
  if (!_tabGroupIndicatorView) {
    return;
  }
  _tabGroupIndicatorView.delegate = self;
  // ToolbarViewController will show its own _separator, when needed.
  _tabGroupIndicatorView.showSeparator = NO;
  _tabGroupIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_tabGroupIndicatorView];

  _tabGroupIndicatorActiveToolbarConstraint =
      [_tabGroupIndicatorView.bottomAnchor
          constraintEqualToAnchor:_locationBarContainer.topAnchor
                         constant:-kLocationBarToTabGroupMargin];

  _tabGroupIndicatorInactiveToolbarConstraint =
      [_tabGroupIndicatorView.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor];

  [self updateTabGroupIndicatorAvailability];

  id<LayoutGuideProvider> safeArea = self.view.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [_tabGroupIndicatorView.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor],
    [_tabGroupIndicatorView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor],
    [_tabGroupIndicatorView.heightAnchor
        constraintEqualToConstant:kTabGroupIndicatorHeight],
  ]];
}

- (void)setNTPScrollProgress:(CGFloat)progress {
  if (!_NTPVisible) {
    return;
  }
  _NTPScrollProgress = progress;

  if (!CanShowTabStrip(self)) {
    if (self.view.alpha == progress) {
      // No change in toolbar visibility alpha.
      return;
    }
    // On iPhone, the entire toolbar is initially hidden on the NTP, and appears
    // based on the scroll `progress`.
    [self setNTPScrollProgressForToolbar:progress];
    return;
  }

  if (_topPosition) {
    // On iPad, the toolbar is always visible. The location bar is initially
    // hidden on the NTP and  appears based on the scroll `progress`.
    [self setNTPScrollProgressForOmnibox:progress];
  }
}

- (void)setLocationBarHidden:(BOOL)hidden {
  _locationBarContainer.hidden = hidden || !_hasOmnibox;
}

- (UIView*)locationBarContainerCopy {
  UIView* locationBarContainerCopy = [self createLocationBarBackground];
  locationBarContainerCopy.translatesAutoresizingMaskIntoConstraints = YES;
  locationBarContainerCopy.frame =
      [_locationBarBackground convertRect:_locationBarBackground.bounds
                                   toView:nil];
  return locationBarContainerCopy;
}

#pragma mark - Properties

- (void)setLocationBarViewController:
            (UIViewController*)locationBarViewController
            andSteadyViewLayoutGuide:(UILayoutGuide*)steadyViewLayoutGuide {
  if (_locationBarViewController == locationBarViewController &&
      _steadyViewLayoutGuide == steadyViewLayoutGuide) {
    return;
  }
  _steadyViewLayoutGuide = steadyViewLayoutGuide;
  [self loadViewIfNeeded];

  if (_locationBarViewController &&
      [_locationBarViewController.view isDescendantOfView:self.view]) {
    [_locationBarViewController willMoveToParentViewController:nil];
    [_locationBarViewController.view removeFromSuperview];
    [_locationBarViewController removeFromParentViewController];
  }

  _locationBarViewController = locationBarViewController;

  UIView* locationBarView = locationBarViewController.view;
  locationBarView.translatesAutoresizingMaskIntoConstraints = NO;
  [locationBarView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                     forAxis:UILayoutConstraintAxisHorizontal];

  [self addChildViewController:_locationBarViewController];
  if (IsGlassToolbarEnabled()) {
    [_locationBarContentView addSubview:locationBarView];
    AddSameConstraints(locationBarView, _locationBarContentView);
  } else {
    [_locationBarContainer addSubview:locationBarView];
    AddSameConstraints(locationBarView, _locationBarContainer);
  }
  [_locationBarViewController didMoveToParentViewController:self];
  [self updateTextOnlyLocationBarViewConstraints];
}

- (void)setTextOnlyLocationBarViewController:
    (UIViewController*)textOnlyLocationBarViewController {
  if (!IsGlassToolbarEnabled()) {
    return;
  }
  if (_textOnlyLocationBarViewController == textOnlyLocationBarViewController) {
    return;
  }
  [self loadViewIfNeeded];

  if (_textOnlyLocationBarViewController &&
      [_textOnlyLocationBarViewController.view isDescendantOfView:self.view]) {
    [_textOnlyLocationBarViewController willMoveToParentViewController:nil];
    [_textOnlyLocationBarViewController.view removeFromSuperview];
    [_textOnlyLocationBarViewController removeFromParentViewController];
  }

  _textOnlyLocationBarViewController = textOnlyLocationBarViewController;
  _locationBarCollapsedWidth.reset();
  if (!_textOnlyLocationBarViewController) {
    return;
  }

  UIView* textOnlyView = _textOnlyLocationBarViewController.view;
  textOnlyView.translatesAutoresizingMaskIntoConstraints = NO;
  textOnlyView.alpha = 1;
  textOnlyView.hidden = YES;

  [self addChildViewController:_textOnlyLocationBarViewController];
  [_glassBackgroundView.contentView addSubview:textOnlyView];
  [_textOnlyLocationBarViewController didMoveToParentViewController:self];

  [self updateTextOnlyLocationBarViewConstraints];
}

- (void)setBannerPromoDelegate:
    (id<BannerPromoViewDelegate>)bannerPromoDelegate {
  _bannerPromoDelegate = bannerPromoDelegate;
  _bannerPromo.delegate = bannerPromoDelegate;
}

#pragma mark - UIViewController

- (void)loadView {
  ToolbarView* view = [[ToolbarView alloc] init];
  view.delegate = self;
  self.view = view;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.view.backgroundColor = [self toolbarBackgroundColor];
  self.view.accessibilityIdentifier = _topPosition
                                          ? kPrimaryToolbarViewIdentifier
                                          : kSecondaryToolbarViewIdentifier;

  [self createView];
  [self setUpHierarchy];

  [self updateToolbarElementsVisibility];
  [self updateToolbarVisibility];
  [self updateTabGroupIndicatorAvailability];

  if (_topPosition) {
    [self.layoutGuideCenter referenceView:self.view
                                underName:kPrimaryToolbarGuide];
    [self.layoutGuideCenter referenceView:_locationBarContainer
                                underName:kTopOmniboxGuide];
  } else {
    [self.layoutGuideCenter referenceView:self.view
                                underName:kSecondaryToolbarGuide];
  }

  [self
      registerForTraitChanges:
          @[ UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class ]
                   withAction:@selector(sizeClassDidChange)];

  [self registerForTraitChanges:
            @[ UITraitUserInterfaceStyle.class, NewTabPageTrait.class ]
                     withAction:@selector(userInterfaceStyleDidChange)];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  _bannerPromoBackgroundHeightConstraint.constant = [self
      bannerPromoBackgroundHeightForFullscreenProgress:_fullscreenProgress];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if (!IsGlassToolbarEnabled()) {
    return;
  }

  // The collapsed pill is sized from the text-only location bar. Its content
  // can change with no fullscreen progress update at all (navigation, badge or
  // reader mode chip appearing); in that case the measurement was dropped in
  // `-locationBarContentSizeDidChange`, which also requested this layout pass.
  // Re-apply the interpolation so the new content is taken into account. The
  // setters below ignore unchanged values, so this cannot loop.
  if (_fullscreenProgress < kFullscreenProgressThreshold) {
    [self updateGlassBackgroundInsetsForFullscreenProgress:_fullscreenProgress];
    [self updateStackViewMarginsForFullscreenProgress:_fullscreenProgress];
  }
}

#pragma mark - UIContentContainer

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  __weak __typeof(self) weakSelf = self;
  CGFloat progress = _fullscreenProgress;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateForFullscreenProgress:progress];
      }
                      completion:nil];
}

#pragma mark - ToolbarViewDelegate

- (void)toolbarViewDidMoveToWindow:(ToolbarView*)view {
  [self updateTabSwitcherGuide];
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  _toolsMenuButton.iphHighlighted = YES;
}

- (void)updateUIForIPHDismissed {
  _toolsMenuButton.iphHighlighted = NO;
  _tabGridButton.iphHighlighted = NO;
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  _toolsMenuButton.hasBlueDot = hasBlueDot;
}

#pragma mark - ToolbarConsumer

- (void)setAssistantButtonVisible:(BOOL)visible enabled:(BOOL)enabled {
  _assistantButton.forceHidden = !visible;
  _assistantButton.enabled = enabled;
  if (self.isViewLoaded) {
    [self updateLayoutGuides];
  }
}

- (void)setCanGoBack:(BOOL)canGoBack {
  _backButton.enabled = canGoBack;
}

- (void)setCanGoForward:(BOOL)canGoForward animated:(BOOL)animated {
  if (_forwardButton.enabled == canGoForward) {
    return;
  }

  if (IsNextOldDesignEnabled()) {
    _forwardButton.enabled = canGoForward;
    if (IsRegularXRegularSizeClass(self)) {
      _forwardButton.hidden = NO;
    } else {
      _forwardButton.hidden = !canGoForward;
    }
    return;
  }

  if (canGoForward) {
    _forwardButton.enabled = YES;
  }

  // If no animation is requested, snap instantly.
  if (!animated) {
    _forwardButton.hidden = !canGoForward;
    if (!canGoForward) {
      _forwardButton.enabled = NO;
    }
    return;
  }

  // Otherwise, do the smooth fade.
  __weak __typeof(self) weakSelf = self;
  ToolbarButton* forwardButton = _forwardButton;
  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        forwardButton.hidden = !canGoForward;
        [weakSelf.view layoutIfNeeded];
      }
      completion:^(BOOL) {
        if (!canGoForward) {
          forwardButton.enabled = NO;
        }
      }];
}

- (void)setShareEnabled:(BOOL)enabled {
  if (enabled == _shareButton.isEnabled) {
    return;
  }
  _shareButton.enabled = enabled;
}

- (void)setHasOmnibox:(BOOL)hasOmnibox {
  if (_hasOmnibox == hasOmnibox) {
    return;
  }
  _hasOmnibox = hasOmnibox;
  if (!_hasOmnibox) {
    _hideProgressBarClosure.Cancel();
    _progressBar.hidden = YES;
  }
  [self loadViewIfNeeded];
  [self updateToolbarElementsVisibility];
  [self updateTabGroupIndicatorAvailability];
  [self updateLayoutGuides];
}

- (void)setNTPVisible:(BOOL)NTPVisible
       isStartSurface:(BOOL)isStartSurface
            isLoading:(BOOL)isLoading
      loadingProgress:(double)progress {
  _isStartSurface = isStartSurface;

  BOOL ntpVisibilityChanged = (NTPVisible != _NTPVisible);
  BOOL loadingStateChanged = (isLoading != _isLoading);

  if (loadingStateChanged || ntpVisibilityChanged) {
    if (ntpVisibilityChanged) {
      _NTPVisible = NTPVisible;
      [self updateBackgroundColors];
      [self updateToolbarVisibility];
    }

    // Loading UI should not be shown on the NTP.
    BOOL mustHideLoadingUI = !isLoading || NTPVisible;
    _isLoading = !mustHideLoadingUI;
    _reloadButton.forceHidden = !mustHideLoadingUI;
    _stopButton.forceHidden = mustHideLoadingUI;

    if (_hasOmnibox && loadingStateChanged && isLoading) {
      [_progressBar setProgress:0.0 animated:NO];
    }
    [self updateProgressBarVisibility];
  }

  if (_hasOmnibox && progress != _progressBar.progress) {
    BOOL isGoingBackward = progress < _progressBar.progress;
    [_progressBar setProgress:progress
                     animated:!_progressBar.isHidden && !isGoingBackward];
  }
}

- (void)updateTabCount:(NSUInteger)tabCount {
  _tabGridButton.tabCount = tabCount;
}

- (void)setInTabGroup:(BOOL)inTabGroup {
  _tabGridButton.inTabGroup = inTabGroup;
}

- (void)setMenu:(UIMenu*)menu forButtonType:(ToolbarButtonType)buttonType {
  switch (buttonType) {
    case ToolbarButtonTypeBack:
      _backButtonMenu = menu;
      _backButton.menu = menu;
      return;
    case ToolbarButtonTypeForward:
      _forwardButtonMenu = menu;
      _forwardButton.menu = menu;
      return;
    case ToolbarButtonTypeAssistant:
      /// TODO(crbug.com/484000556): Add a context menu for the assistant button
      /// when it is implemented (iPad).
      _assistantButtonMenu = menu;
      _assistantButton.menu = menu;
      return;
    case ToolbarButtonTypeTabGrid:
      _tabGridButtonMenu = menu;
      return;
    case ToolbarButtonTypeReload:
    case ToolbarButtonTypeStop:
    case ToolbarButtonTypeShare:
    case ToolbarButtonTypeTools:
      NOTIMPLEMENTED() << "This button does not have a context menu";
      return;
  }
  NOTREACHED();
}

- (void)setLocationIndicatorVisible:(BOOL)locationIndicatorVisible
                    forNotification:(NSNotification*)notification {
  CHECK(!_topPosition);
  _locationIndicatorActive = locationIndicatorVisible;
  if (locationIndicatorVisible) {
    if (IsGlassToolbarEnabled()) {
      _glassBackgroundBottomConstraint.active = NO;
      _glassBackgroundTopConstraint.active = YES;
    } else {
      _locationBarBottomPaddingConstraint.active = NO;
      _locationBarTopConstraint.active = YES;
    }
    [self.toolbarHeightDelegate secondaryToolbarMovedAboveKeyboard];

    [NSLayoutConstraint deactivateConstraints:_portraitOrientationConstraints];
    [NSLayoutConstraint deactivateConstraints:_landscapeOrientationConstraints];
    [NSLayoutConstraint deactivateConstraints:_regularRegularConstraints];
    if (!_locationBarKeyboardCenterXConstraint) {
      _locationBarKeyboardCenterXConstraint =
          [_locationBarContainer.centerXAnchor
              constraintEqualToAnchor:self.view.window.centerXAnchor];
    }
    _locationBarKeyboardCenterXConstraint.active = YES;
  } else {
    if (_topPosition) {
      if (IsGlassToolbarEnabled()) {
        _glassBackgroundTopConstraint.active = NO;
        _glassBackgroundBottomConstraint.active = YES;
      } else {
        _locationBarTopConstraint.active = NO;
        _locationBarBottomPaddingConstraint.active = YES;
      }
    } else {
      if (IsGlassToolbarEnabled()) {
        _glassBackgroundTopConstraint.active = YES;
        _glassBackgroundBottomConstraint.active = NO;
      } else {
        _locationBarTopConstraint.active = YES;
        _locationBarBottomPaddingConstraint.active = NO;
      }
    }
    [self.toolbarHeightDelegate secondaryToolbarRemovedFromKeyboard];

    _locationBarKeyboardCenterXConstraint.active = NO;
    [self updateLayoutConstraints];
  }
  [self updateSeparatorVisibility];

  [self.view layoutIfNeeded];

  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);

  CGFloat visibleKeyboardHeight = 0;
  if (locationIndicatorVisible) {
    if ([self useAccessoryViewPosition]) {
      visibleKeyboardHeight = [self inputAccessoryHeightInWindow];
    } else {
      visibleKeyboardHeight =
          VisibleKeyboardHeightFromNotification(notification, self.view.window);
    }
  }

  [self.toolbarHeightDelegate
      adjustSecondaryToolbarForKeyboardHeight:visibleKeyboardHeight
                                  isCollapsed:locationIndicatorVisible
                                     duration:duration
                                        curve:curve];
}

- (void)showBannerPromo {
  CHECK(_topPosition);
  if (_bannerPromoVisible) {
    return;
  }
  [self setUpBannerPromoIfNecessary];
  _bannerPromoVisible = YES;
  _bannerPromoBackground.alpha = 1;

  [self updateBannerConstraints];

  if ([self isBannerBelowToolbar]) {
    _bannerPromoBackgroundHeightConstraint.constant = 0;
  } else {
    _bannerPromoBackgroundHeightConstraint.constant =
        [self bannerPromoBackgroundHeightForFullscreenProgress:1];
    _bannerPromoBackgroundTopConstraint.constant =
        -_bannerPromoBackgroundHeightConstraint.constant;
  }

  [self.view layoutIfNeeded];
  _bannerPromoBackgroundTopConstraint.constant = 0;

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kBannerPromoAnimationDuration.InSecondsF()
      animations:^{
        [weakSelf showBannerPromoAnimationBlock];
      }
      completion:^(BOOL finished) {
        [weakSelf showBannerPromoCompletionBlock];
      }];
}

- (void)hideBannerPromo {
  if (!_bannerPromoVisible) {
    return;
  }
  [self.view.superview layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kBannerPromoAnimationDuration.InSecondsF()
      animations:^{
        [weakSelf hideBannerPromoAnimationBlock];
      }
      completion:^(BOOL completed) {
        [weakSelf hideBannerPromoCompletionBlock];
      }];

  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  if (progress == 1.0) {
    _isFullscreen = NO;
  } else if (progress == 0.0) {
    _isFullscreen = YES;
  }
  _fullscreenProgress = progress;
  CGFloat locationBarExpandedHeight;
  if (IsGlassToolbarEnabled()) {
    locationBarExpandedHeight = kGlassLocationBarExpandedHeight;
  } else if (ShouldHaveCompactLocationBar(self.traitCollection)) {
    locationBarExpandedHeight = kLocationBarHeight;
  } else {
    locationBarExpandedHeight = kTopLocationBarIPhonePortraitHeight;
  }
  CGFloat collapsedLocationBarHeight = kLocationBarHeightFullscreen;
  if (!_topPosition) {
    collapsedLocationBarHeight = ToolbarCollapsedHeight(
        self.traitCollection.preferredContentSizeCategory);
  }
  CGFloat locationBarHeight = progress * locationBarExpandedHeight +
                              (1 - progress) * collapsedLocationBarHeight;
  _locationBarHeightConstraint.constant = locationBarHeight;
  _locationBarBackground.layer.cornerRadius = locationBarHeight / 2.0;
  _locationBarContainer.layer.cornerRadius = locationBarHeight / 2.0;

  if (IsGlassToolbarEnabled()) {
    [self updateGlassBackgroundScaleForProgress:progress];
    CGFloat glassHeight = progress * kGlassExpandedHeight +
                          (1 - progress) * kGlassCollapsedHeight;
    _glassBackgroundHeightConstraint.constant = glassHeight;
    _glassBackgroundContainer.layer.cornerRadius = glassHeight / 2.0;
    _glassBackgroundView.layer.cornerRadius = glassHeight / 2.0;
    _glassBackgroundOverlay.layer.cornerRadius = glassHeight / 2.0;

    [self updateGlassBackgroundInsetsForFullscreenProgress:progress];
  }

  [self updateStackViewMarginsForFullscreenProgress:progress];

  if (IsGlassToolbarEnabled()) {
    _locationBarBackground.alpha = ButtonBackgroundAlphaForProgress(progress);
  } else {
    _locationBarBackground.alpha = progress;
  }

  [self updateVerticalPositionForFullscreenProgress:progress];

  _bannerPromoBackgroundHeightConstraint.constant =
      [self bannerPromoBackgroundHeightForFullscreenProgress:progress];

  [self updateButtonsForFullscreenProgress:progress];

  if (IsGlassToolbarEnabled()) {
    _glassBackgroundOverlay.backgroundColor =
        ToolbarElementBackgroundColor(_incognito, 1 - progress);
  }

  CGFloat alphaValue = fmax(progress * 2 - 1, 0);
  _tabGroupIndicatorView.alpha = alphaValue;
  if (_bannerPromoVisible) {
    _bannerPromoBackground.alpha = alphaValue;
  }

  CGFloat offset = 0;
  if (!IsRegularXRegularSizeClass(self.traitCollection) &&
      !IsIPhoneLandscape(self.traitCollection)) {
    // The location bar is not centered in iPhone landscape when forward is
    // visible. Add a translation in those cases to have the location bar
    // centered in fullscreen.
    offset = (_leadingStackView.bounds.size.width -
              _trailingStackView.bounds.size.width) /
             2.0;
  }

  if (!IsGlassToolbarEnabled()) {
    CGFloat translation = (progress - 1) * offset;

    CGAffineTransform translationTransform =
        CGAffineTransformMakeTranslation(translation, 0);
    _locationBarContainer.transform = translationTransform;
    _leadingStackView.transform = translationTransform;
    _trailingStackView.transform = translationTransform;
  }

  if (_textOnlyLocationBarViewController && IsGlassToolbarEnabled()) {
    [self updateTextOnlyLocationBarForFullscreenProgress:progress];
  }

  _collapsedToolbarButton.hidden = progress > kFullscreenCollapsedThreshold;
}

#pragma mark - LocationBarContentSizeDelegate

- (void)locationBarContentSizeDidChange {
  _locationBarCollapsedWidth.reset();
  // Deliberately no measurement here: this is called from the middle of the
  // location bar's own constraint updates. Only request a layout pass, which
  // re-applies the interpolation with the new content, and only while
  // collapsed, as the pill geometry is otherwise unaffected.
  if (_fullscreenProgress < kFullscreenProgressThreshold) {
    [self.view setNeedsLayout];
  }
}

#pragma mark - Fullscreen private helpers

// Returns the width of the glass pill when fully collapsed: the compressed
// width of the text-only location bar, bounded so that a missing measurement
// cannot produce a zero-width or overflowing pill.
- (CGFloat)locationBarCollapsedWidth {
  CHECK(IsGlassToolbarEnabled());
  CGFloat maxWidth = self.view.bounds.size.width - 2 * kGlassToolbarMargin;
  if (!_textOnlyLocationBarViewController) {
    // Nothing to collapse to, so keep the pill expanded and let the
    // interpolation be a no-op.
    return maxWidth;
  }
  if (!_locationBarCollapsedWidth) {
    _locationBarCollapsedWidth =
        [_textOnlyLocationBarViewController.view
            systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
            .width;
  }
  return std::clamp<CGFloat>(
      *_locationBarCollapsedWidth,
      std::min<CGFloat>(kMinCollapsedLocationBarWidth, maxWidth),
      std::min<CGFloat>(kMaxCollapsedLocationBarWidth, maxWidth));
}

// Updates all the buttons according to the fullscreen `progress`.
- (void)updateButtonsForFullscreenProgress:(CGFloat)progress {
  CGFloat backgroundAlpha = ButtonBackgroundAlphaForProgress(progress);
  CGFloat buttonAlpha = ButtonAlphaForProgress(progress);

  for (UIView<ToolbarElementWithBackground>* button in _allButtons) {
    if (progress > kFullscreenProgressThreshold) {
      button.alpha = 1;
      button.transform = CGAffineTransformIdentity;
    } else {
      if (IsGlassToolbarEnabled()) {
        button.alpha = buttonAlpha;
      } else {
        button.alpha = progress;
        // Linearly interpolates the scale between kButtonMinScale and 1.0.
        CGFloat scale = progress + (1.0 - progress) * kButtonMinScale;
        button.transform = CGAffineTransformMakeScale(scale, scale);
      }
    }
    if (IsGlassToolbarEnabled()) {
      [button setBackgroundAlpha:backgroundAlpha];
    }
  }
}

// Returns the height of the promo banner for `progress`.
- (CGFloat)bannerPromoBackgroundHeightForFullscreenProgress:(CGFloat)progress {
  if (!_bannerPromoVisible) {
    return 0;
  }

  if (![self isBannerBelowToolbar]) {
    return kToolbarPromoBannerHeight + self.view.safeAreaInsets.top;
  }

  return kToolbarPromoBannerHeight * progress;
}

// Updates the vertical position of the elements for fullscreen `progress`.
- (void)updateVerticalPositionForFullscreenProgress:(CGFloat)progress {
  if (_topPosition) {
    if (IsGlassToolbarEnabled()) {
      _glassBackgroundBottomConstraint.constant =
          -[self glassBackgroundBottomPaddingForFullscreenProgress:progress];
    } else {
      _locationBarBottomPaddingConstraint.constant =
          -[self locationBarBottomPaddingForFullscreenProgress:progress];
    }
  } else {
    if (IsGlassToolbarEnabled()) {
      _glassBackgroundTopConstraint.constant =
          [self glassBackgroundTopPaddingForFullscreenProgress:progress];
    } else {
      _locationBarTopConstraint.constant =
          [self locationBarTopPaddingForFullscreenProgress:progress];
    }
  }
}

// Returns the location bar bottom padding for `progress`.
- (CGFloat)locationBarBottomPaddingForFullscreenProgress:(CGFloat)progress {
  CHECK(!IsGlassToolbarEnabled());
  CGFloat locationBarBottomPadding =
      ShouldHaveCompactLocationBar(self.traitCollection)
          ? kToolbarPadding
          : kToolbarCompactLocationBarPadding;
  if ([self isBannerBelowToolbar]) {
    // When the banner is below the toolbar, always use its height for a
    // progress of 1 as progress is used below.
    locationBarBottomPadding +=
        [self bannerPromoBackgroundHeightForFullscreenProgress:1];
  }
  return progress * locationBarBottomPadding +
         (1 - progress) * kToolbarPaddingFullscreen;
}

// Returns the location bar top padding for the given Fullscreen
// `progress`.
- (CGFloat)locationBarTopPaddingForFullscreenProgress:(CGFloat)progress {
  CHECK(!IsGlassToolbarEnabled());
  CGFloat locationBarTopPadding =
      ShouldHaveCompactLocationBar(self.traitCollection)
          ? kToolbarPadding
          : kToolbarCompactLocationBarPadding;
  return progress * locationBarTopPadding +
         (1 - progress) * kToolbarPaddingFullscreen;
}

// Returns the glass background bottom padding for `progress`.
- (CGFloat)glassBackgroundBottomPaddingForFullscreenProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  CGFloat glassBackgroundBottomPadding = kGlassToolbarMargin;
  if ([self isBannerBelowToolbar]) {
    // When the banner is below the toolbar, always use its height for a
    // progress of 1 as progress is used below.
    glassBackgroundBottomPadding +=
        [self bannerPromoBackgroundHeightForFullscreenProgress:1];
  }
  return progress * glassBackgroundBottomPadding +
         (1 - progress) * kGlassFullscreenMargin;
}

// Returns the glass background top padding for `progress`.
- (CGFloat)glassBackgroundTopPaddingForFullscreenProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  return progress * kGlassToolbarMargin +
         (1 - progress) * kGlassFullscreenMargin;
}

// Returns the eased progress value for the glass toolbar given `progress`.
- (CGFloat)glassToolbarProgressForFullscreenProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  CGFloat linearProgress;
  if (_isFullscreen) {
    linearProgress = std::clamp<CGFloat>(
        progress / kGlassFullscreenExpandedThreshold, 0.0, 1.0);
    return 1.0 - (1.0 - linearProgress) * (1.0 - linearProgress);
  }
  linearProgress =
      std::clamp<CGFloat>((progress - kGlassFullscreenCollapsedThreshold) /
                              (1.0 - kGlassFullscreenCollapsedThreshold),
                          0.0, 1.0);
  return linearProgress * linearProgress;
}

// Returns the button position progress factor for fullscreen `progress`.
- (CGFloat)buttonProgressForFullscreenProgress:(CGFloat)progress {
  if (!IsGlassToolbarEnabled()) {
    return progress;
  }
  if (progress <= kGlassButtonStillProgressThreshold) {
    return kGlassButtonStillProgressThreshold;
  }
  if (progress >= kGlassButtonLinearProgressThreshold) {
    return progress;
  }
  // This is an interpolation between the linear and non-linear parts.
  CGFloat delta =
      kGlassButtonLinearProgressThreshold - kGlassButtonStillProgressThreshold;
  CGFloat t = (progress - kGlassButtonStillProgressThreshold) / delta;
  return kGlassButtonStillProgressThreshold + delta * t * t * (2.0 - t);
}

// Updates the scale of the glass background for `progress`.
- (void)updateGlassBackgroundScaleForProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  CGFloat easedProgress =
      [self glassToolbarProgressForFullscreenProgress:progress];
  CGFloat scaleValue =
      kGlassFullscreenScaleFactor * (1 - easedProgress) + easedProgress;
  _glassBackgroundContainer.transform =
      CGAffineTransformMakeScale(scaleValue, scaleValue);
}

// Returns the distance between a toolbar edge and the fully collapsed glass
// pill, which is centered horizontally.
- (CGFloat)glassCollapsedInset {
  CHECK(IsGlassToolbarEnabled());
  return (self.view.bounds.size.width - [self locationBarCollapsedWidth]) / 2.0;
}

// Returns the base leading and trailing margin of the button stack views for
// the current layout, without any fullscreen collapse offset.
- (CGFloat)stackViewBaseMargin {
  if (IsNextOldDesignEnabled()) {
    return kLegacyOutsideMargin;
  }
  if (IsRegularXRegularSizeClass(self)) {
    return kStackViewMarginRegularRegular;
  }
  if (IsIPhoneLandscape(self)) {
    return kStackViewMarginLandscape;
  }
  return IsGlassToolbarEnabled()
             ? kGlassStackViewMarginPortrait + kGlassToolbarMargin
             : kStackViewMarginPortrait;
}

// Sole writer of the glass pill's horizontal insets. Interpolates between the
// expanded margin and the centered collapsed pill for `progress`.
- (void)updateGlassBackgroundInsetsForFullscreenProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  CGFloat collapsedInset = [self glassCollapsedInset];
  CGFloat leading =
      progress * kGlassToolbarMargin + (1.0 - progress) * collapsedInset;
  // Setting an unchanged constant still invalidates layout, which would make
  // the re-apply in `-viewDidLayoutSubviews` loop.
  if (_glassBackgroundLeadingConstraint.constant != leading) {
    _glassBackgroundLeadingConstraint.constant = leading;
  }
  if (_glassBackgroundTrailingConstraint.constant != -leading) {
    _glassBackgroundTrailingConstraint.constant = -leading;
  }
}

// Sole writer of the button stack view margins. In glass mode the stacks slide
// inwards with the collapsing pill, so the base margin is offset by the pill's
// collapsed inset.
- (void)updateStackViewMarginsForFullscreenProgress:(CGFloat)progress {
  CGFloat margin = [self stackViewBaseMargin];
  if (IsGlassToolbarEnabled()) {
    CGFloat buttonProgress =
        [self buttonProgressForFullscreenProgress:progress];
    margin += (1.0 - buttonProgress) * [self glassCollapsedInset];
  }
  if (_leadingStackLeadingConstraint.constant != margin) {
    _leadingStackLeadingConstraint.constant = margin;
  }
  if (_trailingStackTrailingConstraint.constant != margin) {
    _trailingStackTrailingConstraint.constant = margin;
  }
}

// Updates the text-only location bar view for fullscreen `progress`.
- (void)updateTextOnlyLocationBarForFullscreenProgress:(CGFloat)progress {
  CHECK(IsGlassToolbarEnabled());
  if (!_textOnlyLocationBarViewController || !_locationBarViewController ||
      !_glassBackgroundView || !_steadyViewLayoutGuide) {
    return;
  }

  CGFloat easedProgress =
      [self glassToolbarProgressForFullscreenProgress:progress];
  UIView* textOnlyView = _textOnlyLocationBarViewController.view;
  UIView* locationBarView = _locationBarViewController.view;

  BOOL showNormalLocationBar = easedProgress >= 1.0;
  textOnlyView.hidden = showNormalLocationBar;
  locationBarView.hidden = !showNormalLocationBar;

  [self updateTextOnlyLocationBarConstraintsForProgress:easedProgress];
}

// Updates the text-only location bar constraints for the eased progress.
- (void)updateTextOnlyLocationBarConstraintsForProgress:(CGFloat)easedProgress {
  CHECK(IsGlassToolbarEnabled());
  if (!_textOnlyLeadingConstraint) {
    return;
  }

  UIView* owningView = _steadyViewLayoutGuide.owningView;
  if (!owningView || _glassBackgroundView.bounds.size.width == 0) {
    return;
  }

  CGRect steadyFrame =
      [_glassBackgroundView convertRect:_steadyViewLayoutGuide.layoutFrame
                               fromView:owningView];
  CGFloat glassWidth = _glassBackgroundView.bounds.size.width;
  CGFloat targetLeading = UseRTLLayout()
                              ? glassWidth - CGRectGetMaxX(steadyFrame)
                              : steadyFrame.origin.x;
  CGFloat targetTrailing = UseRTLLayout()
                               ? -steadyFrame.origin.x
                               : CGRectGetMaxX(steadyFrame) - glassWidth;
  CGFloat targetTop = steadyFrame.origin.y;
  CGFloat targetBottom =
      CGRectGetMaxY(steadyFrame) - _glassBackgroundView.bounds.size.height;

  _textOnlyLeadingConstraint.constant = easedProgress * targetLeading;
  _textOnlyTrailingConstraint.constant = easedProgress * targetTrailing;
  _textOnlyTopConstraint.constant = easedProgress * targetTop;
  _textOnlyBottomConstraint.constant = easedProgress * targetBottom;
}

#pragma mark - TabGroupIndicatorViewDelegate

- (void)tabGroupIndicatorViewVisibilityUpdated:(BOOL)visible {
  _tabGroupIndicatorView.hidden = !visible;
  [self updateSeparatorVisibility];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
  [self.mutator tabGroupIndicatorVisibilityUpdated:visible];
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  if (interaction.view == _tabGridButton) {
    UIMenu* menu = _tabGridButtonMenu;
    if (!menu) {
      return nil;
    }
    return [UIContextMenuConfiguration
        configurationWithIdentifier:nil
                    previewProvider:nil
                     actionProvider:^UIMenu*(
                         NSArray<UIMenuElement*>* suggestedActions) {
                       base::RecordAction(base::UserMetricsAction(
                           "MobileMenuToolbarMenuTriggered"));
                       return menu;
                     }];
  }
  return nil;
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
                               configuration:
                                   (UIContextMenuConfiguration*)configuration
       highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier {
  UIView* view = interaction.view;
  if ([view isKindOfClass:[ToolbarTabGridBadgeButton class]]) {
    ToolbarTabGridBadgeButton* tabGridButton = (ToolbarTabGridBadgeButton*)view;
    UIPreviewParameters* parameters = [[UIPreviewParameters alloc] init];
    parameters.visiblePath = [tabGridButton visiblePath];
    parameters.backgroundColor = self.view.backgroundColor;

    return [[UITargetedPreview alloc] initWithView:view parameters:parameters];
  }
  return nil;
}

#pragma mark - Private

// Updates the background colors of the toolbar view and location bar.
- (void)updateBackgroundColors {
  _locationBarBackground.backgroundColor = [self locationBarBackgroundColor];
  self.view.backgroundColor = [self toolbarBackgroundColor];
}

// Returns the background color for the location bar based on current state.
- (UIColor*)locationBarBackgroundColor {
  // On iPhone, the location bar matches the fakebox color (white or custom
  // palette) when `kNewTabPageUICleanup` is enabled. Otherwise, fallback to the
  // standard location bar background color.
  if (!CanShowTabStrip(self) && _NTPVisible &&
      ShouldApplyFakeboxBackgroundAndShadow()) {
    NewTabPageColorPalette* colorPalette =
        [self.traitCollection objectForNewTabPageTrait];
    return colorPalette ? colorPalette.omniboxColor
                        : [UIColor colorNamed:kSolidWhiteColor];
  }
  return ToolbarElementBackgroundColor(_incognito);
}

// Returns the background color for the toolbar view based on current state.
- (UIColor*)toolbarBackgroundColor {
  if (IsGlassToolbarEnabled()) {
    return [UIColor clearColor];
  }

  if (!CanShowTabStrip(self) && _NTPVisible) {
    NewTabPageColorPalette* colorPalette =
        [self.traitCollection objectForNewTabPageTrait];
    if (colorPalette) {
      return colorPalette.primaryColor;
    } else if ([self.traitCollection boolForNewTabPageImageBackgroundTrait]) {
      return [UIColor colorNamed:kBackgroundColor];
    } else if (IsNewTabPageUICleanupEnabled()) {
      // Matches the updated NTP UI cleanup background color.
      return [UIColor colorNamed:kNewTabPageBackgroundColor];
    } else if (ShouldApplyFakeboxBackgroundAndShadow()) {
      // In light mode, matches the default light blue NTP background
      // color to prevent the white pinned omnibox from blending into a white
      // toolbar. In dark mode, matches standard `kBackgroundColor`.
      if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
        return [UIColor colorNamed:kBackgroundColor];
      }
      return [UIColor colorNamed:kNTPBackgroundColor];
    }
  }
  return [UIColor colorNamed:kBackgroundColor];
}

// Creates and configures a separator line for the toolbar.
- (UIView*)createSeparator {
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.hidden = YES;
  return separator;
}

// Updates the visibility of both the inner and outer separators.
- (void)updateSeparatorVisibility {
  if (IsGlassToolbarEnabled()) {
    _innerSeparator.hidden = YES;
    _outerSeparator.hidden = YES;
    return;
  }

  BOOL tabGroupIndicatorVisible =
      _tabGroupIndicatorView && !_tabGroupIndicatorView.hidden;
  _innerSeparator.hidden =
      !(tabGroupIndicatorVisible || [self isOmniboxVisible]);

  if (!_topPosition) {
    _outerSeparator.hidden = !_locationIndicatorActive;
  }
}

// Helper method to actually do the animation to show the banner promo.
- (void)showBannerPromoAnimationBlock {
  _bannerPromoBackgroundHeightConstraint.constant =
      [self bannerPromoBackgroundHeightForFullscreenProgress:1];
  [self updateVerticalPositionForFullscreenProgress:_fullscreenProgress];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
  [self.view.superview layoutIfNeeded];
}

// Helper method for show completion.
- (void)showBannerPromoCompletionBlock {
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  _bannerPromo);
}

// Helper method to actually do the animation to hide the banner promo.
- (void)hideBannerPromoAnimationBlock {
  if ([self isBannerBelowToolbar]) {
    _bannerPromoBackgroundHeightConstraint.constant = 0;
  } else {
    _bannerPromoBackgroundTopConstraint.constant =
        -[self bannerPromoBackgroundHeightForFullscreenProgress:1];
  }

  _bannerPromoVisible = NO;

  [self updateVerticalPositionForFullscreenProgress:_fullscreenProgress];

  [self.toolbarHeightDelegate toolbarsHeightChanged];
  [self.view.superview layoutIfNeeded];
}

// Helper method for hide completion.
- (void)hideBannerPromoCompletionBlock {
  [NSLayoutConstraint deactivateConstraints:_bannerPromoAboveConstraints];
  [NSLayoutConstraint deactivateConstraints:_bannerPromoBelowConstraints];
  _bannerPromoBackground.alpha = 0;
  [self.view.superview layoutIfNeeded];
}

// Returns whether the banner is displayed below the toolbar.
- (BOOL)isBannerBelowToolbar {
  return !IsSplitToolbarMode(self);
}

// Updates the banner-related constraints.
- (void)updateBannerConstraints {
  if (!_bannerPromoVisible) {
    return;
  }

  if ([self isBannerBelowToolbar]) {
    [NSLayoutConstraint deactivateConstraints:_bannerPromoAboveConstraints];
    [NSLayoutConstraint activateConstraints:_bannerPromoBelowConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:_bannerPromoAboveConstraints];
    [NSLayoutConstraint deactivateConstraints:_bannerPromoBelowConstraints];
  }
}

// Updates the constraints for the text-only location bar.
- (void)updateTextOnlyLocationBarViewConstraints {
  if (_textOnlyLeadingConstraint) {
    [NSLayoutConstraint deactivateConstraints:@[
      _textOnlyLeadingConstraint, _textOnlyTrailingConstraint,
      _textOnlyTopConstraint, _textOnlyBottomConstraint
    ]];
    _textOnlyLeadingConstraint = nil;
    _textOnlyTrailingConstraint = nil;
    _textOnlyTopConstraint = nil;
    _textOnlyBottomConstraint = nil;
  }

  if (!_textOnlyLocationBarViewController || !_steadyViewLayoutGuide ||
      !IsGlassToolbarEnabled()) {
    return;
  }

  UIView* textOnlyView = _textOnlyLocationBarViewController.view;
  _textOnlyLeadingConstraint = [textOnlyView.leadingAnchor
      constraintEqualToAnchor:_glassBackgroundView.leadingAnchor];
  _textOnlyTrailingConstraint = [textOnlyView.trailingAnchor
      constraintEqualToAnchor:_glassBackgroundView.trailingAnchor];
  _textOnlyTopConstraint = [textOnlyView.topAnchor
      constraintEqualToAnchor:_glassBackgroundView.topAnchor];
  _textOnlyBottomConstraint = [textOnlyView.bottomAnchor
      constraintEqualToAnchor:_glassBackgroundView.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    _textOnlyLeadingConstraint, _textOnlyTrailingConstraint,
    _textOnlyTopConstraint, _textOnlyBottomConstraint
  ]];

  [self updateTextOnlyLocationBarForFullscreenProgress:_fullscreenProgress];
}

// Updates the availability of the tab group indicator and its constraints.
- (void)updateTabGroupIndicatorAvailability {
  if (_hasOmnibox) {
    _tabGroupIndicatorInactiveToolbarConstraint.active = NO;
    _tabGroupIndicatorActiveToolbarConstraint.active = YES;
  } else {
    _tabGroupIndicatorActiveToolbarConstraint.active = NO;
    _tabGroupIndicatorInactiveToolbarConstraint.active = YES;
  }

  BOOL canShowTabStrip = CanShowTabStrip(self);
  BOOL isAvailable = !IsCompactHeight(self) && !canShowTabStrip;
  _tabGroupIndicatorView.available = isAvailable;
}

// Sets the NTP scroll progress for the toolbar. The toolbar is revealed as the
// page is scrolled.
- (void)setNTPScrollProgressForToolbar:(CGFloat)progress {
  CHECK(_NTPVisible);
  [self updateToolbarVisibility];
  [self setNTPScrollProgressForOmnibox:progress];
  self.view.alpha = progress;
}

// Sets the NTP scroll progress for the location bar. The location bar in the
// toolbar is revealed with a translation effect as the page is scrolled.
- (void)setNTPScrollProgressForOmnibox:(CGFloat)progress {
  CHECK(_NTPVisible);
  // The vertical distance the location bar translates during the NTP scroll
  // animation. Set to `kToolbarPadding` so the location bar slides exactly down
  // to the bottom edge of the toolbar container as it fades out of view.
  const CGFloat kNTPLocationBarTranslation = kToolbarPadding;

  CGAffineTransform translationTransform =
      (progress == 1.0)
          ? CGAffineTransformIdentity
          : CGAffineTransformMakeTranslation(
                0.0, kNTPLocationBarTranslation * (1.0 - progress));

  if (_locationBarContainer.alpha == progress &&

      CGAffineTransformEqualToTransform(_locationBarContainer.transform,
                                        translationTransform)) {
    // No changes.
    return;
  }

  _locationBarContainer.transform = translationTransform;
  _locationBarContainer.alpha = progress;

  if (!_fakeOmniboxTarget || !CanShowTabStrip(self)) {
    return;
  }

  CHECK(_topPosition);

  // When the location bar is fully hidden, activate the fake omnibox
  // target in its place.
  if (_locationBarContainer.alpha == 0.0 && _fakeOmniboxTarget.isHidden) {
    _fakeOmniboxTarget.hidden = NO;
  } else if (_locationBarContainer.alpha > 0.0 &&
             !_fakeOmniboxTarget.isHidden) {
    _fakeOmniboxTarget.hidden = YES;
  }
}

// Returns whether the a accessory view position should be used.
- (BOOL)useAccessoryViewPosition {
  UIView* inputAccessory = [self.layoutGuideCenter
      referencedViewUnderName:kInputAccessoryViewLayoutGuide];
  return inputAccessory != nil;
}

// Returns the input accessory view height, in window coordinates.
- (CGFloat)inputAccessoryHeightInWindow {
  UIView* inputAccessory = [self.layoutGuideCenter
      referencedViewUnderName:kInputAccessoryViewLayoutGuide];
  CGRect rectInWindow =
      [inputAccessory convertRect:inputAccessory.layer.presentationLayer.frame
                           toView:self.view.window];
  return self.view.window.frame.size.height - rectInWindow.origin.y;
}

// Returns a new background for the location bar.
- (UIView*)createLocationBarBackground {
  UIView* locationBarBackground = [[UIView alloc] init];
  locationBarBackground.translatesAutoresizingMaskIntoConstraints = NO;
  locationBarBackground.layer.cornerRadius = kLocationBarHeight / 2.0;
  locationBarBackground.backgroundColor = [self locationBarBackgroundColor];
  ConfigureShadowForToolbarElement(locationBarBackground);

  __weak UIView* weakLocationBarBackground = locationBarBackground;
  [locationBarBackground
      registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                  withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                UITraitCollection* previousCollection) {
                    ConfigureShadowForToolbarElement(weakLocationBarBackground);
                  }];

  return locationBarBackground;
}

// Returns a new location bar container.
- (UIView*)createLocationBarContainerWithBackground:
    (UIView*)locationBarBackground {
  UIView* locationBarContainer = [[UIView alloc] init];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  [locationBarContainer addSubview:locationBarBackground];
  AddSameConstraints(locationBarContainer, locationBarBackground);

  if (IsGlassToolbarEnabled()) {
    _locationBarContentView = [[UIView alloc] init];
    _locationBarContentView.translatesAutoresizingMaskIntoConstraints = NO;
    _locationBarContentView.layer.cornerRadius = kLocationBarHeight / 2.0;
    _locationBarContentView.clipsToBounds = YES;
    [locationBarContainer addSubview:_locationBarContentView];
    AddSameConstraints(locationBarContainer, _locationBarContentView);
  }

  [locationBarContainer
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [locationBarContainer
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisHorizontal];

  return locationBarContainer;
}

// Creates a target to dismiss fullscreen when the collapsed toolbar is tapped.
- (UIButton*)createCollapsedToolbarButton {
  UIButton* collapsedToolbarButton = [[UIButton alloc] init];
  collapsedToolbarButton.translatesAutoresizingMaskIntoConstraints = NO;
  collapsedToolbarButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_COLLAPSED_PRIMARY_TOOLBAR_BUTTON);
  collapsedToolbarButton.hidden = YES;

  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(collapsedToolbarButtonTapped)];
  [collapsedToolbarButton addGestureRecognizer:tapRecognizer];
  return collapsedToolbarButton;
}

// Called when the collapsed toolbar button is tapped.
- (void)collapsedToolbarButtonTapped {
  [self.mutator exitFullscreen];
  [GetFirstResponder() resignFirstResponder];
}

// Creates a loading progress bar.
- (ToolbarProgressBar*)createProgressBar {
  ToolbarProgressBar* progressBar = [[ToolbarProgressBar alloc] init];
  progressBar.translatesAutoresizingMaskIntoConstraints = NO;
  progressBar.hidden = YES;
  return progressBar;
}

// Creates a container with a given `progressBar`. This allows a
// `ToolbarProgressBar` to be shown and hidden without conflicting with its
// internal visibility logic.
- (UIView*)createContainerForProgressBar:(ToolbarProgressBar*)progressBar {
  UIView* progressBarContainer = [[UIView alloc] init];
  progressBarContainer.translatesAutoresizingMaskIntoConstraints = NO;
  progressBarContainer.hidden = YES;
  [progressBarContainer addSubview:progressBar];
  AddSameConstraints(progressBarContainer, progressBar);
  return progressBarContainer;
}

// Creates the views.
- (void)createView {
  CHECK(self.buttonFactory);
  _locationBarBackground = [self createLocationBarBackground];
  _locationBarContainer =
      [self createLocationBarContainerWithBackground:_locationBarBackground];

  if (_topPosition) {
    _fakeOmniboxTarget = [self createFakeOmniboxTarget];
  }
  _progressBar = [self createProgressBar];
  _progressBarContainer = [self createContainerForProgressBar:_progressBar];
  _collapsedToolbarButton = [self createCollapsedToolbarButton];

  _innerSeparator = [self createSeparator];
  _outerSeparator = [self createSeparator];

  _backButton = [self.buttonFactory makeBackButton];
  _backButton.menu = _backButtonMenu;
  [_backButton addTarget:self
                  action:@selector(backButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton addAction:[UIAction actionWithHandler:^(UIAction*) {
                 base::RecordAction(
                     base::UserMetricsAction("MobileMenuToolbarMenuTriggered"));
                 TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
               }]
        forControlEvents:UIControlEventMenuActionTriggered];
  _forwardButton = [self.buttonFactory makeForwardButton];
  _forwardButton.menu = _forwardButtonMenu;
  [_forwardButton addTarget:self
                     action:@selector(forwardButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton addAction:[UIAction actionWithHandler:^(UIAction*) {
                    base::RecordAction(base::UserMetricsAction(
                        "MobileMenuToolbarMenuTriggered"));
                    TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
                  }]
           forControlEvents:UIControlEventMenuActionTriggered];
  if (!IsNextOldDesignEnabled()) {
    _navigationButtonsContainer =
        [self.buttonFactory makeConjoinedBackButton:_backButton
                                      forwardButton:_forwardButton];
  }
  _reloadButton = [self.buttonFactory makeReloadButton];
  [_reloadButton addTarget:self
                    action:@selector(reloadButtonTapped)
          forControlEvents:UIControlEventTouchUpInside];
  _stopButton = [self.buttonFactory makeStopButton];
  [_stopButton addTarget:self
                  action:@selector(stopButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  _shareButton = [self.buttonFactory makeShareButton];
  [_shareButton addTarget:self
                   action:@selector(shareButtonTapped:)
         forControlEvents:UIControlEventTouchUpInside];
  _assistantButton = [self.buttonFactory makeAssistantButton];
  [_assistantButton addTarget:self
                       action:@selector(assistantButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _tabGridButton = [self.buttonFactory makeTabGridButton];
  [_tabGridButton addTarget:self
                     action:@selector(tabGridTouchDown)
           forControlEvents:UIControlEventTouchDown];
  [_tabGridButton addTarget:self
                     action:@selector(tabGridTouchUp)
           forControlEvents:UIControlEventTouchUpInside];
  [_tabGridButton
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];
  _toolsMenuButton = [self.buttonFactory makeToolsMenuButton];
  [_toolsMenuButton addTarget:self
                       action:@selector(toolsMenuButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
}

// Sets up the banner promo view and its constraints if not already done.
- (void)setUpBannerPromoIfNecessary {
  if (_bannerPromoBackground) {
    return;
  }
  _bannerPromoBackground = [[UIView alloc] init];
  _bannerPromoBackground.translatesAutoresizingMaskIntoConstraints = NO;
  _bannerPromoBackground.backgroundColor =
      [UIColor colorNamed:@"banner_promo_background_color"];
  _bannerPromoBackground.clipsToBounds = YES;
  _bannerPromoBackground.alpha = 0;
  [self.view addSubview:_bannerPromoBackground];

  _bannerPromo = [[BannerPromoView alloc] init];
  _bannerPromo.translatesAutoresizingMaskIntoConstraints = NO;
  [_bannerPromoBackground addSubview:_bannerPromo];

  _bannerPromo.delegate = self.bannerPromoDelegate;
  _bannerPromoVisible = NO;

  _bannerPromoBackgroundHeightConstraint =
      [_bannerPromoBackground.heightAnchor constraintEqualToConstant:0];

  _bannerPromoBackgroundTopConstraint = [_bannerPromoBackground.topAnchor
      constraintEqualToAnchor:self.view.topAnchor];

  _bannerPromoAboveConstraints = @[
    _bannerPromoBackgroundTopConstraint,
    [_bannerPromo.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
  ];

  _bannerPromoBelowConstraints = @[
    [_bannerPromo.topAnchor
        constraintEqualToAnchor:_bannerPromoBackground.topAnchor],
    [_bannerPromoBackground.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ];

  [NSLayoutConstraint activateConstraints:@[
    [_bannerPromoBackground.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_bannerPromoBackground.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    _bannerPromoBackgroundHeightConstraint,

    [_bannerPromo.leadingAnchor
        constraintEqualToAnchor:_bannerPromoBackground.leadingAnchor],
    [_bannerPromo.trailingAnchor
        constraintEqualToAnchor:_bannerPromoBackground.trailingAnchor],
    [_bannerPromo.bottomAnchor
        constraintEqualToAnchor:_bannerPromoBackground.bottomAnchor],
  ]];
}

// Returns the stack view spacing depending on feature flags.
- (CGFloat)stackViewSpacing {
  return IsNextOldDesignEnabled() ? kLegacyStackViewSpacing : kStackViewSpacing;
}

- (UIStackView*)makeStackViewWithButtons:(NSArray<UIView*>*)buttons {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = [self stackViewSpacing];
  return stackView;
}

// Sets up the glass effect container, glass effect view, and its constraints if
// `IsGlassToolbarEnabled()` is true.
- (void)setUpGlassEffectHierarchy {
  if (!IsGlassToolbarEnabled()) {
    return;
  }
  if (@available(iOS 26, *)) {
    _glassBackgroundContainer = [[UIView alloc] init];
    _glassBackgroundContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _glassBackgroundContainer.backgroundColor = [UIColor
        colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
          if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
            return [UIColor colorWithWhite:0
                                     alpha:kGlassContainerDarkBackgroundAlpha];
          }
          return [UIColor clearColor];
        }];

    if (!@available(iOS 27, *)) {
      // The shadow (and thus the container) can be removed on iOS 27.
      _glassBackgroundContainer.layer.shadowColor =
          [UIColor blackColor].CGColor;
      _glassBackgroundContainer.layer.shadowOpacity = kGlassShadowOpacity;
      _glassBackgroundContainer.layer.shadowOffset =
          CGSizeMake(0, kGlassShadowOffsetY);
      _glassBackgroundContainer.layer.shadowRadius = kGlassShadowRadius;
    }

    _glassBackgroundContainer.layer.cornerRadius = kGlassExpandedHeight / 2.0;
    [self.view addSubview:_glassBackgroundContainer];

    UIGlassEffect* glassEffect =
        [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
    _glassBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:glassEffect];
    _glassBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _glassBackgroundView.layer.cornerRadius = kGlassExpandedHeight / 2.0;
    _glassBackgroundView.clipsToBounds = YES;
    [_glassBackgroundContainer addSubview:_glassBackgroundView];
    AddSameConstraints(_glassBackgroundContainer, _glassBackgroundView);

    _glassBackgroundOverlay = [[UIView alloc] init];
    _glassBackgroundOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    [_glassBackgroundView.contentView addSubview:_glassBackgroundOverlay];
    AddSameConstraints(_glassBackgroundView, _glassBackgroundOverlay);

    _glassBackgroundHeightConstraint = [_glassBackgroundContainer.heightAnchor
        constraintEqualToConstant:kGlassExpandedHeight];
    _glassBackgroundBottomConstraint = [_glassBackgroundContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-kGlassToolbarMargin];

    _glassBackgroundTopConstraint = [_glassBackgroundContainer.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:kGlassToolbarMargin];

    if (_topPosition) {
      _glassBackgroundBottomConstraint.active = YES;
    } else {
      _glassBackgroundTopConstraint.active = YES;
    }

    _glassBackgroundLeadingConstraint = [_glassBackgroundContainer.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kGlassToolbarMargin];
    _glassBackgroundTrailingConstraint =
        [_glassBackgroundContainer.trailingAnchor
            constraintEqualToAnchor:self.view.trailingAnchor
                           constant:-kGlassToolbarMargin];

    [NSLayoutConstraint activateConstraints:@[
      _glassBackgroundHeightConstraint,
      _glassBackgroundLeadingConstraint,
      _glassBackgroundTrailingConstraint,
    ]];
  }
}

// Sets up the hierarchy of the buttons.
- (void)setUpHierarchy {
  if (IsNextOldDesignEnabled()) {
    _leadingStackView = [self makeStackViewWithButtons:@[
      _backButton,
      _forwardButton,
      _reloadButton,
      _stopButton,
    ]];
  } else {
    _leadingStackView = [self makeStackViewWithButtons:@[
      _navigationButtonsContainer,
      _reloadButton,
      _stopButton,
    ]];
  }
  _trailingStackView = [self makeStackViewWithButtons:@[
    _shareButton, _assistantButton, _tabGridButton, _toolsMenuButton
  ]];

  if (IsNextOldDesignEnabled()) {
    _allButtons = @[
      _backButton, _forwardButton, _reloadButton, _stopButton, _shareButton,
      _assistantButton, _tabGridButton, _toolsMenuButton
    ];
  } else {
    _allButtons = @[
      _navigationButtonsContainer, _reloadButton, _stopButton, _shareButton,
      _assistantButton, _tabGridButton, _toolsMenuButton
    ];
  }

  [self setUpGlassEffectHierarchy];

  UIView* containerView =
      _glassBackgroundView ? _glassBackgroundView.contentView : self.view;

  [containerView addSubview:_leadingStackView];
  [containerView addSubview:_locationBarContainer];

  if (_fakeOmniboxTarget) {
    [containerView addSubview:_fakeOmniboxTarget];
    AddSameConstraints(_locationBarContainer, _fakeOmniboxTarget);
  }

  [containerView addSubview:_trailingStackView];
  if (IsGlassToolbarEnabled()) {
    [_locationBarContentView addSubview:_progressBarContainer];
    [NSLayoutConstraint activateConstraints:@[
      [_progressBarContainer.leadingAnchor
          constraintEqualToAnchor:_locationBarContentView.leadingAnchor],
      [_progressBarContainer.trailingAnchor
          constraintEqualToAnchor:_locationBarContentView.trailingAnchor],
      [_progressBarContainer.bottomAnchor
          constraintEqualToAnchor:_locationBarContentView.bottomAnchor],
      [_progressBarContainer.heightAnchor
          constraintEqualToConstant:kProgressBarHeight],
    ]];
  } else {
    [self.view addSubview:_progressBarContainer];
    NSLayoutConstraint* progressBarEdgeConstraint =
        _topPosition ? [_progressBarContainer.bottomAnchor
                           constraintEqualToAnchor:self.view.bottomAnchor]
                     : [_progressBarContainer.topAnchor
                           constraintEqualToAnchor:self.view.topAnchor];

    [NSLayoutConstraint activateConstraints:@[
      [_progressBarContainer.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_progressBarContainer.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
      [_progressBarContainer.heightAnchor
          constraintEqualToConstant:kProgressBarHeight],
      progressBarEdgeConstraint
    ]];
  }
  [self.view addSubview:_collapsedToolbarButton];
  AddSameConstraints(self.view, _collapsedToolbarButton);

  [self.view addSubview:_innerSeparator];
  NSLayoutConstraint* innerSeparatorEdgeConstraint =
      _topPosition ? [_innerSeparator.bottomAnchor
                         constraintEqualToAnchor:self.view.bottomAnchor]
                   : [_innerSeparator.topAnchor
                         constraintEqualToAnchor:self.view.topAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_innerSeparator.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_innerSeparator.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_innerSeparator.heightAnchor
        constraintEqualToConstant:AlignValueToUpperPixel(
                                      kToolbarSeparatorHeight)],
    innerSeparatorEdgeConstraint
  ]];

  if (!_topPosition) {
    [self.view addSubview:_outerSeparator];
    [NSLayoutConstraint activateConstraints:@[
      [_outerSeparator.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_outerSeparator.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
      [_outerSeparator.heightAnchor
          constraintEqualToConstant:AlignValueToUpperPixel(
                                        kToolbarSeparatorHeight)],
      [_outerSeparator.topAnchor
          constraintEqualToAnchor:_locationBarContainer.bottomAnchor
                         constant:-kOuterSeparatorVerticalOffset],
    ]];
  }

  _locationBarHeightConstraint = [_locationBarContainer.heightAnchor
      constraintEqualToConstant:kLocationBarHeight];
  _locationBarHeightConstraint.active = YES;

  if (IsGlassToolbarEnabled()) {
    [_locationBarContainer.centerYAnchor
        constraintEqualToAnchor:_glassBackgroundView.centerYAnchor]
        .active = YES;
  } else {
    _locationBarBottomPaddingConstraint = [_locationBarContainer.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-kToolbarPadding];

    _locationBarTopConstraint = [_locationBarContainer.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:kToolbarPadding];

    if (_topPosition) {
      _locationBarBottomPaddingConstraint.active = YES;
    } else {
      _locationBarTopConstraint.active = YES;
    }
  }

  [NSLayoutConstraint activateConstraints:@[
    [_leadingStackView.centerYAnchor
        constraintEqualToAnchor:_locationBarContainer.centerYAnchor],
    [_trailingStackView.centerYAnchor
        constraintEqualToAnchor:_locationBarContainer.centerYAnchor],
  ]];

  NSLayoutConstraint* widthConstraint = [_locationBarContainer.widthAnchor
      constraintEqualToAnchor:self.view.widthAnchor];
  widthConstraint.priority = UILayoutPriorityRequired - 1;

  [_locationBarContainer.widthAnchor
      constraintLessThanOrEqualToConstant:kLocationBarMaxWidth]
      .active = YES;

  if (IsNextOldDesignEnabled()) {
    _leadingStackLeadingConstraint = [_leadingStackView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor];
    _trailingStackTrailingConstraint =
        [self.view.safeAreaLayoutGuide.trailingAnchor
            constraintEqualToAnchor:_trailingStackView.trailingAnchor];
  } else {
    _leadingStackLeadingConstraint = [_leadingStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor];
    _trailingStackTrailingConstraint = [self.view.trailingAnchor
        constraintEqualToAnchor:_trailingStackView.trailingAnchor];
  }
  _leadingStackLeadingConstraint.active = YES;
  _trailingStackTrailingConstraint.active = YES;

  CGFloat locationBarMargin = IsGlassToolbarEnabled()
                                  ? kGlassLocationBarStackViewMarginPortrait
                                  : kLocationBarStackViewMarginPortrait;
  _portraitOrientationConstraints = @[
    [_locationBarContainer.leadingAnchor
        constraintEqualToAnchor:_leadingStackView.trailingAnchor
                       constant:locationBarMargin],
    [_locationBarContainer.trailingAnchor
        constraintEqualToAnchor:_trailingStackView.leadingAnchor
                       constant:-locationBarMargin],
  ];

  CGFloat regularMargin = kLocationBarStackViewMarginRegularRegular;
  _regularRegularConstraints = @[
    [_locationBarContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_leadingStackView.trailingAnchor
                                    constant:regularMargin],
    [_trailingStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_locationBarContainer
                                                 .trailingAnchor
                                    constant:regularMargin],
    [_locationBarContainer.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    widthConstraint,
  ];

  // On iPhone portrait and iPad regular x regular, the location bar is not
  // supposed to move when the forward button appears. So add a constrait to
  // make sure the width of the leading stack view is enough to contain 3
  // buttons and one spacing.
  CGFloat minimalLeadingMargin = kLocationBarStackViewMarginLandscape +
                                 2 * kToolbarButtonSize +
                                 [self stackViewSpacing] + kToolbarButtonSize;
  _landscapeOrientationConstraints = @[
    [_locationBarContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_leadingStackView.trailingAnchor
                                    constant:
                                        kLocationBarStackViewMarginLandscape],
    [_locationBarContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_leadingStackView.leadingAnchor
                                    constant:minimalLeadingMargin],
    [_trailingStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_locationBarContainer
                                                 .trailingAnchor
                                    constant:
                                        kLocationBarStackViewMarginLandscape],
    [_locationBarContainer.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    widthConstraint,
  ];

  [self updateLayoutConstraints];
}

// Updates constraints for the toolbar layout depending on interface
// orientation. In portrait orientation, the location bar fills the space
// between the leading and trailing toolbar buttons. In landscape orientation,
// the location bar has a fixed size in the center of the toolbar.
- (void)updateLayoutConstraints {
  [NSLayoutConstraint deactivateConstraints:_portraitOrientationConstraints];
  [NSLayoutConstraint deactivateConstraints:_landscapeOrientationConstraints];
  [NSLayoutConstraint deactivateConstraints:_regularRegularConstraints];

  if (IsNextOldDesignEnabled()) {
    if (IsRegularXRegularSizeClass(self)) {
      _forwardButton.hidden = NO;
      [NSLayoutConstraint activateConstraints:_regularRegularConstraints];
    } else if (IsIPhoneLandscape(self)) {
      _forwardButton.hidden = !_forwardButton.enabled;
      [NSLayoutConstraint activateConstraints:_landscapeOrientationConstraints];
    } else {
      _forwardButton.hidden = !_forwardButton.enabled;
      [NSLayoutConstraint activateConstraints:_portraitOrientationConstraints];
    }
  } else if (IsRegularXRegularSizeClass(self)) {
    [NSLayoutConstraint activateConstraints:_regularRegularConstraints];
  } else if (IsIPhoneLandscape(self)) {
    [NSLayoutConstraint activateConstraints:_landscapeOrientationConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:_portraitOrientationConstraints];
  }

  // The margins themselves are owned by
  // `-updateStackViewMarginsForFullscreenProgress:`, which folds the fullscreen
  // collapse offset into them. Writing them here too would drop that offset
  // until the next fullscreen progress update.
  [self updateStackViewMarginsForFullscreenProgress:_fullscreenProgress];
}

// Creates a fake omnibox target to activate when the location bar is not
// visible (iPad only).
- (UIView*)createFakeOmniboxTarget {
  UIView* fakeOmniboxTarget = [[UIView alloc] init];
  fakeOmniboxTarget.translatesAutoresizingMaskIntoConstraints = NO;
  fakeOmniboxTarget.hidden = YES;

  UITapGestureRecognizer* tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self.browserCoordinatorHandler
              action:@selector(showComposebox)];
  [fakeOmniboxTarget addGestureRecognizer:tapRecognizer];
  return fakeOmniboxTarget;
}

// Handles back button tap.
- (void)backButtonTapped {
  if (_NTPVisible) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarBackOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarBack"));

  [self.mutator goBack];
}

// Handles forward button tap.
- (void)forwardButtonTapped {
  if (_NTPVisible) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarForwardOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarForward"));

  [self.mutator goForward];
}

// Handles reload button tap.
- (void)reloadButtonTapped {
  if (_NTPVisible) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarReloadOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarReload"));
  [self.mutator reload];
}

// Handles stop button tap.
- (void)stopButtonTapped {
  if (_NTPVisible) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarStopOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarStop"));
  [self.mutator stop];
}

// Handles share button tap.
- (void)shareButtonTapped:(UIView*)sender {
  if (_NTPVisible) {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShareMenuOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarShareMenu"));
  [self.activityServiceHandler showShareSheetFromShareButton:sender];
}

// Handles assistant button tap.
- (void)assistantButtonTapped {
  if (_NTPVisible) {
    base::RecordAction(
        base::UserMetricsAction("MobileToolbarShowAssistantOnNTP"));
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowAssistant"));
  [self.mutator assistantButtonTapped];
}

// Handles tools menu button tap.
- (void)toolsMenuButtonTapped {
  [self.mutator recordUserActionsForToolsMenuTapped];
  [self.popupMenuHandler showToolsMenuPopup];
}

// Handles tab grid button touch down.
- (void)tabGridTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Handles tab grid button touch up.
- (void)tabGridTouchUp {
  if (_NTPVisible) {
    base::RecordAction(
        base::UserMetricsAction("MobileToolbarShowStackViewOnNTP"));
    RecordHomeAction(IOSHomeActionType::kTabSwitcher, _isStartSurface);
  }
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));

  [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

// Returns whether the toolbar should be hidden. The toolbar is typically hidden
// on the regular NTP when scrolled to the top, unless a tab strip is visible.
- (BOOL)shouldHideToolbar {
  BOOL alwaysShowToolbar = CanShowTabStrip(self) && _topPosition;
  if (alwaysShowToolbar) {
    return NO;
  }
  return _NTPVisible && !_incognito && !CanShowTabStrip(self) &&
         _NTPScrollProgress == 0.0;
}

// Updates the visibility of the toolbar.
- (void)updateToolbarVisibility {
  BOOL hideToolbar = [self shouldHideToolbar];

  BOOL alwaysShowToolbar = CanShowTabStrip(self) && _topPosition;
  if (alwaysShowToolbar) {
    self.view.alpha = 1.0;
  }

  [self updateLayoutGuides];

  BOOL visibilityChanged = hideToolbar != self.view.isHidden;

  // While browsing (non-NTP), the toolbar should be reset to be fully visible
  // if it is not already.
  BOOL needsToolbarReset =
      !_NTPVisible &&
      (!CGAffineTransformIsIdentity(_locationBarContainer.transform) ||
       _locationBarContainer.alpha != 1.0 || self.view.alpha != 1.0);

  if (!visibilityChanged && !needsToolbarReset) {
    // No change.
    return;
  }

  BOOL toolbarWillAppear = visibilityChanged && !hideToolbar;

  if (toolbarWillAppear) {
    __weak __typeof(self) weakSelf = self;
    [UIView performWithoutAnimation:^{
      __strong __typeof(self) strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      // When unhiding the toolbar (e.g. when navigating forward from the NTP
      // when the toolbar is hidden), resolve the parent constraints instantly.
      // This prevents a race condition with the toolbar height and prevents
      // visual glitching where the toolbar appears initially out of place.
      [strongSelf applyToolbarVisibility:hideToolbar
                       needsToolbarReset:needsToolbarReset];
      [strongSelf.view.superview layoutIfNeeded];
    }];
  } else {
    [self applyToolbarVisibility:hideToolbar
               needsToolbarReset:needsToolbarReset];
  }
}

// Helper for `-updateToolbarVisibility`. Applies the computed toolbar
// visibility state and resets location bar transforms and transparency if
// needed.
- (void)applyToolbarVisibility:(BOOL)hideToolbar
             needsToolbarReset:(BOOL)needsToolbarReset {
  self.view.hidden = hideToolbar;

  if (needsToolbarReset) {
    _locationBarContainer.transform = CGAffineTransformIdentity;
    _locationBarContainer.alpha = 1.0;
    self.view.alpha = 1.0;
    if (_fakeOmniboxTarget) {
      _fakeOmniboxTarget.hidden = YES;
    }
  }

  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

// Returns whether the toolbar has a visible omnibox.
- (BOOL)isOmniboxVisible {
  if (!_hasOmnibox) {
    return NO;
  }
  return !_locationBarContainer.isHidden && _locationBarContainer.alpha != 0.0;
}

// Updates the visibility of the toolbar elements.
- (void)updateToolbarElementsVisibility {
  _leadingStackView.hidden = !_hasOmnibox;
  _locationBarContainer.hidden = !_hasOmnibox;
  _trailingStackView.hidden = !_hasOmnibox;
  _progressBarContainer.hidden = !_hasOmnibox || CanShowTabStrip(self);
  if (IsGlassToolbarEnabled()) {
    _glassBackgroundContainer.hidden = !_hasOmnibox;
    _glassBackgroundView.hidden = !_hasOmnibox;
  }
  [self updateSeparatorVisibility];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

// Starts or stops the loading progress bar.
- (void)updateProgressBarVisibility {
  if (!_progressBar || !_hasOmnibox) {
    return;
  }

  // Cancel any pending task to hide the progress bar.
  _hideProgressBarClosure.Cancel();

  __weak __typeof(self) weakSelf = self;

  // Start and unhide the progress bar.
  if (_isLoading && (_progressBar.isHidden || _progressBar.alpha < 1.0)) {
    [_progressBar setProgress:0 animated:NO];
    [_progressBar setHidden:NO
                   animated:YES
                 completion:^(BOOL) {
                   [weakSelf updateProgressBarVisibility];
                 }];
  } else if (!_isLoading && !_progressBar.isHidden) {
    // Stop and hide the progress bar.
    __weak ToolbarProgressBar* progressBar = _progressBar;
    [_progressBar setProgress:1 animated:YES];

    _hideProgressBarClosure.Reset(base::BindOnce(^{
      [progressBar setHidden:YES
                    animated:YES
                  completion:^(BOOL) {
                    [weakSelf updateProgressBarVisibility];
                  }];
    }));

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, _hideProgressBarClosure.callback(),
        kProgressBarEndAnimationDuration);
  }
}

// Called when the size class is updated.
- (void)sizeClassDidChange {
  [self updateForFullscreenProgress:_fullscreenProgress];
  [self updateLayoutConstraints];
  [self updateToolbarElementsVisibility];
  [self updateToolbarVisibility];
  [self updateTabGroupIndicatorAvailability];
  [self updateTabSwitcherGuide];
  [self updateBackgroundColors];
  if (_topPosition) {
    [self updateBannerConstraints];
    _bannerPromoBackgroundHeightConstraint.constant = [self
        bannerPromoBackgroundHeightForFullscreenProgress:_fullscreenProgress];
  }
}

// Handles user interface style trait collection changes.
- (void)userInterfaceStyleDidChange {
  [self updateBackgroundColors];
}

// Safely updates a layout guide by either referencing `view` or unreferencing
// it if `hide` is YES and the guide is currently owned by `view`.
- (void)updateGuide:(GuideName*)guide withView:(UIView*)view hide:(BOOL)hide {
  if (hide) {
    if ([self.layoutGuideCenter referencedViewUnderName:guide] == view) {
      [self.layoutGuideCenter referenceView:nil underName:guide];
    }
  } else {
    [self.layoutGuideCenter referenceView:view underName:guide];
  }
}

// Updates the layout guides to point to the buttons in this toolbar.
// This should be called when this toolbar becomes the active visible toolbar.
- (void)updateLayoutGuides {
  if (!_hasOmnibox) {
    return;
  }

  BOOL hideToolbar = [self shouldHideToolbar];

  [self updateGuide:kToolsMenuGuide withView:_toolsMenuButton hide:hideToolbar];
  [self updateGuide:kBackButtonGuide withView:_backButton hide:hideToolbar];
  [self updateGuide:kForwardButtonGuide
           withView:_forwardButton
               hide:hideToolbar];
  [self updateGuide:kShareButtonGuide withView:_shareButton hide:hideToolbar];

  // The assistant button is hidden in non Regular-Regular size classes, but the
  // toolbar button's visibility handler may run after this, so
  // `_assistantButton.hidden` is not yet accurate.
  BOOL hideAssistant = hideToolbar || !IsRegularXRegularSizeClass(self) ||
                       _assistantButton.forceHidden;
  [self updateGuide:kAppBarAssistantButtonGuide
           withView:_assistantButton
               hide:hideAssistant];

  [self updateTabSwitcherGuide];
}

// Conditionally registers the Tab Switcher layout guide.
// It should only be registered to the toolbar if the App Bar is not visible.
- (void)updateTabSwitcherGuide {
  if (!_hasOmnibox || !self.view.window) {
    return;
  }
  if (self.layoutState.appBarPosition == AppBarPosition::kNone) {
    [self.layoutGuideCenter referenceView:_tabGridButton
                                underName:kTabSwitcherGuide];
  }
}

@end
