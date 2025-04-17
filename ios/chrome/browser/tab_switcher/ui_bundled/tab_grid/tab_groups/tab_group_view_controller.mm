// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "base/i18n/time_formatting.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/menu/ui_bundled/action_factory.h"
#import "ios/chrome/browser/share_kit/model/sharing_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_mutator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_presentation_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_action_type.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/tab_group_indicator_features_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using tab_groups::SharingState;

namespace {
// Background.
constexpr CGFloat kBackgroundAlpha = 0.6;

// Top toolbar
constexpr CGFloat kTopToolbarHeight = 44;
constexpr CGFloat kTopStackViewSpacing = 10;
constexpr CGFloat kTopToolbarMargin = 16;

// Button.
constexpr CGFloat kPlusImageSize = 20;

// Animation.
constexpr CGFloat kTranslationCompletion = 0;
constexpr CGFloat kOriginScale = 0.1;

// Top title.
constexpr CGFloat kDotSize = 12;
constexpr CGFloat kSpace = 8;

// FacePile constraints.
constexpr CGFloat kFacePileWidth = 84;
constexpr CGFloat kFacePileHeight = 44;

// Container constraints.
constexpr CGFloat kContainerMargin = 12;
constexpr CGFloat kContainerMultiplier = 0.8;
constexpr CGFloat kContainerCornerRadius = 24;
constexpr CGFloat kContainerBackgroundAlpha = 0.8;

}  // namespace

@interface TabGroupViewController () <TabGridToolbarsGridDelegate,
                                      UINavigationBarDelegate>
@end

@implementation TabGroupViewController {
  // The embedded navigation bar.
  UINavigationBar* _navigationBar;
  // The top toolbar.
  UIView* _topToolbar;
  // The background of the top toolbar.
  UIView* _topToolbarBackground;
  // The stack view containing the buttons of the top toolbar.
  UIStackView* _topToolbarButtonsStackView;
  // The the tab group menu button.
  UIButton* _menuButton;
  // Tab Groups handler.
  __weak id<TabGroupsCommands> _handler;
  // Group's title.
  NSString* _groupTitle;
  // Group's color.
  UIColor* _groupColor;
  // The blur background.
  UIVisualEffectView* _blurView;
  // Currently displayed group.
  raw_ptr<const TabGroup> _tabGroup;
  // Whether the `Back` button or the `Esc` key has been tapped.
  BOOL _backButtonTapped;
  // Title view displayed in the navigation bar containing group title and
  // color.
  UIView* _titleView;
  // Title label in the navigation bar.
  UILabel* _titleLabel;
  // Dot view in the navigation bar.
  UIView* _coloredDotView;
  // Whether this is an incognito group.
  BOOL _incognito;
  // Whether the share option is available.
  BOOL _shareAvailable;
  // Sharing state of the saved tab group.
  SharingState _sharingState;
  // The bottom toolbar.
  TabGridBottomToolbar* _bottomToolbar;
  // The face pile view that displays the share button or the face pile.
  UIView* _facePileView;
  // Container for the content of the ViewController.
  UIView* _container;
}

#pragma mark - Public

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler
                      incognito:(BOOL)incognito
                       tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group view controller outside "
         "the Tab Groups experiment.";
  CHECK(tabGroup);
  if ((self = [super init])) {
    _handler = handler;
    _incognito = incognito;
    _tabGroup = tabGroup;
    _gridViewController = [[TabGroupGridViewController alloc] init];
    if (!incognito) {
      _gridViewController.theme = GridThemeLight;
    } else {
      _gridViewController.theme = GridThemeDark;
    }
    _gridViewController.viewDelegate = self;

    // This view controller has a dark background and should be considered as
    // dark mode regardless of the theme of the grid.
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }
  return self;
}

- (void)contentWillAppearAnimated:(BOOL)animated {
  [self.view layoutIfNeeded];
  [_gridViewController contentWillAppearAnimated:YES];
}

