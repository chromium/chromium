// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import <CoreGraphics/CoreGraphics.h>

#import <optional>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_background_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_iph_background_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The font size for the tab count label.
constexpr CGFloat kTabGridFontSize = 11;
// The size of the button images.
constexpr CGFloat kButtonImageSize = 23;
// The padding between the image and the text in the buttons.
constexpr CGFloat kButtonImagePadding = 3;
// The shadow radius for the buttons.
constexpr CGFloat kButtonShadowRadius = 3;
// The shadow opacity for the buttons.
constexpr CGFloat kButtonShadowOpacity = 0.2;
// The shadow offset for the buttons.
constexpr CGFloat kButtonShadowOffset = 1;
// The duration of the animation to update the TabGrid button.
constexpr CGFloat kTabGridAnimationDuration = 0.25;
// Spacing between tab grid button and the tab grid spotlight view anchor.
constexpr CGFloat kSpotlightViewHorizontalInset = 12;
constexpr CGFloat kSpotlightViewVerticalInset = 2;
// Offset of the tab count label in the tab grid button tab group state.
constexpr CGFloat kTabGroupLabelOffset = 3;

// The size of the assistant button highlight.
constexpr CGFloat kAssistantHighlightWidth = 44;
constexpr CGFloat kAssistantHighlightHeight = 30;

// The spacing inside the stack view.
constexpr CGFloat kStackViewSpacing = 4;
// The horizontal margins of the stack view.
constexpr CGFloat kStackViewHorizontalMargin = 8;
// The vertical offset of the stack view in portrait.
constexpr CGFloat kStackViewPortraitVerticalOffset = 2;

// The inner padding of the buttons.
constexpr CGFloat kButtonHorizontalPadding = 4;
constexpr CGFloat kButtonVerticalPadding = 12;

// Duration of the IPH show/hide animation.
constexpr CGFloat kIPHAnimationDuration = 0.3;

// Returns the color to be used as foreground color for the buttons.
UIColor* ButtonsForegroundColor() {
  return UIColor.whiteColor;
}

// Returns the configuration for all the symbols.
UIImageSymbolConfiguration* AppBarSymbolConfiguration() {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:kButtonImageSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
}

// Returns a default symbol with the common configuration.
UIImage* DefaultAppBarSymbol(NSString* symbol_name) {
  return DefaultSymbolWithConfiguration(symbol_name,
                                        AppBarSymbolConfiguration());
}

// Returns a custom symbol with the common configuration.
UIImage* CustomAppBarSymbol(NSString* symbol_name) {
  return CustomSymbolWithConfiguration(symbol_name,
                                       AppBarSymbolConfiguration());
}

// Returns the font size for the assistant button.
UIFont* AssistantButtonFontSize(UITraitCollection* traitCollection) {
  return PreferredFontForTextStyle(UIFontTextStyleCaption2, UIFontWeightMedium,
                                   std::nullopt);
}

// Returns the alpha for the button based on its enabled and highlighted state.
CGFloat ButtonHighlightAlpha(UIButton* button) {
  BOOL useEnabledColor = button.enabled && !button.isHighlighted;
  return useEnabledColor ? 1.0 : 0.5;
}

}  // namespace

@interface AppBarViewController () <AppBarViewDelegate,
                                    LayoutStateObserver,
                                    UIContextMenuInteractionDelegate>
@end

@implementation AppBarViewController {
  UIButton* _assistantButton;
  UIButton* _openNewTabButton;
  UIButton* _tabGridButton;
  UIImageView* _tabGridSymbolView;
  UILabel* _tabCountLabel;
  NSUInteger _tabCount;
  // Whether the Tab Grid is currently visible.
  BOOL _isTabGridVisible;
  // Whether the tab groups page in the tab grid is currently visible.
  BOOL _isTabGroupsPageVisible;
  // Whether a tab group is currently being shown in the tab grid.
  BOOL _isTabGroupVisible;
  // Whether the current tab is in a tab group.
  BOOL _inTabGroup;
  // Context menus for the App Bar buttons.
  UIMenu* _assistantButtonMenu;
  UIMenu* _openNewTabButtonMenu;
  UIMenu* _tabGridButtonMenu;
  UIView* _spotlightView;
  // The positioning constraints for the tab count label in the normal tab grid
  // button state.
  NSArray<NSLayoutConstraint*>* _tabGridButtonNormalStateConstraints;
  // The positioning constraints for the tab count label in the tab group tab
  // grid button state.
  NSArray<NSLayoutConstraint*>* _tabGridButtonTabGroupStateConstraints;
  // Cached state for the assistant button.
  AppBarAssistantButtonState _assistantButtonState;
  // Whether the assistant button is highlighted.
  BOOL _assistantButtonHighlighted;
  // Cached avatar for the assistant button.
  UIImage* _assistantButtonAvatar;
  // The highlight view for the assistant button.
  UIView* _assistantHighlightView;
  // Constraints for the assistant highlight view.
  NSArray<NSLayoutConstraint*>* _assistantHighlightConstraints;
  // The background view.
  AppBarBackgroundView* _backgroundView;
  // Whether the app bar is in incognito mode.
  BOOL _incognito;
  // Whether the buttons are enabled.
  BOOL _buttonsEnabled;
  // Whether the assistant button is enabled.
  BOOL _assistantButtonEnabled;
  // Whether the user is signed in.
  BOOL _signedIn;
  // Container view for the Tab Grid button's custom preview.
  UIView* _tabGridContentView;
  // The alpha for the titles of the buttons.
  CGFloat _buttonsTitleAlpha;
  // The fullscreen progress.
  CGFloat _fullscreenProgress;
  // Background view for the IPH.
  AppBarIPHBackgroundView* _IPHBackgroundView;
  // Whether the App Bar content is rotated.
  BOOL _isRotated;
  // The current rotation angle.
  CGFloat _angle;
  // Constraints to make buttons square in landscape so that long press
  // animation does not leak beyond bounds of app bar.
  NSArray<NSLayoutConstraint*>* _buttonWidthConstraints;
  // Stack view for buttons.
  UIStackView* _stackView;
  // Constraints for vertical positioning of the stack view.
  NSLayoutConstraint* _stackViewBottomConstraint;
  // Spacers to for button layout in landscape.
  UIView* _leadingSpacer;
  UIView* _trailingSpacer;
  // The button currently being previewed by a context menu.
  __weak UIButton* _previewedButton;
}

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
}

