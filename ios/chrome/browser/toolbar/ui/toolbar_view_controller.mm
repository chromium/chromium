// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/toolbar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/cancelable_callback.h"
#import "base/notimplemented.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/banner_promo_view.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/toolbar_progress_bar.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_view.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"
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
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

constexpr CGFloat kStackViewSpacing = 9;
const base::TimeDelta kBannerPromoAnimationDuration = base::Seconds(0.5);

constexpr CGFloat kButtonMinScale = 0.2;

constexpr CGFloat kAnimationDuration = 0.2f;

constexpr CGFloat kLocationBarToTabGroupMargin = 6;

// The margin for the leading/trailing edges of the stack view.
constexpr CGFloat kStackViewMarginRegularRegular = 16;
constexpr CGFloat kStackViewMarginLandscape = 46;
constexpr CGFloat kStackViewMarginPortrait = 9;

// The margin between the stack views and the location bar.
constexpr CGFloat kLocationBarStackViewMarginRegularRegular = 40;
constexpr CGFloat kLocationBarStackViewMarginPortrait = 9;
constexpr CGFloat kLocationBarStackViewMarginLandscape = 18;

// Max width of the location bar.
constexpr CGFloat kLocationBarMaxWidth = 600;

// The threshold for the fullscreen progress of a collapsed toolbar.
constexpr CGFloat kFullscreenCollapsedThreshold = 0.05;

// Timing to finish the animation of the progress bar before hiding it.
const base::TimeDelta kProgressBarEndAnimationDuration =
    base::Milliseconds(250);

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
  UIView* _navigationButtonsContainer;
  // The stack views that hold the buttons on the leading side.
  UIStackView* _leadingStackView;
  // The container for the location bar, which is transparent.
  UIView* _locationBarContainer;
  // The background for the location bar, which is a pill-shaped view.
  UIView* _locationBarBackground;
  // The target for the fake omnibox, which replaces the location bar when the
  // location bar is not visible.
  UIView* _fakeOmniboxTarget;
  // The stack views that hold the buttons on the trailing side.
  UIStackView* _trailingStackView;

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

  // Whether this toolbar is currently visible.
  /// TODO(crbug.com/493268305): Clean up the animation dismissing the toolbar
  /// when navigating to a page where it is not visible (e.g. the New Tab Page).
  BOOL _visible;

  // Whether this toolbar is in incognito mode.
  BOOL _incognito;

  // Whether the visible page is the NTP.
  BOOL _NTPVisible;

  // Whether the visible page is loading.
  BOOL _isLoading;

  // Used to record the latest fullscreen progress.
  CGFloat _fullscreenProgress;

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
}

- (instancetype)initInIncognito:(BOOL)incognito topPosition:(BOOL)topPosition {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _incognito = incognito;
    _topPosition = topPosition;
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
  _locationBarContainer.hidden = hidden || !_visible;
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
    (UIViewController*)locationBarViewController {
  if (_locationBarViewController == locationBarViewController) {
    return;
  }
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
  [_locationBarContainer addSubview:locationBarView];
  AddSameConstraints(locationBarView, _locationBarContainer);
  [_locationBarViewController didMoveToParentViewController:self];
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
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
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
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateLayoutConstraints];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  _bannerPromoBackgroundHeightConstraint.constant = [self
      bannerPromoBackgroundHeightForFullscreenProgress:_fullscreenProgress];
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
}

- (void)setCanGoBack:(BOOL)canGoBack {
  _backButton.enabled = canGoBack;
}