- (void)prepareForPresentation {
  [self fadeBlurOut];

  [self contentWillAppearAnimated:YES];

  _navigationBar.alpha = 0;
  _gridViewController.view.alpha = 0;
  CGPoint center = [_gridViewController.view convertPoint:self.view.center
                                                 fromView:self.view];
  [_gridViewController centerVisibleCellsToPoint:center
                           translationCompletion:kTranslationCompletion
                                       withScale:kOriginScale];
}

- (void)animateTopElementsPresentation {
  _navigationBar.alpha = 1;
}

- (void)animateGridPresentation {
  _gridViewController.view.alpha = 1;
  [_gridViewController resetVisibleCellsCenterAndScale];
}

- (void)fadeBlurIn {
  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    if (IsContainedTabGroupEnabled()) {
      self.view.backgroundColor = [UIColor colorNamed:kStaticGrey600Color];
    } else {
      self.view.backgroundColor = UIColor.blackColor;
    }
  } else {
    if (IsContainedTabGroupEnabled()) {
      UIBlurEffect* blurEffect = [UIBlurEffect
          effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterial];
      _blurView.effect = blurEffect;
    } else {
      self.view.backgroundColor = [[UIColor colorNamed:kStaticGrey900Color]
          colorWithAlphaComponent:kBackgroundAlpha];
      UIBlurEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
      _blurView.effect = blurEffect;
    }
  }
}

- (void)animateDismissal {
  if (_backButtonTapped) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridTabGroupDismissed"));
  }

  CGPoint center = [_gridViewController.view convertPoint:self.view.center
                                                 fromView:self.view];
  [_gridViewController centerVisibleCellsToPoint:center
                           translationCompletion:kTranslationCompletion
                                       withScale:kOriginScale];
  self.view.alpha = 0;
}

- (void)fadeBlurOut {
  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    self.view.backgroundColor = UIColor.clearColor;
  } else {
    _blurView.effect = nil;
  }
}

- (void)gridViewControllerDidScroll {
  [_bottomToolbar
      setScrollViewScrolledToEdge:self.gridViewController.scrolledToBottom];
  _topToolbarBackground.hidden = self.gridViewController.scrolledToTop;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kTabGroupViewIdentifier;
  self.view.accessibilityViewIsModal = YES;
  self.view.backgroundColor = UIColor.clearColor;

  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    _blurView = [[UIVisualEffectView alloc] initWithEffect:nil];
    _blurView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_blurView];
    AddSameConstraints(self.view, _blurView);
  }

  [self fadeBlurIn];

  _container = [[UIView alloc] init];
  _container.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_container];

  if (IsContainedTabGroupEnabled()) {
    _container.backgroundColor =
        [UIColor.blackColor colorWithAlphaComponent:kContainerBackgroundAlpha];
    _container.layer.cornerRadius = kContainerCornerRadius;
    _container.layer.masksToBounds = YES;

    [NSLayoutConstraint activateConstraints:@[
      [self.view.centerXAnchor
          constraintEqualToAnchor:_container.centerXAnchor],
      [self.view.centerYAnchor
          constraintEqualToAnchor:_container.centerYAnchor],
      [_container.heightAnchor constraintEqualToAnchor:self.view.heightAnchor
                                            multiplier:kContainerMultiplier],
      [self.view.trailingAnchor
          constraintEqualToAnchor:_container.trailingAnchor
                         constant:kContainerMargin],
      [self.view.leadingAnchor constraintEqualToAnchor:_container.leadingAnchor
                                              constant:-kContainerMargin],
    ]];
  } else {
    AddSameConstraints(self.view, _container);
  }

  if (IsContainedTabGroupEnabled()) {
    _topToolbar = [self configuredTopToolbar];
    [_container addSubview:_topToolbar];

    [NSLayoutConstraint activateConstraints:@[
      [_topToolbar.topAnchor constraintEqualToAnchor:_container.topAnchor],
      [_topToolbar.leadingAnchor
          constraintEqualToAnchor:_container.leadingAnchor],
      [_topToolbar.trailingAnchor
          constraintEqualToAnchor:_container.trailingAnchor],
    ]];
  } else {
    [self configureNavigationBar];
  }

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_gridViewController];
  if (IsContainedTabGroupEnabled()) {
    [_container insertSubview:gridView belowSubview:_topToolbar];
  } else {
    [_container insertSubview:gridView belowSubview:_navigationBar];
  }

  [self updateGridInsets];

  [_gridViewController didMoveToParentViewController:self];

  [NSLayoutConstraint activateConstraints:@[
    [gridView.leadingAnchor constraintEqualToAnchor:_container.leadingAnchor],
    [gridView.trailingAnchor constraintEqualToAnchor:_container.trailingAnchor],
    [gridView.bottomAnchor constraintEqualToAnchor:_container.bottomAnchor],
  ]];

  if (IsContainedTabGroupEnabled()) {
    [gridView.topAnchor constraintEqualToAnchor:_container.topAnchor].active =
        YES;
  } else {
    [gridView.topAnchor constraintEqualToAnchor:_navigationBar.bottomAnchor]
        .active = YES;
  }

  // Add the toolbar after the grid to make sure it is above it.
  [self configureBottomToolbar];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.class ]
                       withAction:@selector(updateGridInsets)];
    [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.class ]
                       withAction:@selector(updateGridInsets)];
  }
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateGridInsets];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (previousTraitCollection.verticalSizeClass !=
          self.traitCollection.verticalSizeClass ||
      previousTraitCollection.horizontalSizeClass !=
          self.traitCollection.horizontalSizeClass) {
    [self updateGridInsets];
  }
}
#endif

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  _backButtonTapped = YES;
  [_handler hideTabGroup];
  return NO;
}