#pragma mark - LayoutStateObserver

- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  // Update the alpha with a duration of 0 as it is already in an animation
  // block.
  [self setButtonsTitleAlpha:_fullscreenProgress animationDuration:0];
  [self updateTabSwitcherGuide];
}

#pragma mark - Accessors & Mutators

- (void)setButtonsTitleAlpha:(CGFloat)buttonsTitleAlpha
           animationDuration:(NSTimeInterval)duration {
  AppBarPosition appBarPosition = self.layoutState.appBarPosition;

  CGFloat targetAlpha = 1;
  if (appBarPosition == AppBarPosition::kBottom) {
    targetAlpha = buttonsTitleAlpha;
  } else if (appBarPosition == AppBarPosition::kLeft ||
             appBarPosition == AppBarPosition::kRight) {
    targetAlpha = 0;
  }

  if (targetAlpha == _buttonsTitleAlpha) {
    return;
  }
  _buttonsTitleAlpha = targetAlpha;
  [self setNeedsUpdateConfiguration:_assistantButton
                  animationDuration:duration];
  [self setNeedsUpdateConfiguration:_openNewTabButton
                  animationDuration:duration];
  [self setNeedsUpdateConfiguration:_tabGridButton animationDuration:duration];
}

#pragma mark - Public

- (void)updateForAngle:(CGFloat)angle {
  [self loadViewIfNeeded];

  if (_angle == angle) {
    return;
  }
  _angle = angle;

  _isRotated = (angle != 0);

  CGAffineTransform transform = CGAffineTransformMakeRotation(angle);
  _assistantButton.transform = transform;
  _openNewTabButton.transform = transform;
  _tabGridButton.transform = transform;

  if (_isRotated) {
    _stackView.distribution = UIStackViewDistributionEqualSpacing;
    [NSLayoutConstraint activateConstraints:_buttonWidthConstraints];
    _leadingSpacer.hidden = NO;
    _trailingSpacer.hidden = NO;
    _stackViewBottomConstraint.constant =
        -(kAppBarHeight - kAppBarHeightLandscape);
  } else {
    _stackView.distribution = UIStackViewDistributionFillEqually;
    [NSLayoutConstraint deactivateConstraints:_buttonWidthConstraints];
    _leadingSpacer.hidden = YES;
    _trailingSpacer.hidden = YES;
    _stackViewBottomConstraint.constant = 0;
  }
  [_stackView setNeedsLayout];
  [_stackView layoutIfNeeded];
}

- (void)toggleSpotlightView:(BOOL)shouldShow {
  CHECK(IsBestOfAppGuidedTourEnabled());
  _spotlightView.hidden = !shouldShow;
}

- (void)showIPHBackgroundWithCentering:(BOOL)centered {
  if (!_IPHBackgroundView) {
    _IPHBackgroundView = [[AppBarIPHBackgroundView alloc] init];
    _IPHBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    _IPHBackgroundView.alpha = 0;
    [_backgroundView insertSubview:_IPHBackgroundView atIndex:0];

    AddSameConstraints(_backgroundView, _IPHBackgroundView);
  }

  _IPHBackgroundView.centered = centered;

  UIView* background = _IPHBackgroundView;

  [UIView animateWithDuration:kIPHAnimationDuration
                   animations:^{
                     background.alpha = 1.0;
                   }];
}

- (void)hideIPHBackground {
  UIView* background = _IPHBackgroundView;
  [UIView animateWithDuration:kIPHAnimationDuration
                   animations:^{
                     background.alpha = 0.0;
                   }];
}

#pragma mark - UIViewController

