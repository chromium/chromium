// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import <optional>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_utils.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
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

// The spacing inside the stack view.
constexpr CGFloat kStackViewSpacing = 4;
// The horizontal margins of the stack view.
constexpr CGFloat kStackViewHorizontalMargin = 8;
// The vertical offset of the stack view in portrait.
constexpr CGFloat kStackViewLandscapeVerticalOffset = 2;

// The inner padding of the buttons.
constexpr CGFloat kButtonHorizontalPadding = 4;
constexpr CGFloat kButtonVerticalPadding = 12;

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

}  // namespace

@interface AppBarViewController ()

// The alpha for the titles of the buttons.
@property(nonatomic, assign) CGFloat buttonsTitleAlpha;

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
  // Cached avatar for the assistant button.
  UIImage* _assistantButtonAvatar;
  // The background view.
  AppBarView* _backgroundView;
  // The stack view constraints that are updated on rotation.
  NSLayoutConstraint* _stackViewTopConstraint;
  NSLayoutConstraint* _stackViewBottomConstraint;
}

#pragma mark - Accessors & Mutators

- (void)setButtonsTitleAlpha:(CGFloat)buttonsTitleAlpha {
  if (buttonsTitleAlpha == self.buttonsTitleAlpha) {
    return;
  }
  _buttonsTitleAlpha = buttonsTitleAlpha;
  [_assistantButton setNeedsUpdateConfiguration];
  [_openNewTabButton setNeedsUpdateConfiguration];
  [_tabGridButton setNeedsUpdateConfiguration];
}

#pragma mark - Public

- (void)updateForAngle:(CGFloat)angle {
  [self loadViewIfNeeded];

  CGAffineTransform transform = CGAffineTransformMakeRotation(angle);
  _assistantButton.transform = transform;
  _openNewTabButton.transform = transform;
  _tabGridButton.transform = transform;

  [self updateStackViewConstraintsForPortrait:(angle == 0)];
}

- (void)toggleSpotlightView:(BOOL)shouldShow {
  CHECK(IsBestOfAppGuidedTourEnabled());
  _spotlightView.hidden = !shouldShow;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _backgroundView = [[AppBarView alloc] init];
  _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_backgroundView atIndex:0];

  self.buttonsTitleAlpha = 1;

  _assistantButton = [self createAssistantButton];
  _openNewTabButton = [self createOpenNewTabButton];
  _tabGridButton = [self createTabGridButton];
  [self updateTabGridButtonForTabGridVisibility];
  [self updateNewTabButtonAccessibilityLabel];

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _assistantButton, _openNewTabButton, _tabGridButton
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.distribution = UIStackViewDistributionFillEqually;
  stackView.spacing = kStackViewSpacing;
  stackView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;

  UIView* view = self.view;
  [view addSubview:stackView];

  _stackViewTopConstraint =
      [stackView.topAnchor constraintEqualToAnchor:view.topAnchor];
  _stackViewBottomConstraint =
      [stackView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [_backgroundView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_backgroundView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],
    [_backgroundView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [_backgroundView.topAnchor constraintEqualToAnchor:view.topAnchor
                                              constant:-kAppBarCornerRadius],
    [stackView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor
                       constant:kStackViewHorizontalMargin],
    _stackViewTopConstraint,
    [stackView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor
                       constant:-kStackViewHorizontalMargin],
    _stackViewBottomConstraint,
    [view.heightAnchor constraintEqualToConstant:kAppBarHeight],
  ]];

  [self.layoutGuideCenter referenceView:stackView underName:kAppBarGuide];

  // The AppBar is created in "portrait" orientation.
  [self updateStackViewConstraintsForPortrait:YES];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateAssistantButtonTitleIfNeeded];
  [self updateTabGridButtonTitleIfNeeded];
  [self updateOpenNewTabButtonTitleIfNeeded];
}

#pragma mark - UIContentContainer

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof__(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateUIForTransitionToSize:size];
      }
                      completion:nil];
}

#pragma mark - AppBarConsumer