- (void)setCanGoForward:(BOOL)canGoForward animated:(BOOL)animated {
  if (_forwardButton.enabled == canGoForward) {
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
    [self.view layoutIfNeeded];
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

- (void)setIsLoading:(BOOL)isLoading {
  if (_isLoading == isLoading) {
    return;
  }

  _isLoading = isLoading;
  _reloadButton.forceHidden = isLoading;
  _stopButton.forceHidden = !isLoading;

  if (!_progressBar) {
    return;
  }

  if (_isLoading) {
    [_progressBar setProgress:0 animated:NO];
  }
  [self updateProgressBarVisibility];
}

- (void)setLoadingProgress:(double)progress {
  if (!_progressBar || progress == _progressBar.progress) {
    return;
  }

  BOOL isGoingBackward = progress < _progressBar.progress;
  [_progressBar setProgress:progress
                   animated:!_progressBar.hidden && !isGoingBackward];
}

- (void)setShareEnabled:(BOOL)enabled {
  _shareButton.enabled = enabled;
}

- (void)setVisible:(BOOL)visible {
  if (_visible == visible) {
    return;
  }
  _visible = visible;
  [self loadViewIfNeeded];
  [self updateToolbarElementsVisibility];
  [self updateTabGroupIndicatorAvailability];

  if (_visible) {
    [self.layoutGuideCenter referenceView:_toolsMenuButton
                                underName:kToolsMenuGuide];
    [self.layoutGuideCenter referenceView:_backButton
                                underName:kBackButtonGuide];
    [self.layoutGuideCenter referenceView:_forwardButton
                                underName:kForwardButtonGuide];
    [self.layoutGuideCenter referenceView:_shareButton
                                underName:kShareButtonGuide];

    [self updateTabSwitcherGuide];
  }
}

- (void)setNTPVisible:(BOOL)NTPVisible {
  if (NTPVisible == _NTPVisible) {
    return;
  }
  _NTPVisible = NTPVisible;
  [self updateToolbarVisibility];
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
    _locationBarBottomPaddingConstraint.active = NO;
    _locationBarTopConstraint.active = YES;
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
    _locationBarTopConstraint.active = NO;
    _locationBarBottomPaddingConstraint.active = YES;
    [self.toolbarHeightDelegate secondaryToolbarRemovedFromKeyboard];
    [GetFirstResponder() resignFirstResponder];

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
  _fullscreenProgress = progress;
  CGFloat locationBarExpandedHeight;
  if (ShouldHaveCompactLocationBar(self.traitCollection)) {
    locationBarExpandedHeight = kLocationBarHeight;
  } else {
    locationBarExpandedHeight = kTopLocationBarIPhonePortraitHeight;
  }
  CGFloat locationBarHeight = progress * locationBarExpandedHeight +
                              (1 - progress) * kLocationBarHeightFullscreen;
  _locationBarHeightConstraint.constant = locationBarHeight;
  _locationBarBackground.layer.cornerRadius = locationBarHeight / 2.0;
  _locationBarContainer.layer.cornerRadius = locationBarHeight / 2.0;

  _locationBarBackground.alpha = progress;

  _locationBarBottomPaddingConstraint.constant =
      -[self locationBarBottomPaddingForFullscreenProgress:progress];

  _bannerPromoBackgroundHeightConstraint.constant =
      [self bannerPromoBackgroundHeightForFullscreenProgress:progress];

  [self updateButtons:_leadingStackView.arrangedSubviews
      forFullscreenProgress:progress];
  [self updateButtons:_trailingStackView.arrangedSubviews
      forFullscreenProgress:progress];

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
  CGFloat translation = (progress - 1) * offset;

  CGAffineTransform translationTransform =
      CGAffineTransformMakeTranslation(translation, 0);
  _locationBarContainer.transform = translationTransform;
  _leadingStackView.transform = translationTransform;
  _trailingStackView.transform = translationTransform;

  _collapsedToolbarButton.hidden = progress > kFullscreenCollapsedThreshold;
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
  BOOL tabGroupIndicatorVisible =
      _tabGroupIndicatorView && !_tabGroupIndicatorView.hidden;
  _innerSeparator.hidden = !(tabGroupIndicatorVisible || [self hasOmnibox]);

  if (!_topPosition) {
    _outerSeparator.hidden = !_locationIndicatorActive;
  }
}

// Helper method to actually do the animation to show the banner promo.
- (void)showBannerPromoAnimationBlock {
  _bannerPromoBackgroundHeightConstraint.constant =
      [self bannerPromoBackgroundHeightForFullscreenProgress:1];
  _locationBarBottomPaddingConstraint.constant =
      -[self locationBarBottomPaddingForFullscreenProgress:_fullscreenProgress];
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

  _locationBarBottomPaddingConstraint.constant =
      -[self locationBarBottomPaddingForFullscreenProgress:_fullscreenProgress];

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

// Returns the location bar bottom padding for `progress`.
- (CGFloat)locationBarBottomPaddingForFullscreenProgress:(CGFloat)progress {
  CGFloat locationBarBottomPadding;
  if (ShouldHaveCompactLocationBar(self.traitCollection)) {
    locationBarBottomPadding = kToolbarPadding;
  } else {
    locationBarBottomPadding = kTopToolbarIPhonePortraitPadding;
  }
  if ([self isBannerBelowToolbar]) {
    // When the banner is below the toolbar, always use its height for a
    // progress of 1 as progress is used below.
    locationBarBottomPadding +=
        [self bannerPromoBackgroundHeightForFullscreenProgress:1];
  }
  return progress * locationBarBottomPadding +
         (1 - progress) * kToolbarPaddingFullscreen;
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

// Updates the availability of the tab group indicator and its constraints.
- (void)updateTabGroupIndicatorAvailability {
  if (_visible) {
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

// Updates all the `buttons` according to the fullscreen `progress`.
- (void)updateButtons:(NSArray<UIView*>*)buttons
    forFullscreenProgress:(CGFloat)progress {
  for (UIView* button in buttons) {
    if (button.hidden) {
      button.alpha = 0;
      continue;
    }
    if (progress > 0.99) {
      button.alpha = 1;
      button.transform = CGAffineTransformIdentity;
    } else {
      button.alpha = progress;
      CGFloat scale = progress + (1 - progress) * kButtonMinScale;
      button.transform = CGAffineTransformMakeScale(scale, scale);
    }
  }
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

  locationBarBackground.backgroundColor =
      ToolbarElementBackgroundColor(_incognito);

  ConfigureShadowForToolbarElement(locationBarBackground);

  return locationBarBackground;
}

// Returns a new location bar container.
- (UIView*)createLocationBarContainerWithBackground:
    (UIView*)locationBarBackground {
  UIView* locationBarContainer = [[UIView alloc] init];
  locationBarContainer.translatesAutoresizingMaskIntoConstraints = NO;

  [locationBarContainer addSubview:locationBarBackground];
  AddSameConstraints(locationBarContainer, locationBarBackground);

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

// Creates the views.
- (void)createView {
  CHECK(self.buttonFactory);
  _locationBarBackground = [self createLocationBarBackground];
  _locationBarContainer =
      [self createLocationBarContainerWithBackground:_locationBarBackground];

  if (_topPosition &&
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    _fakeOmniboxTarget = [self createFakeOmniboxTarget];
  }
  _progressBar = [self createProgressBar];
  _collapsedToolbarButton = [self createCollapsedToolbarButton];

  _innerSeparator = [self createSeparator];
  _outerSeparator = [self createSeparator];

  _backButton = [self.buttonFactory makeBackButton];
  _backButton.menu = _backButtonMenu;
  [_backButton addTarget:self
                  action:@selector(backButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton addAction:[UIAction actionWithHandler:^(UIAction*) {
                 TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
               }]
        forControlEvents:UIControlEventMenuActionTriggered];
  _forwardButton = [self.buttonFactory makeForwardButton];
  _forwardButton.menu = _forwardButtonMenu;
  [_forwardButton addTarget:self
                     action:@selector(forwardButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton addAction:[UIAction actionWithHandler:^(UIAction*) {
                    TriggerHapticFeedbackForImpact(UIImpactFeedbackStyleHeavy);
                  }]
           forControlEvents:UIControlEventMenuActionTriggered];
  _navigationButtonsContainer =
      [self.buttonFactory makeConjoinedBackButton:_backButton
                                    forwardButton:_forwardButton];
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

- (UIStackView*)makeStackViewWithButtons:(NSArray<UIView*>*)buttons {
  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:buttons];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.spacing = kStackViewSpacing;
  return stackView;
}

// Sets up the hierarchy of the buttons.
- (void)setUpHierarchy {
  _leadingStackView = [self makeStackViewWithButtons:@[
    _navigationButtonsContainer,
    _reloadButton,
    _stopButton,
  ]];
  _trailingStackView = [self makeStackViewWithButtons:@[
    _shareButton, _assistantButton, _tabGridButton, _toolsMenuButton
  ]];

  [self.view addSubview:_leadingStackView];
  [self.view addSubview:_locationBarContainer];

  if (_fakeOmniboxTarget) {
    [self.view addSubview:_fakeOmniboxTarget];
    AddSameConstraints(_locationBarContainer, _fakeOmniboxTarget);
  }

  [self.view addSubview:_trailingStackView];
  [self.view addSubview:_progressBar];
  [self.view addSubview:_innerSeparator];
  [self.view addSubview:_collapsedToolbarButton];
  AddSameConstraints(self.view, _collapsedToolbarButton);

  NSLayoutConstraint* progressBarEdgeConstraint =
      _topPosition ? [_progressBar.bottomAnchor
                         constraintEqualToAnchor:self.view.bottomAnchor]
                   : [_progressBar.topAnchor
                         constraintEqualToAnchor:self.view.topAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_progressBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_progressBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_progressBar.heightAnchor constraintEqualToConstant:kProgressBarHeight],
    progressBarEdgeConstraint
  ]];

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
        constraintEqualToConstant:ui::AlignValueToUpperPixel(
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
          constraintEqualToConstant:ui::AlignValueToUpperPixel(
                                        kToolbarSeparatorHeight)],
      [_outerSeparator.topAnchor
          constraintEqualToAnchor:_locationBarContainer.bottomAnchor
                         constant:-kOuterSeparatorVerticalOffset],
    ]];
  }

  _locationBarHeightConstraint = [_locationBarContainer.heightAnchor
      constraintEqualToConstant:kLocationBarHeight];
  _locationBarHeightConstraint.active = YES;

  _locationBarBottomPaddingConstraint = [_locationBarContainer.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor
                     constant:-kToolbarPadding];
  _locationBarBottomPaddingConstraint.active = YES;

  _locationBarTopConstraint = [_locationBarContainer.topAnchor
      constraintEqualToAnchor:self.view.topAnchor];

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

  _leadingStackLeadingConstraint = [_leadingStackView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor];
  _leadingStackLeadingConstraint.active = YES;
  _trailingStackTrailingConstraint = [self.view.trailingAnchor
      constraintEqualToAnchor:_trailingStackView.trailingAnchor];
  _trailingStackTrailingConstraint.active = YES;

  _portraitOrientationConstraints = @[
    [_locationBarContainer.leadingAnchor
        constraintEqualToAnchor:_leadingStackView.trailingAnchor
                       constant:kLocationBarStackViewMarginPortrait],
    [_locationBarContainer.trailingAnchor
        constraintEqualToAnchor:_trailingStackView.leadingAnchor
                       constant:-kLocationBarStackViewMarginPortrait],
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
                                 2 * kToolbarButtonSize + kStackViewSpacing +
                                 kToolbarButtonSize;
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

  if (IsRegularXRegularSizeClass(self)) {
    _leadingStackLeadingConstraint.constant = kStackViewMarginRegularRegular;
    _trailingStackTrailingConstraint.constant = kStackViewMarginRegularRegular;
    [NSLayoutConstraint activateConstraints:_regularRegularConstraints];
  } else if (IsIPhoneLandscape(self)) {
    _leadingStackLeadingConstraint.constant = kStackViewMarginLandscape;
    _trailingStackTrailingConstraint.constant = kStackViewMarginLandscape;
    [NSLayoutConstraint activateConstraints:_landscapeOrientationConstraints];
  } else {
    _leadingStackLeadingConstraint.constant = kStackViewMarginPortrait;
    _trailingStackTrailingConstraint.constant = kStackViewMarginPortrait;
    [NSLayoutConstraint activateConstraints:_portraitOrientationConstraints];
  }
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
  [self.mutator goBack];
}

// Handles forward button tap.
- (void)forwardButtonTapped {
  [self.mutator goForward];
}

// Handles reload button tap.
- (void)reloadButtonTapped {
  [self.mutator reload];
}

// Handles stop button tap.
- (void)stopButtonTapped {
  [self.mutator stop];
}

// Handles share button tap.
- (void)shareButtonTapped:(UIView*)sender {
  [self.activityServiceHandler showShareSheetFromShareButton:sender];
}

// Handles assistant button tap.
- (void)assistantButtonTapped {
  [self.mutator assistantButtonTapped];
}

// Handles tools menu button tap.
- (void)toolsMenuButtonTapped {
  [self.popupMenuHandler showToolsMenuPopup];
}

// Handles tab grid button touch down.
- (void)tabGridTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Handles tab grid button touch up.
- (void)tabGridTouchUp {
  [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
}

// Updates the visibility of the toolbar.
- (void)updateToolbarVisibility {
  BOOL hideToolbar;
  BOOL alwaysShowToolbar = CanShowTabStrip(self) && _topPosition;
  if (alwaysShowToolbar) {
    self.view.alpha = 1.0;
    hideToolbar = NO;
  } else {
    hideToolbar = _NTPVisible && !_incognito && !CanShowTabStrip(self) &&
                  _NTPScrollProgress == 0.0;
  }

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

// Returns whether the toolbar has the omnibox.
- (BOOL)hasOmnibox {
  return !_locationBarContainer.isHidden && _locationBarContainer.alpha != 0.0;
}

// Updates the visibility of the toolbar elements.
- (void)updateToolbarElementsVisibility {
  _leadingStackView.hidden = !_visible;
  _locationBarContainer.hidden = !_visible;
  _trailingStackView.hidden = !_visible;
  [self updateSeparatorVisibility];
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

// Starts or stops the loading progress bar.
- (void)updateProgressBarVisibility {
  CHECK(_progressBar);

  if (![self hasOmnibox]) {
    _progressBar.hidden = YES;
    return;
  }

  [self.view layoutIfNeeded];

  // Cancel any pending task to hide the progress bar.
  _hideProgressBarClosure.Cancel();

  __weak __typeof(self) weakSelf = self;

  // Start and unhide the progress bar.
  if (_isLoading && !_NTPVisible && !CanShowTabStrip(self) &&
      (_progressBar.isHidden || _progressBar.alpha < 1.0)) {
    [_progressBar setProgress:0 animated:NO];
    [_progressBar setHidden:NO
                   animated:YES
                 completion:^(BOOL) {
                   [weakSelf updateProgressBarVisibility];
                 }];
  } else if (!_isLoading && !_progressBar.hidden) {
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
  [self updateToolbarVisibility];
  [self updateTabGroupIndicatorAvailability];
  [self updateTabSwitcherGuide];
  if (_topPosition) {
    [self updateBannerConstraints];
    _bannerPromoBackgroundHeightConstraint.constant = [self
        bannerPromoBackgroundHeightForFullscreenProgress:_fullscreenProgress];
  }
}

// Conditionally registers the Tab Switcher layout guide.
// It should only be registered to the toolbar if the App Bar is not visible.
- (void)updateTabSwitcherGuide {
  if (!_visible || !self.view.window) {
    return;
  }
  if (self.layoutState.appBarPosition == AppBarPosition::kNone) {
    [self.layoutGuideCenter referenceView:_tabGridButton
                                underName:kTabSwitcherGuide];
  }
}

@end