- (void)loadView {
  AppBarView* view = [[AppBarView alloc] init];
  view.delegate = self;
  self.view = view;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _angle = CGFLOAT_MAX;
  _backgroundView = [[AppBarBackgroundView alloc] init];
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _backgroundView.incognito = _incognito;
  [self.view insertSubview:_backgroundView atIndex:0];

  _buttonsTitleAlpha = 1;
  _buttonsEnabled = YES;
  _assistantButtonEnabled = YES;
  _fullscreenProgress = 1;

  _assistantButton = [self createAssistantButton];
  _openNewTabButton = [self createOpenNewTabButton];
  _tabGridButton = [self createTabGridButton];
  [self updateTabGridButtonForTabGridVisibility];
  [self updateNewTabButtonAccessibilityLabel];
  [self updateNewTabButtonAccessibilityHint];
  [self updateTabSwitcherGuide];

  // When rotated in landscape, add spacers at the beginning and end of the
  // stack view so that the buttons width match the "height" of the stack view,
  // thus not leaking outside of the stack view's frame during the long press
  // animation.
  _leadingSpacer = [[UIView alloc] init];
  _trailingSpacer = [[UIView alloc] init];
  _leadingSpacer.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingSpacer.translatesAutoresizingMaskIntoConstraints = NO;
  _leadingSpacer.hidden = YES;
  _trailingSpacer.hidden = YES;

  _stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _leadingSpacer, _assistantButton, _openNewTabButton, _tabGridButton,
    _trailingSpacer
  ]];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.distribution = UIStackViewDistributionFillEqually;
  _stackView.spacing = kStackViewSpacing;
  _stackView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;

  _buttonWidthConstraints = @[
    [_assistantButton.widthAnchor
        constraintEqualToAnchor:_stackView.heightAnchor],
    [_openNewTabButton.widthAnchor
        constraintEqualToAnchor:_stackView.heightAnchor],
    [_tabGridButton.widthAnchor constraintEqualToAnchor:_stackView.heightAnchor]
  ];

  UIView* view = self.view;
  [view addSubview:_stackView];

  _stackViewBottomConstraint =
      [_stackView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_backgroundView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_backgroundView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_backgroundView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [_backgroundView.topAnchor constraintEqualToAnchor:view.topAnchor
                                              constant:-kAppBarCornerRadius],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor
                       constant:kStackViewHorizontalMargin],
    [_stackView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor
                       constant:-kStackViewHorizontalMargin],
    _stackViewBottomConstraint,
    [view.heightAnchor constraintEqualToConstant:kAppBarHeight],
  ]];

  [self.layoutGuideCenter referenceView:_stackView underName:kAppBarGuide];
  [self.layoutGuideCenter referenceView:_assistantButton
                              underName:kAppBarAssistantButtonGuide];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateAssistantButtonTitleIfNeeded];
  [self updateTabGridButtonTitleIfNeeded];
  [self updateOpenNewTabButtonTitleIfNeeded];
}

#pragma mark - AppBarConsumer

- (void)updateTabCount:(NSUInteger)count {
  _tabCount = count;
  _tabCountLabel.attributedText = TextForTabCount(count, kTabGridFontSize);
  _tabGridButton.accessibilityValue = [NSString stringWithFormat:@"%lu", count];
}

- (void)setTabGridVisible:(BOOL)tabGridVisible {
  if (_isTabGridVisible == tabGridVisible) {
    return;
  }
  _isTabGridVisible = tabGridVisible;
  _backgroundView.hideColorBackground = tabGridVisible;
  [self updateTabGridButtonForTabGridVisibility];
  [self updateNewTabButtonForTabGroupsVisibility];
  [self updateNewTabButtonAccessibilityLabel];
  [self updateNewTabButtonAccessibilityHint];
}

- (void)setIncognito:(BOOL)incognito {
  if (_incognito == incognito) {
    return;
  }
  _incognito = incognito;
  _backgroundView.incognito = incognito;
  [self updateNewTabButtonAccessibilityLabel];
  [self updateAssistantButton];
}

- (void)setInTabGroup:(BOOL)inTabGroup {
  if (_inTabGroup == inTabGroup) {
    return;
  }
  _inTabGroup = inTabGroup;
  [self updateTabGridButtonForTabGridVisibility];
}

- (void)setMenu:(UIMenu*)menu forButtonType:(AppBarButtonType)buttonType {
  switch (buttonType) {
    case AppBarButtonTypeAssistant:
      _assistantButtonMenu = menu;
      return;
    case AppBarButtonTypeNewTab:
      _openNewTabButtonMenu = menu;
      [self updateNewTabButtonForTabGroupsVisibility];
      return;
    case AppBarButtonTypeTabGrid:
      _tabGridButtonMenu = menu;
      return;
  }
  NOTREACHED();
}

- (void)setAssistantButtonState:(AppBarAssistantButtonState)state
                    highlighted:(BOOL)highlighted
                        enabled:(BOOL)enabled
                         avatar:(UIImage*)avatar
                       signedIn:(BOOL)signedIn {
  _assistantButtonState = state;
  _assistantButtonHighlighted = highlighted;
  _assistantButtonEnabled = enabled;
  _assistantButtonAvatar = avatar;
  _signedIn = signedIn;

  [self updateAssistantButton];
}

- (void)setTabGroupsPageVisible:(BOOL)tabGroupsPageVisible {
  if (tabGroupsPageVisible == _isTabGroupsPageVisible) {
    return;
  }
  _isTabGroupsPageVisible = tabGroupsPageVisible;
  [self updateNewTabButtonForTabGroupsVisibility];
  [self updateNewTabButtonAccessibilityLabel];
}

