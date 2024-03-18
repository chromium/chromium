// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kColoredDotSize = 20;
constexpr CGFloat kTitleHorizontalMargin = 16;
constexpr CGFloat kTitleVerticalMargin = 10;
constexpr CGFloat kHorizontalMargin = 9;
constexpr CGFloat kPrimaryTitleMargin = 24;
constexpr CGFloat kDotTitleSeparationMargin = 8;
constexpr CGFloat kBackgroundAlpha = 0.6;
constexpr CGFloat kSubTitleHorizontalPadding = 7;
constexpr CGFloat kThreeDotButtonSize = 19;
constexpr CGFloat kTitleBackgroundCornerRadius = 17;
}  // namespace

@interface TabGroupViewController () <UINavigationBarDelegate>
@end

@implementation TabGroupViewController {
  // The embedded navigation bar.
  UINavigationBar* _navigationBar;
  // Tab Groups handler.
  __weak id<TabGroupsCommands> _handler;
  // Group's title.
  NSString* _groupTitle;
  // Group's color.
  UIColor* _groupColor;
  // The title of the view.
  UIView* _primaryTitle;
  // The blur background.
  UIVisualEffectView* _blurView;
  // Currently displayed group.
  const TabGroup* _tabGroup;
}

#pragma mark - Public

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler
                     lightTheme:(BOOL)lightTheme
                       tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group view controller outside "
         "the Tab Groups experiment.";
  CHECK(tabGroup);
  if (self = [super init]) {
    _handler = handler;
    _tabGroup = tabGroup;
    _gridViewController = [[BaseGridViewController alloc] init];
    if (lightTheme) {
      _gridViewController.theme = GridThemeLight;
    } else {
      _gridViewController.theme = GridThemeDark;
    }
    _gridViewController.mode = TabGridModeGroup;
  }
  return self;
}

- (void)prepareForPresentation {
  [self.view layoutIfNeeded];
  CGAffineTransform scaleDown =
      CGAffineTransformScale(CGAffineTransformIdentity, 0.5, 0.5);
  _navigationBar.alpha = 0;
  _primaryTitle.alpha = 0;
  _primaryTitle.transform = CGAffineTransformTranslate(
      scaleDown, -_primaryTitle.bounds.size.width / 5.0, 0);

  _gridViewController.view.alpha = 0;
  CGPoint center = [_gridViewController.view convertPoint:self.view.center
                                                 fromView:self.view];
  [_gridViewController centerVisibleCellsToPoint:center withScale:0.1];
}

- (void)animateTopElementsPresentation {
  _navigationBar.alpha = 1;
  _primaryTitle.alpha = 1;
  _primaryTitle.transform = CGAffineTransformIdentity;
}

- (void)animateGridPresentation {
  _gridViewController.view.alpha = 1;
  [_gridViewController resetVisibleCellsCenterAndScale];
}

- (void)fadeBlurIn {
  if (UIAccessibilityIsReduceTransparencyEnabled()) {
    self.view.backgroundColor = UIColor.blackColor;
  } else {
    self.view.backgroundColor = [[UIColor colorNamed:kStaticGrey900Color]
        colorWithAlphaComponent:kBackgroundAlpha];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    _blurView.effect = blurEffect;
  }
}

- (void)animateDismissal {
  CGPoint center = [_gridViewController.view convertPoint:self.view.center
                                                 fromView:self.view];
  [_gridViewController centerVisibleCellsToPoint:center withScale:0.1];
}