- (void)navigationBar:(UINavigationBar*)navigationBar
           didPopItem:(UINavigationItem*)item {
  _backButtonTapped = YES;
  [_handler hideTabGroup];
}

#pragma mark - UIBarPositioningDelegate

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  // Let the background of the navigation bar extend to the top, behind the
  // Dynamic Island or notch.
  return UIBarPositionTopAttached;
}

#pragma mark - TabGroupConsumer

- (void)setGroupTitle:(NSString*)title {
  _groupTitle = title;
  _gridViewController.groupTitle = title;
  [_titleLabel setText:_groupTitle];
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
  _gridViewController.groupColor = color;
  [_coloredDotView setBackgroundColor:_groupColor];
}

- (void)setShareAvailable:(BOOL)shareAvailable {
  _shareAvailable = shareAvailable;
  if (IsContainedTabGroupEnabled()) {
    _menuButton.menu = [self configuredTabGroupMenu];
  } else {
    [self configureNavigationBarItems];
  }
}

- (void)setSharingState:(SharingState)state {
  if (_sharingState == state) {
    return;
  }
  _sharingState = state;
  _gridViewController.shared = _sharingState != SharingState::kNotShared;
  if (IsContainedTabGroupEnabled()) {
    _menuButton.menu = [self configuredTabGroupMenu];
  } else {
    [self configureNavigationBarItems];
  }
}

- (void)setFacePileView:(UIView*)facePileView {
  if (_facePileView == facePileView) {
    return;
  }

  if (_facePileView.superview == self.view) {
    [_facePileView removeFromSuperview];
  }

  _facePileView = facePileView;

  if (!_facePileView) {
    return;
  }

  if (IsContainedTabGroupEnabled()) {
    [_topToolbarButtonsStackView insertArrangedSubview:_facePileView atIndex:0];
  } else {
    _facePileView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_facePileView];
    [self configureNavigationBarItems];
  }
}

- (void)setActivitySummaryCellText:(NSString*)text {
  _gridViewController.activitySummaryCellText = text;
}

#pragma mark - Private

// The plus button has been tapped.
- (void)didTapPlusButton {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridTabGroupCreateNewTab"));
  [self openNewTab];
}

// The close button has been tapped.
- (void)didTapCloseButton {
  _backButtonTapped = YES;
  [_handler hideTabGroup];
}

// The facePile button has been tapped.
- (void)didTapFacePileButton {
  [self.presentationHandler showShareKitFlow];
}