- (void)setTabGroupVisible:(BOOL)tabGroupVisible {
  if (tabGroupVisible == _isTabGroupVisible) {
    return;
  }
  _isTabGroupVisible = tabGroupVisible;
  [self updateNewTabButtonForTabGroupsVisibility];
  [self updateTabGridButtonForTabGridVisibility];
  [self updateNewTabButtonAccessibilityLabel];
}

- (void)setButtonsEnabled:(BOOL)enabled {
  _buttonsEnabled = enabled;
  _openNewTabButton.enabled = enabled;
  _tabGridButton.enabled = enabled;
  [self updateAssistantButton];
}

#pragma mark - AppBarViewDelegate

- (void)appBarViewDidMoveToWindow:(AppBarView*)view {
  [self updateTabSwitcherGuide];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _fullscreenProgress = progress;
  [self setButtonsTitleAlpha:_fullscreenProgress animationDuration:0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  [self setButtonsTitleAlpha:animator.finalProgress
           animationDuration:animator.duration];
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  _fullscreenProgress = agent->bottom_progress();
  [self setButtonsTitleAlpha:_fullscreenProgress
           animationDuration:agent->animation_duration().InSecondsF()];
}

#pragma mark - Private

// Conditionally registers the Tab Switcher layout guide.
// It should only be registered to the App Bar if the App Bar is visible.
- (void)updateTabSwitcherGuide {
  if (!self.view.window) {
    return;
  }
  if (self.layoutState.appBarPosition == AppBarPosition::kNone) {
    [self.layoutGuideCenter referenceView:nil underName:kTabSwitcherGuide];
  } else {
    [self.layoutGuideCenter referenceView:_tabGridButton
                                underName:kTabSwitcherGuide];
  }
}

// Returns `fullTitle` if it fits within the available width for the
// buttons, or `truncatedTitle` otherwise.
- (NSString*)buttonTitleWithFullTitle:(NSString*)fullTitle
                       truncatedTitle:(NSString*)truncatedTitle {
  if (self.view.bounds.size.width == 0) {
    return fullTitle;
  }
  CGSize size = [fullTitle sizeWithAttributes:@{
    NSFontAttributeName : AssistantButtonFontSize(self.traitCollection)
  }];

  CGFloat availableWidthForButton;
  if (_isRotated) {
    availableWidthForButton = self.view.bounds.size.height;
  } else {
    availableWidthForButton =
        (self.view.bounds.size.width - 2 * kStackViewHorizontalMargin -
         2 * kStackViewSpacing) /
        3.0;
  }

  CGFloat availableWidthForTitle =
      availableWidthForButton - 2 * kButtonHorizontalPadding;

  return (size.width > availableWidthForTitle) ? truncatedTitle : fullTitle;
}

// Returns the title for the assistant button based on current state and size.
- (NSString*)assistantButtonTitleForCurrentState {
  switch (_assistantButtonState) {
    case AppBarAssistantButtonState::kAsk:
      return [self
          buttonTitleWithFullTitle:l10n_util::GetNSString(
                                       IDS_IOS_APP_BAR_ASK_GEMINI)
                    truncatedTitle:l10n_util::GetNSString(IDS_IOS_APP_BAR_ASK)];
    case AppBarAssistantButtonState::kAIM:
      return l10n_util::GetNSString(IDS_OMNIBOX_AI_MODE_SCOPE_PLACEHOLDER_TEXT);
    case AppBarAssistantButtonState::kAccount:
      return _signedIn ? l10n_util::GetNSString(IDS_IOS_APP_BAR_ACCOUNT)
                       : l10n_util::GetNSString(IDS_IOS_APP_BAR_SIGN_IN);
  }
}

// Updates the assistant button title if it has changed.
- (void)updateAssistantButtonTitleIfNeeded {
  if (!_assistantButton) {
    return;
  }
  NSString* title = [self assistantButtonTitleForCurrentState];
  if (![_assistantButton.configuration.title isEqualToString:title]) {
    UIButtonConfiguration* configuration = _assistantButton.configuration;
    configuration.title = title;
    _assistantButton.configuration = configuration;
  }
}

// Returns the title for the Tab Grid button based on size.
- (NSString*)tabGridButtonTitleForCurrentState {
  return [self
      buttonTitleWithFullTitle:l10n_util::GetNSString(IDS_IOS_APP_BAR_ALL_TABS)
                truncatedTitle:l10n_util::GetNSString(IDS_IOS_APP_BAR_TABS)];
}

// Updates the Tab Grid button title if it has changed.
- (void)updateTabGridButtonTitleIfNeeded {
  if (!_tabGridButton) {
    return;
  }
  NSString* title = [self tabGridButtonTitleForCurrentState];
  if (![_tabGridButton.configuration.title isEqualToString:title]) {
    UIButtonConfiguration* configuration = _tabGridButton.configuration;
    configuration.title = title;
    _tabGridButton.configuration = configuration;
  }
}

// Returns the title for the Open New Tab button based on size.
- (NSString*)openNewTabButtonTitleForCurrentState {
  return [self
      buttonTitleWithFullTitle:l10n_util::GetNSString(
                                   IDS_IOS_TOOLS_MENU_NEW_TAB)
                truncatedTitle:l10n_util::GetNSString(IDS_IOS_APP_BAR_NEW)];
}

// Updates the Open New Tab button title if it has changed.
- (void)updateOpenNewTabButtonTitleIfNeeded {
  if (!_openNewTabButton) {
    return;
  }
  NSString* title = [self openNewTabButtonTitleForCurrentState];
  if (![_openNewTabButton.configuration.title isEqualToString:title]) {
    UIButtonConfiguration* configuration = _openNewTabButton.configuration;
    configuration.title = title;
    _openNewTabButton.configuration = configuration;
  }
}

// Updates the assistant button configuration based on the current state.
- (void)updateAssistantButton {
  if (!_assistantButton) {
    return;
  }

  NSString* title = [self assistantButtonTitleForCurrentState];
  UIImage* image;
  switch (_assistantButtonState) {
    case AppBarAssistantButtonState::kAsk:
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      image = CustomAppBarSymbol(kGeminiBrandedLogoSymbol);
#else
      image = DefaultAppBarSymbol(kGeminiNonBrandedLogoSymbol);
#endif
      break;
    case AppBarAssistantButtonState::kAIM:
      image = CustomAppBarSymbol(kMagnifyingglassSparkSymbol);
      break;
    case AppBarAssistantButtonState::kAccount:
      image =
          _assistantButtonAvatar
              ? [_assistantButtonAvatar
                    imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal]
              : DefaultAppBarSymbol(kPersonCropCircleSymbol);
      break;
  }

  UIButtonConfiguration* configuration = _assistantButton.configuration;
  configuration.title = title;
  configuration.image = image ? image : CustomAppBarSymbol(kCameraLensSymbol);

  // Set up custom background view if not already done
  if (_assistantButtonHighlighted && !configuration.background.customView) {
    UIView* customBackgroundView = [[UIView alloc] init];
    customBackgroundView.backgroundColor = [UIColor clearColor];

    _assistantHighlightView = [[UIView alloc] init];
    _assistantHighlightView.translatesAutoresizingMaskIntoConstraints = NO;
    _assistantHighlightView.backgroundColor = [UIColor colorWithWhite:1.0
                                                                alpha:0.15];
    _assistantHighlightView.layer.cornerRadius =
        kAssistantHighlightHeight / 2.0;
    _assistantHighlightView.layer.masksToBounds = YES;
    _assistantHighlightView.hidden = YES;

    [customBackgroundView addSubview:_assistantHighlightView];
    configuration.background.backgroundColor = [UIColor clearColor];
    configuration.background.customView = customBackgroundView;
  }
  _assistantHighlightView.hidden = !_assistantButtonHighlighted;

  if (_assistantButtonHighlighted) {
    configuration.baseForegroundColor = [UIColor whiteColor];
  } else {
    configuration.baseForegroundColor = ButtonsForegroundColor();
  }

  _assistantButton.configuration = configuration;

  // Update constraints to point to the current imageView
  if (_assistantHighlightConstraints) {
    [NSLayoutConstraint deactivateConstraints:_assistantHighlightConstraints];
    _assistantHighlightConstraints = nil;
  }

  if (_assistantHighlightView && _assistantButton.imageView) {
    _assistantHighlightConstraints = @[
      [_assistantHighlightView.centerXAnchor
          constraintEqualToAnchor:_assistantButton.imageView.centerXAnchor],
      [_assistantHighlightView.centerYAnchor
          constraintEqualToAnchor:_assistantButton.imageView.centerYAnchor],
      [_assistantHighlightView.widthAnchor
          constraintEqualToConstant:kAssistantHighlightWidth],
      [_assistantHighlightView.heightAnchor
          constraintEqualToConstant:kAssistantHighlightHeight],
    ];
    [NSLayoutConstraint activateConstraints:_assistantHighlightConstraints];
  }

  _assistantButton.enabled =
      _buttonsEnabled && _assistantButtonEnabled && !_incognito;
}

// Returns a new "Assistant" button.
- (UIButton*)createAssistantButton {
  UIButton* button = [self buttonWithTitle:nil image:nil];

  [button addTarget:self
                action:@selector(didTapAssistantButton)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kAppBarAssistantButtonId;

  _assistantButton = button;
  [self updateAssistantButton];

  [button
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];

  return button;
}

// Returns a new "New Tab" button.
- (UIButton*)createOpenNewTabButton {
  NSString* title = [self openNewTabButtonTitleForCurrentState];
  UIImage* image = DefaultAppBarSymbol(kPlusInCircleSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];
  button.accessibilityIdentifier = kAppBarNewTabButtonIdentifier;

  [button addTarget:self
                action:@selector(didTapOpenNewTabButton:)
      forControlEvents:UIControlEventTouchUpInside];

  [button
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];

  return button;
}

// Updates the title configuration for buttons.
- (void)updateTitleAlphaForButton:(UIButton*)button
                   highlightAlpha:(CGFloat)highlightAlpha {
  // Text fades on highlight/disabled AND scroll.
  CGFloat targetAlpha = (button == _previewedButton) ? 1.0 : _buttonsTitleAlpha;
  CGFloat textAlpha = highlightAlpha * targetAlpha;

  if (self.layoutState.appBarPosition == AppBarPosition::kLeft ||
      self.layoutState.appBarPosition == AppBarPosition::kRight) {
    // Even if the button is highlighted, the text should be hidden.
    textAlpha = 0;
  }

  button.titleLabel.alpha = textAlpha;
}

// Updates the vertical content insets of a button configuration based on the
// current orientation.
- (void)updateVerticalInsetsForButtonConfiguration:
    (UIButtonConfiguration*)config {
  BOOL portrait = !_isRotated;
  CGFloat topInset =
      portrait ? (kButtonVerticalPadding - kStackViewPortraitVerticalOffset)
               : kButtonVerticalPadding;
  CGFloat bottomInset =
      portrait ? (kButtonVerticalPadding + kStackViewPortraitVerticalOffset)
               : kButtonVerticalPadding;
  config.contentInsets =
      NSDirectionalEdgeInsetsMake(topInset, kButtonHorizontalPadding,
                                  bottomInset, kButtonHorizontalPadding);
}

// Updates the configuration for standard buttons.
- (void)updateStandardButtonConfiguration:(UIButton*)button {
  UIButtonConfiguration* config = button.configuration;
  CGFloat highlightAlpha = ButtonHighlightAlpha(button);

  BOOL isAssistantButtonHighlighted =
      (button == _assistantButton && _assistantButtonHighlighted);

  CGFloat activeAlpha = isAssistantButtonHighlighted ? 1.0 : highlightAlpha;

  BOOL isAssistantButtonWithAvatar =
      (button == _assistantButton &&
       _assistantButtonState == AppBarAssistantButtonState::kAccount &&
       _assistantButtonAvatar != nil);
  if (isAssistantButtonWithAvatar) {
    config.imageColorTransformer = nil;
  } else {
    config.imageColorTransformer = ^UIColor*(UIColor* color) {
      UIColor* baseColor = isAssistantButtonHighlighted
                               ? [UIColor whiteColor]
                               : ButtonsForegroundColor();
      return [baseColor colorWithAlphaComponent:activeAlpha];
    };
  }

  [self updateTitleAlphaForButton:button highlightAlpha:activeAlpha];

  [self updateVerticalInsetsForButtonConfiguration:config];

  button.configuration = config;
}

// Updates the configuration for the tab grid button.
- (void)updateTabGridButtonConfiguration:(UIButton*)button
                              symbolView:(UIImageView*)symbolView
                              countLabel:(UILabel*)countLabel {
  UIButtonConfiguration* config = button.configuration;
  // Keep image clear as set in createTabGridButton.
  config.imageColorTransformer = ^UIColor*(UIColor* color) {
    return UIColor.clearColor;
  };

  CGFloat highlightAlpha = ButtonHighlightAlpha(button);

  [self updateTitleAlphaForButton:button highlightAlpha:highlightAlpha];

  UIColor* symbolColor = ButtonsForegroundColor();
  UIColor* baseLabelColor =
      _isTabGridVisible ? UIColor.blackColor : ButtonsForegroundColor();

  symbolView.tintColor = [symbolColor colorWithAlphaComponent:highlightAlpha];
  countLabel.textColor =
      [baseLabelColor colorWithAlphaComponent:highlightAlpha];

  [self updateVerticalInsetsForButtonConfiguration:config];

  button.configuration = config;
}

// Returns a new "TabGrid" button.
- (UIButton*)createTabGridButton {
  // Use a custom Symbol and Label instead of the ones from the button to be
  // able to modify them as necessary.
  UIImageView* tabGridSymbolView = [[UIImageView alloc] init];
  tabGridSymbolView.translatesAutoresizingMaskIntoConstraints = NO;
  tabGridSymbolView.image = DefaultAppBarSymbol(kAppSymbol);
  _tabGridSymbolView = tabGridSymbolView;

  // Set up button.
  NSString* title = [self tabGridButtonTitleForCurrentState];
  UIImage* image = DefaultAppBarSymbol(kAppSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];
  button.accessibilityIdentifier = kAppBarTabGridButtonIdentifier;
  _tabGridButton = button;

  UIButtonConfiguration* configuration = button.configuration;
  // Make the base image clear so we can overlay our own with the label while
  // keeping the right size.
  configuration.imageColorTransformer = ^UIColor*(UIColor* color) {
    return UIColor.clearColor;
  };
  button.configuration = configuration;

  [button addTarget:self
                action:@selector(tabGridButtonTouchDown)
      forControlEvents:UIControlEventTouchDown];
  [button addTarget:self
                action:@selector(didTapTabGridButton)
      forControlEvents:UIControlEventTouchUpInside];
  _tabGridContentView = [[UIView alloc] init];
  _tabGridContentView.translatesAutoresizingMaskIntoConstraints = NO;
  _tabGridContentView.userInteractionEnabled = NO;
  [button addSubview:_tabGridContentView];

  [NSLayoutConstraint activateConstraints:@[
    [_tabGridContentView.centerXAnchor
        constraintEqualToAnchor:button.imageView.centerXAnchor],
    [_tabGridContentView.centerYAnchor
        constraintEqualToAnchor:button.imageView.centerYAnchor],
    [_tabGridContentView.widthAnchor
        constraintEqualToAnchor:button.imageView.widthAnchor],
    [_tabGridContentView.heightAnchor
        constraintEqualToAnchor:button.imageView.heightAnchor],
  ]];

  [_tabGridContentView addSubview:tabGridSymbolView];
  AddSameCenterConstraints(tabGridSymbolView, _tabGridContentView);

  _tabCountLabel = [[UILabel alloc] init];
  _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _tabCountLabel.textColor = ButtonsForegroundColor();
  [self updateTabCount:_tabCount];
  [_tabGridContentView addSubview:_tabCountLabel];

  __weak __typeof(self) weakSelf = self;
  __weak UIImageView* weakTabGridSymbolView = tabGridSymbolView;
  __weak UILabel* weakTabCountLabel = _tabCountLabel;
  button.configurationUpdateHandler = ^(UIButton* incomingButton) {
    [weakSelf updateTabGridButtonConfiguration:incomingButton
                                    symbolView:weakTabGridSymbolView
                                    countLabel:weakTabCountLabel];
  };
  _tabGridButtonNormalStateConstraints = @[
    [_tabCountLabel.centerXAnchor
        constraintEqualToAnchor:_tabGridContentView.centerXAnchor],
    [_tabCountLabel.centerYAnchor
        constraintEqualToAnchor:_tabGridContentView.centerYAnchor],
  ];
  _tabGridButtonTabGroupStateConstraints = @[
    [_tabCountLabel.centerXAnchor
        constraintEqualToAnchor:_tabGridContentView.centerXAnchor
                       constant:kTabGroupLabelOffset],
    [_tabCountLabel.centerYAnchor
        constraintEqualToAnchor:_tabGridContentView.centerYAnchor
                       constant:kTabGroupLabelOffset],
  ];

  [_tabGridContentView bringSubviewToFront:_tabCountLabel];

  if (IsBestOfAppGuidedTourEnabled()) {
    _spotlightView = [[UIView alloc] init];
    _spotlightView.translatesAutoresizingMaskIntoConstraints = NO;
    _spotlightView.userInteractionEnabled = NO;
    [button addSubview:_spotlightView];
    AddSameConstraintsToSidesWithInsets(
        _spotlightView, button,
        LayoutSides::kTop | LayoutSides::kTrailing | LayoutSides::kLeading |
            LayoutSides::kBottom,
        NSDirectionalEdgeInsetsMake(
            kSpotlightViewVerticalInset, kSpotlightViewHorizontalInset,
            kSpotlightViewVerticalInset, kSpotlightViewHorizontalInset));
  }

  [button
      addInteraction:[[UIContextMenuInteraction alloc] initWithDelegate:self]];

  return button;
}

// Creates a new button with `title` and `image`.
- (UIButton*)buttonWithTitle:(NSString*)title image:(UIImage*)image {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  configuration = button.configuration;
  configuration.imagePlacement = NSDirectionalRectEdgeTop;
  configuration.imagePadding = kButtonImagePadding;
  configuration.image = image;

  configuration.baseForegroundColor = ButtonsForegroundColor();
  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* textAttributes) {
    NSMutableDictionary* mutableAttributes = [textAttributes mutableCopy];
    mutableAttributes[NSFontAttributeName] =
        AssistantButtonFontSize(self.traitCollection);
    return mutableAttributes;
  };

  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalPadding, kButtonHorizontalPadding, kButtonVerticalPadding,
      kButtonHorizontalPadding);

  configuration.title = title;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;

  __weak __typeof(self) weakSelf = self;
  button.configurationUpdateHandler = ^(UIButton* incomingButton) {
    [weakSelf updateStandardButtonConfiguration:incomingButton];
  };

  button.configuration = configuration;

  button.titleLabel.adjustsFontSizeToFitWidth = YES;

  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowOffset);
  button.layer.shadowRadius = kButtonShadowRadius;
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.masksToBounds = NO;

  return button;
}