- (void)fadeBlurOut {
  _blurView.effect = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.clearColor;
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    _blurView = [[UIVisualEffectView alloc] initWithEffect:nil];
    _blurView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_blurView];
    AddSameConstraints(self.view, _blurView);
  }

  [self configureNavigationBar];
  UIView* primaryTitle = [self configuredPrimaryTitle];
  _primaryTitle = primaryTitle;
  UIView* secondaryTitle = [self configuredSubTitle];

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];
  [_gridViewController didMoveToParentViewController:self];

  [self.view addSubview:primaryTitle];
  [self.view addSubview:secondaryTitle];

  [NSLayoutConstraint activateConstraints:@[
    [primaryTitle.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [primaryTitle.topAnchor constraintEqualToAnchor:_navigationBar.bottomAnchor
                                           constant:kPrimaryTitleMargin],
    [secondaryTitle.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [secondaryTitle.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [secondaryTitle.topAnchor constraintEqualToAnchor:primaryTitle.bottomAnchor
                                             constant:kTitleVerticalMargin],
    [gridView.topAnchor constraintEqualToAnchor:secondaryTitle.bottomAnchor
                                       constant:kTitleVerticalMargin],
    [gridView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [gridView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
    [gridView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
  ]];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)didTapPlusButton {
  // TODO(crbug.com/1501837): Take into account the returned bool value of
  // `addNewItemInGroup`.
  [self.mutator addNewItemInGroup];
}

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  [_handler hideTabGroup];
  return NO;
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
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
}

#pragma mark - Private

// Returns the navigation item which contain the back button.
- (UINavigationItem*)configuredBackButton {
  return [[UINavigationItem alloc] init];
}

// Returns the navigation item which contain the plus button.
- (UINavigationItem*)configuredPlusButton {
  UINavigationItem* plus = [[UINavigationItem alloc] init];
  plus.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemAdd
                           target:self
                           action:@selector(didTapPlusButton)];
  return plus;
}

// Configures the navigation bar.
- (void)configureNavigationBar {
  _navigationBar = [[UINavigationBar alloc] init];
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  _navigationBar.items = @[
    [self configuredBackButton],
    [self configuredPlusButton],
  ];

  // Make the navigation bar transparent so it completly match the view.
  [_navigationBar setBackgroundImage:[[UIImage alloc] init]
                       forBarMetrics:UIBarMetricsDefault];
  _navigationBar.shadowImage = [[UIImage alloc] init];
  _navigationBar.translucent = YES;

  _navigationBar.tintColor = UIColor.whiteColor;
  _navigationBar.delegate = self;
  [self.view addSubview:_navigationBar];

  [NSLayoutConstraint activateConstraints:@[
    [_navigationBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [_navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
  ]];
}

// Returns the group color dot view.
- (UIView*)groupColorDotView {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;
  dotView.backgroundColor = _groupColor;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the title label view.
- (UILabel*)groupTitleView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = UIColor.whiteColor;
  titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];

  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  NSMutableAttributedString* boldTitle =
      [[NSMutableAttributedString alloc] initWithString:_groupTitle];

  [boldTitle addAttribute:NSFontAttributeName
                    value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                    range:NSMakeRange(0, _groupTitle.length)];
  titleLabel.attributedText = boldTitle;

  titleLabel.numberOfLines = 0;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

  return titleLabel;
}

// Returns the configured full primary title (colored dot and text title).
- (UIView*)configuredPrimaryTitle {
  UIView* fullTitleView = [[UIView alloc] initWithFrame:CGRectZero];
  fullTitleView.translatesAutoresizingMaskIntoConstraints = NO;
  fullTitleView.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
  fullTitleView.layer.cornerRadius = kTitleBackgroundCornerRadius;
  fullTitleView.opaque = NO;

  UIView* coloredDotView = [self groupColorDotView];
  UILabel* titleView = [self groupTitleView];
  [fullTitleView addSubview:coloredDotView];
  [fullTitleView addSubview:titleView];

  [NSLayoutConstraint activateConstraints:@[
    [titleView.leadingAnchor
        constraintEqualToAnchor:coloredDotView.trailingAnchor
                       constant:kDotTitleSeparationMargin],
    [coloredDotView.centerYAnchor
        constraintEqualToAnchor:titleView.centerYAnchor],
    [coloredDotView.leadingAnchor
        constraintEqualToAnchor:fullTitleView.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [fullTitleView.trailingAnchor
        constraintEqualToAnchor:titleView.trailingAnchor
                       constant:kTitleHorizontalMargin],
    [titleView.topAnchor constraintEqualToAnchor:fullTitleView.topAnchor
                                        constant:kTitleVerticalMargin],
    [fullTitleView.bottomAnchor constraintEqualToAnchor:titleView.bottomAnchor
                                               constant:kTitleVerticalMargin],
  ]];
  return fullTitleView;
}

// Returns the string with give the current number of tabs in the group.
- (NSString*)numberOfTabsString {
  // TODO(crbug.com/1501837): Configure the string with the real number of
  // items.
  return l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 1);
}

// Returns the configured sub titles view.
- (UIView*)configuredSubTitle {
  UIView* subTitleView = [[UIView alloc] initWithFrame:CGRectZero];
  subTitleView.translatesAutoresizingMaskIntoConstraints = NO;

  UITraitCollection* interfaceStyleDarkTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  UIColor* textColor = [[UIColor colorNamed:kTextSecondaryColor]
      resolvedColorWithTraitCollection:interfaceStyleDarkTraitCollection];

  UILabel* numberOfTabsLabel = [[UILabel alloc] init];
  numberOfTabsLabel.translatesAutoresizingMaskIntoConstraints = NO;
  numberOfTabsLabel.textColor = textColor;
  numberOfTabsLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  numberOfTabsLabel.text = [self numberOfTabsString];

  // TODO(crbug.com/1501837): Add action to the button.
  UIButton* menuButton = [[UIButton alloc] init];
  menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  menuButton.menu = [self configuredTabGroupMenu];
  menuButton.showsMenuAsPrimaryAction = YES;
  [menuButton
      setImage:DefaultSymbolWithPointSize(kMenuSymbol, kThreeDotButtonSize)
      forState:UIControlStateNormal];
  menuButton.tintColor = UIColor.whiteColor;

  [subTitleView addSubview:numberOfTabsLabel];
  [subTitleView addSubview:menuButton];

  [NSLayoutConstraint activateConstraints:@[
    [numberOfTabsLabel.leadingAnchor
        constraintEqualToAnchor:subTitleView.leadingAnchor
                       constant:kSubTitleHorizontalPadding],
    [numberOfTabsLabel.topAnchor
        constraintEqualToAnchor:subTitleView.topAnchor],
    [subTitleView.heightAnchor
        constraintEqualToAnchor:numberOfTabsLabel.heightAnchor],
    [menuButton.trailingAnchor
        constraintEqualToAnchor:subTitleView.trailingAnchor
                       constant:-kSubTitleHorizontalPadding],
    [menuButton.centerYAnchor
        constraintEqualToAnchor:numberOfTabsLabel.centerYAnchor],
  ]];

  return subTitleView;
}

// Displays the menu to rename and change the color of the currently displayed
// group.
- (void)displayEditionMenu {
  [_handler showTabGroupEditionForGroup:_tabGroup];
}

// Returns the tab group menu.
- (UIMenu*)configuredTabGroupMenu {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGroupViewEntry];

  __weak TabGroupViewController* weakSelf = self;
  UIAction* renameGroup = [actionFactory actionToRenameTabGroupWithBlock:^{
    [weakSelf displayEditionMenu];
  }];

  UIAction* newTabAction = [actionFactory actionToAddNewTabInGroupWithBlock:^{
      // TODO(crbug.com/1501837): Add new tab in current group and open it.
  }];
  newTabAction.image =
      DefaultSymbolWithPointSize(kPlusInSquareSymbol, kSymbolActionPointSize);

  UIAction* ungroupAction = [actionFactory actionToUngroupTabGroupWithBlock:^{
      // TODO(crbug.com/1501837): Remove the group but keep tabs and
      // dismiss the view.
  }];

  UIAction* closeGroupAction = [actionFactory actionToCloseTabGroupWithBlock:^{
      // TODO(crbug.com/1501837): Close all the tabs from the
      // current group, remove the group and dismiss the view.
  }];

  return
      [UIMenu menuWithTitle:@""
                   children:@[
                     renameGroup, newTabAction, ungroupAction, closeGroupAction
                   ]];
}

@end