// Returns the navigation item which contain the back button.
- (UINavigationItem*)configuredBackButton {
  CHECK(!IsContainedTabGroupEnabled());
  UINavigationItem* back = [[UINavigationItem alloc] init];
  back.title = @"";
  return back;
}

// Returns the menu button, configured.
- (UIButton*)configuredMenuButton {
  UIImage* threeDotImage =
      DefaultSymbolWithPointSize(kMenuSymbol, kPlusImageSize);
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.image = threeDotImage;

  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.showsMenuAsPrimaryAction = YES;
  button.menu = [self configuredTabGroupMenu];
  button.accessibilityIdentifier = kTabGroupOverflowMenuButtonIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL);

  return button;
}

// Returns the stack view containing the top toolbar buttons.
- (UIStackView*)configuredTopToolbarStackView {
  CHECK(IsContainedTabGroupEnabled());
  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.spacing = kTopStackViewSpacing;

  if (_facePileView) {
    [stackView addArrangedSubview:_facePileView];
  }

  _menuButton = [self configuredMenuButton];
  [stackView addArrangedSubview:_menuButton];

  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf didTapCloseButton];
  }];
  UIButtonConfiguration* closeConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  closeConfiguration.image =
      DefaultSymbolWithPointSize(kXMarkSymbol, kPlusImageSize);
  UIButton* closeButton = [UIButton buttonWithConfiguration:closeConfiguration
                                              primaryAction:closeAction];
  [stackView addArrangedSubview:closeButton];

  return stackView;
}

// Returns the top toolbar with all its content.
- (UIView*)configuredTopToolbar {
  CHECK(IsContainedTabGroupEnabled());
  _topToolbarBackground = [[UIVisualEffectView alloc]
      initWithEffect:
          [UIBlurEffect
              effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterialDark]];
  _topToolbarBackground.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* topToolbar = [[UIView alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [topToolbar addSubview:_topToolbarBackground];

  [topToolbar.heightAnchor constraintEqualToConstant:kTopToolbarHeight].active =
      YES;
  AddSameConstraints(topToolbar, _topToolbarBackground);

  _titleView = [self configuredTitleView];

  _topToolbarButtonsStackView = [self configuredTopToolbarStackView];

  [topToolbar addSubview:_titleView];
  [topToolbar addSubview:_topToolbarButtonsStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_titleView.leadingAnchor constraintEqualToAnchor:topToolbar.leadingAnchor
                                             constant:kTopToolbarMargin],
    [_titleView.centerYAnchor constraintEqualToAnchor:topToolbar.centerYAnchor],
    [_titleView.topAnchor
        constraintGreaterThanOrEqualToAnchor:topToolbar.topAnchor],
    [_titleView.bottomAnchor
        constraintLessThanOrEqualToAnchor:topToolbar.bottomAnchor],

    [_topToolbarButtonsStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:_titleView.trailingAnchor
                                    constant:kTopToolbarMargin],

    [_topToolbarButtonsStackView.trailingAnchor
        constraintEqualToAnchor:topToolbar.trailingAnchor
                       constant:-kTopToolbarMargin],
    [_topToolbarButtonsStackView.centerYAnchor
        constraintEqualToAnchor:topToolbar.centerYAnchor],
    [_topToolbarButtonsStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:topToolbar.topAnchor],
    [_topToolbarButtonsStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:topToolbar.bottomAnchor],

  ]];

  return topToolbar;
}