// Updates the new tab button for whether the tab groups page in the tab grid or
// a tab group is visible.
- (void)updateNewTabButtonForTabGroupsVisibility {
  if (_isTabGroupsPageVisible || (_isTabGridVisible && _isTabGroupVisible)) {
    _openNewTabButton.menu = _openNewTabButtonMenu;
    _openNewTabButton.showsMenuAsPrimaryAction = YES;
    return;
  }

  // The context menu for the New Tab button should appear on a long press when
  // the tab groups page is not visible.
  _openNewTabButton.menu = nil;
  _openNewTabButton.showsMenuAsPrimaryAction = NO;
}

// Updates the accessibility label for the new tab button based on the current
// state.
- (void)updateNewTabButtonAccessibilityLabel {
  if (_isTabGridVisible) {
    if (_isTabGroupsPageVisible) {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB_GROUP);
    } else if (_incognito) {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
    } else {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
    }
  } else {
    _openNewTabButton.accessibilityLabel =
        _incognito
            ? l10n_util::GetNSString(IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO)
            : l10n_util::GetNSString(IDS_IOS_TOOLBAR_OPEN_NEW_TAB);
  }
}

// Updates the accessibility hint for the new tab button based on the current
// state.
- (void)updateNewTabButtonAccessibilityHint {
  _openNewTabButton.accessibilityHint =
      _isTabGridVisible
          ? nil
          : l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_NEW_TAB);
}