- (void)updateTabCount:(NSUInteger)count {
  _tabCount = count;
  _tabCountLabel.attributedText = TextForTabCount(count, kTabGridFontSize);
}

- (void)setTabGridVisible:(BOOL)tabGridVisible {
  if (_isTabGridVisible == tabGridVisible) {
    return;
  }
  _isTabGridVisible = tabGridVisible;
  _backgroundView.hideColorBackground = tabGridVisible;
  [self updateTabGridButtonForTabGridVisibility];
  [self updateNewTabButtonAccessibilityLabel];
}

- (void)setIncognito:(BOOL)incognito {
  if (_backgroundView.incognito == incognito) {
    return;
  }
  _backgroundView.incognito = incognito;
  [self updateNewTabButtonAccessibilityLabel];
  if (incognito) {
    _assistantButton.enabled = NO;
  }
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
      _assistantButton.menu = menu;
      return;
    case AppBarButtonTypeNewTab:
      _openNewTabButtonMenu = menu;
      _openNewTabButton.menu = menu;
      return;
    case AppBarButtonTypeTabGrid:
      _tabGridButtonMenu = menu;
      _tabGridButton.menu = menu;
      return;
  }
  NOTREACHED();
}

- (void)setAssistantButtonState:(AppBarAssistantButtonState)state {
  _assistantButtonState = state;

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
  _assistantButton.enabled = enabled && !_backgroundView.incognito;
  _openNewTabButton.enabled = enabled;
  _tabGridButton.enabled = enabled;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // The App Bar and the button titles should be fully visible in landscape
  // orientation.
  self.buttonsTitleAlpha =
      AppBarPositionForView(self.view) == AppBarPosition::kBottom ? progress
                                                                  : 1.0;
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  if (AppBarPositionForView(self.view) != AppBarPosition::kBottom) {
    return;
  }

  [self updateForFullscreenProgress:agent->bottom_progress()];
}

#pragma mark - Private

// Updates the stack view constraints based on the orientation.
- (void)updateStackViewConstraintsForPortrait:(BOOL)portrait {
  CGFloat offset = portrait ? 0 : -kStackViewLandscapeVerticalOffset;
  _stackViewTopConstraint.constant = offset;
  _stackViewBottomConstraint.constant = offset;
}

// Handles updating the UI for a size transition.
- (void)updateUIForTransitionToSize:(CGSize)size {
  if (size.width > size.height) {
    [self updateForFullscreenProgress:1.0];
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

  CGFloat availableWidthForButton =
      (self.view.bounds.size.width - 2 * kStackViewHorizontalMargin -
       2 * kStackViewSpacing) /
      3.0;
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
    default:
      return @"TODO(crbug.com/484000888): To be removed when lens is "
             @"implemented";
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
    case AppBarAssistantButtonState::kLens:
      title = @"TODO(crbug.com/484000888): Use actual text";
      image = CustomAppBarSymbol(kCameraLensSymbol);
      break;
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
  }

  UIButtonConfiguration* configuration = _assistantButton.configuration;
  configuration.title = title;
  configuration.image = image ? image : CustomAppBarSymbol(kCameraLensSymbol);
  _assistantButton.configuration = configuration;
}