// Returns the navigation item which contain the plus button and the overflow
// menu.
- (UINavigationItem*)configuredRightNavigationItems {
  CHECK(!IsContainedTabGroupEnabled());
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];

  UIImage* threeDotImage =
      DefaultSymbolWithPointSize(kMenuSymbol, kPlusImageSize);
  UIBarButtonItem* menuItem =
      [[UIBarButtonItem alloc] initWithImage:threeDotImage
                                        menu:[self configuredTabGroupMenu]];
  menuItem.accessibilityIdentifier = kTabGroupOverflowMenuButtonIdentifier;
  menuItem.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL);

  UIBarButtonItem* facePileBarButton;
  if (_facePileView) {
    _facePileView.userInteractionEnabled = NO;

    UIButton* facePileButton =
        [[UIButton alloc] initWithFrame:_facePileView.bounds];
    [facePileButton addTarget:self
                       action:@selector(didTapFacePileButton)
             forControlEvents:UIControlEventTouchUpInside];
    [facePileButton addSubview:_facePileView];
    facePileButton.accessibilityIdentifier = kTabGroupFacePileButtonIdentifier;
    if (_sharingState == SharingState::kNotShared) {
      facePileButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_SHARE_GROUP);
    } else {
      facePileButton.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_MANAGE_GROUP);
    }
    [NSLayoutConstraint activateConstraints:@[
      [facePileButton.widthAnchor constraintEqualToConstant:kFacePileWidth],
      [facePileButton.heightAnchor constraintEqualToConstant:kFacePileHeight],
    ]];

    facePileBarButton =
        [[UIBarButtonItem alloc] initWithCustomView:facePileButton];
  }

  if (IsTabGroupIndicatorEnabled() && HasTabGroupIndicatorButtonsUpdated()) {
    NSMutableArray* buttons = [NSMutableArray array];
    [buttons addObject:menuItem];
    if (facePileBarButton) {
      [buttons addObject:facePileBarButton];
    }
    navigationItem.rightBarButtonItems = buttons;
  } else {
    UIImage* plusImage =
        DefaultSymbolWithPointSize(kPlusSymbol, kPlusImageSize);
    UIBarButtonItem* plusItem =
        [[UIBarButtonItem alloc] initWithImage:plusImage
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(didTapPlusButton)];
    plusItem.accessibilityIdentifier = kTabGroupNewTabButtonIdentifier;
    plusItem.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);

    navigationItem.rightBarButtonItems = @[ menuItem, plusItem ];
  }
  return navigationItem;
}

// Returns the colorful dot for the title, configured.
- (UIView*)configuredTitleDot {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kDotSize / 2;
  dotView.backgroundColor = _groupColor;

  return dotView;
}

// Returns the title label, configured.
- (UILabel*)configuredTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = UIColor.whiteColor;
  titleLabel.numberOfLines = 1;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.accessibilityIdentifier = kTabGroupViewTitleIdentifier;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleHeadline]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  NSMutableAttributedString* boldTitle =
      [[NSMutableAttributedString alloc] initWithString:_groupTitle];

  [boldTitle addAttribute:NSFontAttributeName
                    value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                    range:NSMakeRange(0, _groupTitle.length)];
  titleLabel.attributedText = boldTitle;

  return titleLabel;
}

// Returns the configured title view.
- (UIView*)configuredTitleView {
  _coloredDotView = [self configuredTitleDot];
  _titleLabel = [self configuredTitleLabel];

  UIView* titleView = [[UIView alloc] init];
  titleView.translatesAutoresizingMaskIntoConstraints = NO;

  [titleView addSubview:_coloredDotView];
  [titleView addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_coloredDotView.trailingAnchor
                       constant:kSpace],
    [_coloredDotView.centerYAnchor
        constraintEqualToAnchor:titleView.centerYAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:titleView.trailingAnchor],
    [_titleLabel.topAnchor constraintEqualToAnchor:titleView.topAnchor],
    [_titleLabel.bottomAnchor constraintEqualToAnchor:titleView.bottomAnchor],
    [_coloredDotView.heightAnchor constraintEqualToConstant:kDotSize],
    [_coloredDotView.widthAnchor constraintEqualToConstant:kDotSize],
  ]];

  if (IsContainedTabGroupEnabled()) {
    [_coloredDotView.leadingAnchor
        constraintEqualToAnchor:titleView.leadingAnchor
                       constant:kSpace]
        .active = YES;
  } else {
    [_coloredDotView.leadingAnchor
        constraintEqualToAnchor:titleView.leadingAnchor
                       constant:-kDotSize - kSpace]
        .active = YES;
  }

  return titleView;
}