// Updates the Tab Grid button for the given Tab Grid showing state.
- (void)updateTabGridButtonForTabGridVisibility {
  NSString* symbolName;
  BOOL shouldShowTabGroupSymbol = _isTabGroupVisible || _inTabGroup;
  if (shouldShowTabGroupSymbol) {
    symbolName = _isTabGridVisible ? kSquareFilledOnSquareSymbol : kTabsSymbol;
  } else {
    symbolName = _isTabGridVisible ? kAppFillSymbol : kAppSymbol;
  }
  [_tabGridSymbolView setSymbolImage:DefaultAppBarSymbol(symbolName)
               withContentTransition:[NSSymbolReplaceContentTransition
                                         replaceOffUpTransition]];
  _tabGridButton.accessibilityLabel = l10n_util::GetNSString(
      shouldShowTabGroupSymbol ? IDS_IOS_TOOLBAR_SHOW_TAB_GROUP
                               : IDS_IOS_TOOLBAR_SHOW_TABS);
  if (shouldShowTabGroupSymbol) {
    [NSLayoutConstraint
        deactivateConstraints:_tabGridButtonNormalStateConstraints];
    [NSLayoutConstraint
        activateConstraints:_tabGridButtonTabGroupStateConstraints];
  } else {
    [NSLayoutConstraint
        deactivateConstraints:_tabGridButtonTabGroupStateConstraints];
    [NSLayoutConstraint
        activateConstraints:_tabGridButtonNormalStateConstraints];
  }
  UILabel* label = _tabCountLabel;
  UIColor* labelColor =
      _isTabGridVisible ? UIColor.blackColor : ButtonsForegroundColor();
  [UIView transitionWithView:label
                    duration:kTabGridAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    label.textColor = labelColor;
                  }
                  completion:nil];
  _tabGridButton.accessibilityHint =
      _isTabGridVisible
          ? nil
          : l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_TAB_GRID);
}

