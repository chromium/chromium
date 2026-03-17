// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
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
const CGFloat kTabGridFontSize = 11;
// The size of the button images.
const CGFloat kButtonImageSize = 23;
// The padding between the image and the text in the buttons.
const CGFloat kButtonImagePadding = 3;
// The shadow radius for the buttons.
const CGFloat kButtonShadowRadius = 3;
// The shadow opacity for the buttons.
const CGFloat kButtonShadowOpacity = 0.2;
// The shadow offset for the buttons.
const CGFloat kButtonShadowOffset = 1;
// The duration of the animation to update the TabGrid button.
const CGFloat kTabGridAnimationDuration = 0.25;
// Spacing between tab grid button and the tab grid spotlight view anchor.
const CGFloat kSpotlightViewHorizontalInset = 12;
const CGFloat kSpotlightViewVerticalInset = 2;
// Offset of the tab count label in the tab grid button tab group state.
const CGFloat kTabGroupLabelOffset = 3;

// The spacing inside the stack view.
const CGFloat kStackViewSpacing = 4;
// The horizontal margins of the stack view.
const CGFloat kStackViewHorizontalMargin = 8;

// The inner padding of the buttons.
const CGFloat kButtonHorizontalPadding = 4;
const CGFloat kButtonVerticalPadding = 12;

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

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
// Returns a custom symbol with the common configuration.
UIImage* CustomAppBarSymbol(NSString* symbol_name) {
  return CustomSymbolWithConfiguration(symbol_name,
                                       AppBarSymbolConfiguration());
}
#endif

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
}

- (void)updateForAngle:(CGFloat)angle {
  [self loadViewIfNeeded];

  CGAffineTransform transform = CGAffineTransformMakeRotation(angle);
  _assistantButton.transform = transform;
  _openNewTabButton.transform = transform;
  _tabGridButton.transform = transform;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.buttonsTitleAlpha = 1;

  // TODO(crbug.com/483998773): Use a real design.
  self.view.backgroundColor = [UIColor.purpleColor colorWithAlphaComponent:0.5];

  _assistantButton = [self createAssistantButton];
  _openNewTabButton = [self createOpenNewTabButton];
  _tabGridButton = [self createTabGridButton];
  [self updateTabGridButtonForTabGridVisibility];

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _assistantButton, _openNewTabButton, _tabGridButton
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.distribution = UIStackViewDistributionFillEqually;
  stackView.spacing = kStackViewSpacing;

  UIView* view = self.view;
  [view addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor
                       constant:kStackViewHorizontalMargin],
    [stackView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [stackView.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor
                       constant:-kStackViewHorizontalMargin],
    [stackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:view.bottomAnchor],
    [view.heightAnchor constraintEqualToConstant:kAppBarHeight],
  ]];

  [self.layoutGuideCenter referenceView:view underName:kAppBarGuide];
}

#pragma mark - Public

- (void)toggleSpotlightView:(BOOL)shouldShow {
  CHECK(IsBestOfAppGuidedTourEnabled());
  _spotlightView.hidden = !shouldShow;
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
  [self updateTabGridButtonForTabGridVisibility];
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

- (void)setTabGroupsPageVisible:(BOOL)tabGroupsPageVisible {
  if (tabGroupsPageVisible == _isTabGroupsPageVisible) {
    return;
  }
  _isTabGroupsPageVisible = tabGroupsPageVisible;
  [self updateNewTabButtonForTabGroupsVisibility];
}

- (void)setTabGroupVisible:(BOOL)tabGroupVisible {
  if (tabGroupVisible == _isTabGroupVisible) {
    return;
  }
  _isTabGroupVisible = tabGroupVisible;
  [self updateNewTabButtonForTabGroupsVisibility];
  [self updateTabGridButtonForTabGridVisibility];
}

- (void)setButtonsEnabled:(BOOL)enabled {
  _assistantButton.enabled = enabled;
  _openNewTabButton.enabled = enabled;
  _tabGridButton.enabled = enabled;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  self.buttonsTitleAlpha = progress;
  [_assistantButton setNeedsUpdateConfiguration];
  [_openNewTabButton setNeedsUpdateConfiguration];
  [_tabGridButton setNeedsUpdateConfiguration];
}

- (void)setAssistantButtonState:(AppBarAssistantButtonState)state
                         avatar:(UIImage*)avatar {
  _assistantButtonState = state;
  _assistantButtonAvatar = avatar;

  [self updateAssistantButton];
}

#pragma mark - Private

// Updates the assistant button configuration based on the current state.
- (void)updateAssistantButton {
  if (!_assistantButton) {
    return;
  }

  NSString* title;
  UIImage* image;
  switch (_assistantButtonState) {
    case AppBarAssistantButtonState::kSignedOut:
      title =
          l10n_util::GetNSString(IDS_IOS_NON_MODAL_SIGNIN_PROMO_SIGNIN_BUTTON);
      image = DefaultAppBarSymbol(kPersonCropCircleSymbol);
      break;
    case AppBarAssistantButtonState::kAccount:
      title = l10n_util::GetNSString(IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_TITLE);
      image = _assistantButtonAvatar;
      break;
    case AppBarAssistantButtonState::kAsk:
      title = l10n_util::GetNSString(IDS_IOS_APP_BAR_ASK_GEMINI);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
      image = CustomAppBarSymbol(kGeminiBrandedLogoSymbol);
#else
      image = DefaultAppBarSymbol(kGeminiNonBrandedLogoSymbol);
#endif
      break;
  }

  UIButtonConfiguration* configuration = _assistantButton.configuration;
  configuration.title = title;
  configuration.image =
      image ? image : DefaultAppBarSymbol(kPersonCropCircleSymbol);
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
  NSString* title = l10n_util::GetNSString(IDS_IOS_APP_BAR_NEW);
  UIImage* image = DefaultAppBarSymbol(kPlusInCircleSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];
  button.menu = _openNewTabButtonMenu;

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
  NSString* title = l10n_util::GetNSString(IDS_IOS_APP_BAR_ALL_TABS);
  UIImage* image = DefaultAppBarSymbol(kAppSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];
  button.menu = _tabGridButtonMenu;

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
  __weak UIButton* weakButton = button;

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

  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* textAttributes) {
    NSMutableDictionary* mutableAttributes = [textAttributes mutableCopy];

    mutableAttributes[NSFontAttributeName] =
        PreferredFontForTextStyleWithMaxCategory(
            UIFontTextStyleCaption2,
            weakButton.traitCollection.preferredContentSizeCategory,
            UIContentSizeCategoryExtraExtraExtraLarge);
    mutableAttributes[NSForegroundColorAttributeName] =
        [ButtonsForegroundColor()
            colorWithAlphaComponent:weakSelf.buttonsTitleAlpha];
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