// Returns the navigation item which contain the group title, color and the
// right navigation items.
- (UINavigationItem*)configuredGroupItem {
  CHECK(!IsContainedTabGroupEnabled());
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];

  _titleView = [self configuredTitleView];

  navigationItem.titleView = _titleView;
  navigationItem.titleView.hidden = YES;
  navigationItem.rightBarButtonItems =
      [self configuredRightNavigationItems].rightBarButtonItems;
  return navigationItem;
}

// Configures the navigation bar.
- (void)configureNavigationBar {
  CHECK(!IsContainedTabGroupEnabled());
  _navigationBar = [[UINavigationBar alloc] init];
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self configureNavigationBarItems];

  // Make the navigation bar transparent so it completly match the view.
  [_navigationBar setBackgroundImage:[[UIImage alloc] init]
                       forBarMetrics:UIBarMetricsDefault];
  _navigationBar.shadowImage = [[UIImage alloc] init];
  _navigationBar.translucent = YES;

  _navigationBar.tintColor = UIColor.whiteColor;
  _navigationBar.delegate = self;
  [_container addSubview:_navigationBar];

  [NSLayoutConstraint activateConstraints:@[
    [_navigationBar.topAnchor
        constraintEqualToAnchor:_container.safeAreaLayoutGuide.topAnchor],
    [_navigationBar.leadingAnchor
        constraintEqualToAnchor:_container.safeAreaLayoutGuide.leadingAnchor],
    [_navigationBar.trailingAnchor
        constraintEqualToAnchor:_container.safeAreaLayoutGuide.trailingAnchor],
  ]];
}

// Configures the navigation bar items.
- (void)configureNavigationBarItems {
  CHECK(!IsContainedTabGroupEnabled());
  if (!_navigationBar) {
    return;
  }
  _navigationBar.items =
      @[ [self configuredBackButton], [self configuredGroupItem] ];
}

// Adds the bottom toolbar containing the "plus" button.
- (void)configureBottomToolbar {
  if (!IsTabGroupIndicatorEnabled() || !HasTabGroupIndicatorButtonsUpdated()) {
    return;
  }

  TabGridBottomToolbar* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  _bottomToolbar = bottomToolbar;
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  bottomToolbar.hideScrolledToEdgeBackground = YES;
  bottomToolbar.buttonsDelegate = self;
  bottomToolbar.page =
      _incognito ? TabGridPageIncognitoTabs : TabGridPageRegularTabs;
  bottomToolbar.mode = TabGridMode::kNormal;
  [bottomToolbar
      setScrollViewScrolledToEdge:self.gridViewController.scrolledToBottom];
  [bottomToolbar setEditButtonHidden:YES];
  [bottomToolbar setDoneButtonHidden:YES];

  if (IsTabGroupSendFeedbackAvailable()) {
    [bottomToolbar setTabGroupFeedbackVisible:YES];
  }

  [_container addSubview:bottomToolbar];

  [NSLayoutConstraint activateConstraints:@[
    [bottomToolbar.bottomAnchor
        constraintEqualToAnchor:_container.bottomAnchor],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:_container.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:_container.trailingAnchor],
  ]];

  [self updateGridInsets];
}

// Displays the menu to rename and change the color of the currently displayed
// group.
- (void)displayEditionMenu {
  [_handler showTabGroupEditionForGroup:_tabGroup];
}