// Calls the button's setNeedsUpdateConfiguration, either immediately or in an
// animation block.
- (void)setNeedsUpdateConfiguration:(UIButton*)button
                  animationDuration:(NSTimeInterval)duration {
  if (!button) {
    // Do nothing if -viewDidLoad has not been called yet.
    return;
  }
  if (duration > 0) {
    // Cross-fade to the new color along with the current animation.
    [UIView transitionWithView:button
                      duration:duration
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      [button setNeedsUpdateConfiguration];
                      [button layoutIfNeeded];
                    }
                    completion:nil];
  } else {
    [button setNeedsUpdateConfiguration];
    [button layoutIfNeeded];
  }
}

#pragma mark - Actions

// Called when the Assistant button is tapped.
- (void)didTapAssistantButton {
  base::RecordAction(base::UserMetricsAction("MobileToolbarAssistant"));
  [self.mutator assistantButtonTappedWithState:_assistantButtonState
                                      fromView:_assistantButton];
}

// Called when the New Tab button is tapped.
- (void)didTapOpenNewTabButton:(UIView*)sender {
  base::RecordAction(base::UserMetricsAction("MobileToolbarNewTabShortcut"));
  [self.mutator createNewTabFromView:sender];
}

// Called when the Tab Grid button has a touch down.
- (void)tabGridButtonTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Called when the Tab Grid button is tapped.
- (void)didTapTabGridButton {
  if (_isTabGridVisible) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
    [self.tabGridHandler exitTabGrid];
  } else {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
    [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
  }
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  UIView* view = interaction.view;
  UIMenu* menu = nil;
  if (view == _assistantButton) {
    menu = _assistantButtonMenu;
  } else if (view == _openNewTabButton) {
    menu = _openNewTabButtonMenu;
  } else if (view == _tabGridButton) {
    menu = _tabGridButtonMenu;
  }

  if (!menu) {
    return nil;
  }

  if ([view isKindOfClass:[UIButton class]]) {
    _previewedButton = (UIButton*)view;
    [_previewedButton setNeedsUpdateConfiguration];
    [_previewedButton layoutIfNeeded];
  }

  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return menu;
                   }];
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
                               configuration:
                                   (UIContextMenuConfiguration*)configuration
       highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier {
  UIView* view = interaction.view;
  if ([view isKindOfClass:[UIButton class]]) {
    UIPreviewParameters* parameters = [[UIPreviewParameters alloc] init];
    parameters.backgroundColor =
        _incognito ? [UIColor colorNamed:kAppBarIncognitoColor]
                   : [UIColor colorNamed:kAppBarColor];

    return [[UITargetedPreview alloc] initWithView:view parameters:parameters];
  }
  return nil;
}

- (UITargetedPreview*)contextMenuInteraction:
                          (UIContextMenuInteraction*)interaction
                               configuration:
                                   (UIContextMenuConfiguration*)configuration
       dismissalPreviewForItemWithIdentifier:(id<NSCopying>)identifier {
  UIView* view = interaction.view;
  if ([view isKindOfClass:[UIButton class]]) {
    UIPreviewParameters* parameters = [[UIPreviewParameters alloc] init];
    parameters.shadowPath = [UIBezierPath bezierPath];
    parameters.backgroundColor = [UIColor clearColor];

    return [[UITargetedPreview alloc] initWithView:view parameters:parameters];
  }
  return nil;
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  if (interaction.view == _previewedButton) {
    __weak __typeof(self) weakSelf = self;
    [animator addAnimations:^{
      __strong __typeof(weakSelf) strongSelf = weakSelf;
      if (strongSelf) {
        strongSelf->_previewedButton = nil;
        [interaction.view setNeedsUpdateConfiguration];
      }
    }];
  }
}

@end