// Returns a new "Assistant" button.
- (UIButton*)createAssistantButton {
  UIButton* button = [self buttonWithTitle:nil image:nil];
  button.menu = _assistantButtonMenu;

  [button addTarget:self
                action:@selector(didTapAssistantButton)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier = kAppBarAssistantButtonId;

  _assistantButton = button;
  [self updateAssistantButton];

  return button;
}

// Returns a new "New Tab" button.
- (UIButton*)createOpenNewTabButton {
  NSString* title = [self openNewTabButtonTitleForCurrentState];
  UIImage* image = DefaultAppBarSymbol(kPlusInCircleSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];
  button.menu = _openNewTabButtonMenu;
  button.accessibilityIdentifier = kAppBarNewTabButtonIdentifier;

  [button addTarget:self
                action:@selector(didTapOpenNewTabButton:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
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
  button.menu = _tabGridButtonMenu;
  button.accessibilityIdentifier = kAppBarTabGridButtonIdentifier;

  UIButtonConfiguration* configuration = button.configuration;
  // Make the base image clear so we can overlay our own with the label while
  // keeping the right size.
  configuration.imageColorTransformer = ^UIColor*(UIColor* color) {
    tabGridSymbolView.tintColor = color;
    return UIColor.clearColor;
  };
  button.configuration = configuration;

  [button addTarget:self
                action:@selector(tabGridButtonTouchDown)
      forControlEvents:UIControlEventTouchDown];
  [button addTarget:self
                action:@selector(didTapTabGridButton)
      forControlEvents:UIControlEventTouchUpInside];
  [button addSubview:tabGridSymbolView];
  AddSameCenterConstraints(tabGridSymbolView, button.imageView);

  _tabCountLabel = [[UILabel alloc] init];
  _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _tabCountLabel.textColor = ButtonsForegroundColor();
  [self updateTabCount:_tabCount];
  [button addSubview:_tabCountLabel];
  _tabGridButtonNormalStateConstraints = @[
    [_tabCountLabel.centerXAnchor
        constraintEqualToAnchor:button.imageView.centerXAnchor],
    [_tabCountLabel.centerYAnchor
        constraintEqualToAnchor:button.imageView.centerYAnchor],
  ];
  _tabGridButtonTabGroupStateConstraints = @[
    [_tabCountLabel.centerXAnchor
        constraintEqualToAnchor:button.imageView.centerXAnchor
                       constant:kTabGroupLabelOffset],
    [_tabCountLabel.centerYAnchor
        constraintEqualToAnchor:button.imageView.centerYAnchor
                       constant:kTabGroupLabelOffset],
  ];

  [button bringSubviewToFront:_tabCountLabel];

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
    [self.layoutGuideCenter referenceView:_spotlightView
                                underName:kTabSwitcherGuide];
  }

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

  configuration.contentInsets = NSDirectionalEdgeInsetsMake(
      kButtonVerticalPadding, kButtonHorizontalPadding, kButtonVerticalPadding,
      kButtonHorizontalPadding);

  configuration.title = title;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;

  __weak __typeof(self) weakSelf = self;
  __weak UIButton* weakButton = button;
  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* textAttributes) {
    NSMutableDictionary* mutableAttributes = [textAttributes mutableCopy];
    mutableAttributes[NSFontAttributeName] =
        AssistantButtonFontSize(weakSelf.traitCollection);

    BOOL useEnabledColor = !weakButton || weakButton.enabled;
    UIColor* textColor = useEnabledColor ? ButtonsForegroundColor()
                                         : [ButtonsForegroundColor()
                                               colorWithAlphaComponent:0.5];

    mutableAttributes[NSForegroundColorAttributeName] = textColor;
    return mutableAttributes;
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
  if (_isTabGroupsPageVisible || _isTabGroupVisible) {
    _openNewTabButton.menu = _openNewTabButtonMenu;
    _openNewTabButton.showsMenuAsPrimaryAction = YES;
    return;
  }

  // The context menu for the New Tab button should appear on a long press when
  // the tab groups page is not visible.
  _openNewTabButton.showsMenuAsPrimaryAction = NO;
}

// Updates the accessibility label for the new tab button based on the current
// state.
- (void)updateNewTabButtonAccessibilityLabel {
  if (_isTabGridVisible) {
    if (_isTabGroupsPageVisible) {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB_GROUP);
    } else if (_backgroundView.incognito) {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
    } else {
      _openNewTabButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
    }
  } else {
    _openNewTabButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TOOLBAR_ACCESSIBILITY_HINT_NEW_TAB);
  }
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
}

#pragma mark - Actions

// Called when the Assistant button is tapped.
- (void)didTapAssistantButton {
  base::RecordAction(base::UserMetricsAction("MobileToolbarAssistant"));
  [self.mutator assistantButtonTappedWithState:_assistantButtonState];
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

@end