// Returns the tab group menu.
- (UIMenu*)configuredTabGroupMenu {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGroupViewMenuEntry];

  __weak TabGroupViewController* weakSelf = self;
  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  // Shared actions.
  NSMutableArray<UIAction*>* sharedActions = [[NSMutableArray alloc] init];
  if (_gridViewController.shared) {
    CHECK(IsTabGroupSyncEnabled());
    [sharedActions addObject:[actionFactory actionToManageTabGroupWithBlock:^{
                     [weakSelf manageGroup];
                   }]];

    [sharedActions addObject:[actionFactory actionToShowRecentActivity:^{
                     [weakSelf showRecentActivity];
                   }]];
  } else if (_shareAvailable) {
    [sharedActions addObject:[actionFactory actionToShareTabGroupWithBlock:^{
                     [weakSelf shareGroup];
                   }]];
  }
  if ([sharedActions count] > 0) {
    [menuElements addObject:[UIMenu menuWithTitle:@""
                                            image:nil
                                       identifier:nil
                                          options:UIMenuOptionsDisplayInline
                                         children:sharedActions]];
  }

  // Edit actions.
  NSMutableArray<UIAction*>* editActions = [[NSMutableArray alloc] init];
  [editActions addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                 [weakSelf displayEditionMenu];
               }]];
  [editActions addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
                 [weakSelf openNewTab];
               }]];
  if (_sharingState == SharingState::kNotShared) {
    [editActions addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                   [weakSelf ungroup];
                 }]];
  }
  [menuElements addObject:[UIMenu menuWithTitle:@""
                                          image:nil
                                     identifier:nil
                                        options:UIMenuOptionsDisplayInline
                                       children:editActions]];

  // Destructive actions.
  NSMutableArray<UIAction*>* destructiveActions = [[NSMutableArray alloc] init];
  if (IsTabGroupSyncEnabled()) {
    [destructiveActions
        addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
          [weakSelf closeGroup];
        }]];
    if (!_incognito) {
      switch (_sharingState) {
        case SharingState::kNotShared: {
          [destructiveActions
              addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                [weakSelf deleteGroup];
              }]];
          break;
        }
        case SharingState::kShared: {
          [destructiveActions
              addObject:[actionFactory actionToLeaveSharedTabGroupWithBlock:^{
                [weakSelf leaveSharedGroup];
              }]];
          break;
        }

        case SharingState::kSharedAndOwned: {
          [destructiveActions
              addObject:[actionFactory actionToDeleteSharedTabGroupWithBlock:^{
                [weakSelf deleteSharedGroup];
              }]];
          break;
        }
      }
    }
  } else {
    [destructiveActions
        addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
          [weakSelf deleteGroup];
        }]];
  }
  [menuElements addObject:[UIMenu menuWithTitle:@""
                                          image:nil
                                     identifier:nil
                                        options:UIMenuOptionsDisplayInline
                                       children:destructiveActions]];

  return [UIMenu menuWithTitle:@"" children:menuElements];
}

// Opens a new tab in the group.
- (void)openNewTab {
  if ([self.mutator addNewItemInGroup]) {
    [_handler showActiveTab];
  } else {
    // Dismiss the view as it looks like the policy changed, and it is not
    // possible to create a new tab anymore. In this case, the user should not
    // see any tabs.
    [_handler hideTabGroup];
  }
}

// Ungroups the current group (keeps the tab) and closes the view.
- (void)ungroup {
  // Shows the confirmation to ungroup the current group (keep the tab) and
  // close the view. Do nothing when a user cancels the action.
  if (IsTabGroupSyncEnabled()) {
    [_handler
        showTabGroupConfirmationForAction:TabGroupActionType::kUngroupTabGroup
                                    group:_tabGroup->GetWeakPtr()
                         sourceButtonItem:_navigationBar.topItem
                                              .rightBarButtonItems[0]];
    return;
  }

  [self.mutator ungroup];
  [_handler hideTabGroup];
}

// Closes the tabs and deletes the current group and closes the view.
- (void)closeGroup {
  [self.mutator closeGroup];
  [_handler hideTabGroup];
}

// Deletes the tabs and deletes the current group and closes the view.
- (void)deleteGroup {
  if (IsTabGroupSyncEnabled()) {
    // Shows the confirmation to delete the tabs, delete the current group and
    // close the view. Do nothing when a user cancels the action.
    [_handler
        showTabGroupConfirmationForAction:TabGroupActionType::kDeleteTabGroup
                                    group:_tabGroup->GetWeakPtr()
                         sourceButtonItem:_navigationBar.topItem
                                              .rightBarButtonItems[0]];
    return;
  }

  [self.mutator deleteGroup];
  [_handler hideTabGroup];
}

// Deletes the shared group and closes the view.
- (void)deleteSharedGroup {
  CHECK(IsTabGroupSyncEnabled());
  CHECK(_gridViewController.shared);
  CHECK_EQ(_sharingState, SharingState::kSharedAndOwned);

  [_handler
      startLeaveOrDeleteSharedGroup:_tabGroup->GetWeakPtr()
                          forAction:TabGroupActionType::kDeleteSharedTabGroup
                   sourceButtonItem:_navigationBar.topItem
                                        .rightBarButtonItems[0]];
}

// Leaves the shared group and closes the view.
- (void)leaveSharedGroup {
  CHECK(IsTabGroupSyncEnabled());
  CHECK(_gridViewController.shared);
  CHECK_EQ(_sharingState, SharingState::kShared);

  [_handler
      startLeaveOrDeleteSharedGroup:_tabGroup->GetWeakPtr()
                          forAction:TabGroupActionType::kLeaveSharedTabGroup
                   sourceButtonItem:_navigationBar.topItem
                                        .rightBarButtonItems[0]];
}

// Updates the safe area inset of the grid based on this VC safe areas and the
// bottom toolbar, except the top one as the grid is below a toolbar.
- (void)updateGridInsets {
  if (IsContainedTabGroupEnabled()) {
    _gridViewController.contentInsets = UIEdgeInsetsMake(
        kTopToolbarHeight, 0, _bottomToolbar.intrinsicContentSize.height, 0);
    return;
  }
  CGFloat bottomToolbarInset = 0;
  if (IsTabGroupIndicatorEnabled() && HasTabGroupIndicatorButtonsUpdated()) {
    BOOL shouldUseCompactLayout = self.traitCollection.verticalSizeClass ==
                                      UIUserInterfaceSizeClassRegular &&
                                  self.traitCollection.horizontalSizeClass ==
                                      UIUserInterfaceSizeClassCompact;

    bottomToolbarInset =
        shouldUseCompactLayout ? _bottomToolbar.intrinsicContentSize.height : 0;
  }

  UIEdgeInsets safeAreaInsets = self.view.safeAreaInsets;
  safeAreaInsets.top = 0;
  safeAreaInsets.bottom += bottomToolbarInset;
  _gridViewController.contentInsets = safeAreaInsets;
}

// Starts managing the shared group.
- (void)manageGroup {
  CHECK(_gridViewController.shared);
  [_handler showManageForGroup:_tabGroup->GetWeakPtr()];
}

// Starts sharing the group.
- (void)shareGroup {
  CHECK(!_gridViewController.shared);
  CHECK(_shareAvailable);
  [_handler showShareForGroup:_tabGroup->GetWeakPtr()];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  _backButtonTapped = YES;
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [_handler hideTabGroup];
}

#pragma mark - GridViewDelegate

- (void)gridViewHeaderHidden:(BOOL)hidden {
  _titleView.hidden = !hidden;
}

- (void)showRecentActivity {
  CHECK(_gridViewController.shared);
  [_handler showRecentActivityForGroup:_tabGroup->GetWeakPtr()];
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED();
}

- (void)doneButtonTapped:(id)sende {
  NOTREACHED();
}

- (void)newTabButtonTapped:(id)sender {
  [self didTapPlusButton];
}

- (void)selectAllButtonTapped:(id)sender {
  NOTREACHED();
}

- (void)searchButtonTapped:(id)sender {
  NOTREACHED();
}

- (void)cancelSearchButtonTapped:(id)sender {
  NOTREACHED();
}

- (void)closeSelectedTabs:(id)sender {
  NOTREACHED();
}

- (void)shareSelectedTabs:(id)sender {
  NOTREACHED();
}

- (void)selectTabsButtonTapped:(id)sender {
  NOTREACHED();
}

- (void)sendFeedbackGroupTapped:(id)sender {
  // TODO(crbug.com/398183785): Remove once we got feedback.
  [self.applicationHandler
      showReportAnIssueFromViewController:self
                                   sender:UserFeedbackSender::TabGroup];
}

@end
