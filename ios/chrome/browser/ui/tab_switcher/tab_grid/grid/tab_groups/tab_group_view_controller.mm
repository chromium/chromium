// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "base/i18n/time_formatting.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
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
constexpr CGFloat kPlusImageSize = 20;

constexpr CGFloat kSmallMotionTranslationCompletion = 0.8;
constexpr CGFloat kTranslationCompletion = 0;
constexpr CGFloat kSmallMotionOriginScale = 0.8;
constexpr CGFloat kOriginScale = 0.1;
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
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
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

- (void)prepareForPresentationWithSmallMotions:(BOOL)smallMotions {
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
  CGFloat translationCompletion =
      smallMotions ? kSmallMotionTranslationCompletion : kTranslationCompletion;
  CGFloat scale = smallMotions ? kSmallMotionOriginScale : kOriginScale;
  [_gridViewController centerVisibleCellsToPoint:center
                           translationCompletion:translationCompletion
                                       withScale:scale];
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
  [_gridViewController centerVisibleCellsToPoint:center
                           translationCompletion:kTranslationCompletion
                                       withScale:kOriginScale];
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
  [self openNewTab];
}

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  [_handler hideTabGroup];
  return NO;
}

- (void)navigationBar:(UINavigationBar*)navigationBar
           didPopItem:(UINavigationItem*)item {
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
  [_titleView setText:_groupTitle];
}

- (void)setGroupColor:(UIColor*)color {
  _groupColor = color;
  [_coloredDotView setBackgroundColor:_groupColor];
}

#pragma mark - Private

// Returns the navigation item which contain the back button.
- (UINavigationItem*)configuredBackButton {
  return [[UINavigationItem alloc] init];
}

// Returns the navigation item which contain the plus button.
- (UINavigationItem*)configuredPlusButton {
  UINavigationItem* plus = [[UINavigationItem alloc] init];
  UIImage* plusImage = DefaultSymbolWithPointSize(kPlusSymbol, kPlusImageSize);
  plus.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithImage:plusImage
                                       style:UIBarButtonItemStylePlain
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
  fullTitleView.layer.cornerRadius = kTitleBackgroundCornerRadius;
  fullTitleView.opaque = NO;

  _coloredDotView = [self groupColorDotView];
  _titleView = [self groupTitleView];
  [fullTitleView addSubview:_coloredDotView];
  [fullTitleView addSubview:_titleView];

  [NSLayoutConstraint activateConstraints:@[
    [_titleView.leadingAnchor
        constraintEqualToAnchor:_coloredDotView.trailingAnchor
                       constant:kDotTitleSeparationMargin],
    [_coloredDotView.centerYAnchor
        constraintEqualToAnchor:_titleView.centerYAnchor],
    [_coloredDotView.leadingAnchor
        constraintEqualToAnchor:fullTitleView.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [fullTitleView.trailingAnchor
        constraintEqualToAnchor:_titleView.trailingAnchor
                       constant:kTitleHorizontalMargin],
    [_titleView.topAnchor constraintEqualToAnchor:fullTitleView.topAnchor
                                         constant:kTitleVerticalMargin],
    [fullTitleView.bottomAnchor constraintEqualToAnchor:_titleView.bottomAnchor
                                               constant:kTitleVerticalMargin],
  ]];
  return fullTitleView;
}

// Returns the configured sub titles view.
- (UIView*)configuredSubTitle {
  UIView* subTitleView = [[UIView alloc] initWithFrame:CGRectZero];
  subTitleView.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton* menuButton = [[ExtendedTouchTargetButton alloc] init];
  menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  menuButton.menu = [self configuredTabGroupMenu];
  menuButton.showsMenuAsPrimaryAction = YES;
  [menuButton
      setImage:DefaultSymbolWithPointSize(kMenuSymbol, kThreeDotButtonSize)
      forState:UIControlStateNormal];
  menuButton.tintColor = UIColor.whiteColor;

  [subTitleView addSubview:menuButton];

  [NSLayoutConstraint activateConstraints:@[
    [menuButton.trailingAnchor
        constraintEqualToAnchor:subTitleView.trailingAnchor
                       constant:-kSubTitleHorizontalPadding],
    [subTitleView.heightAnchor constraintEqualToAnchor:menuButton.heightAnchor],
    [menuButton.centerYAnchor
        constraintEqualToAnchor:subTitleView.centerYAnchor],
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
      initWithScenario:kMenuScenarioHistogramTabGroupViewMenuEntry];

  __weak TabGroupViewController* weakSelf = self;
  UIAction* renameGroup = [actionFactory actionToRenameTabGroupWithBlock:^{
    [weakSelf displayEditionMenu];
  }];

  UIAction* newTabAction = [actionFactory actionToAddNewTabInGroupWithBlock:^{
    [weakSelf openNewTab];
  }];

  UIAction* ungroupAction = [actionFactory actionToUngroupTabGroupWithBlock:^{
    [weakSelf ungroup];
  }];

  UIAction* deleteGroupAction =
      [actionFactory actionToDeleteTabGroupWithBlock:^{
        [weakSelf deleteGroup];
      }];

  return
      [UIMenu menuWithTitle:@""
                   children:@[
                     renameGroup, newTabAction, ungroupAction, deleteGroupAction
                   ]];
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
  [self.mutator ungroup];
  [_handler hideTabGroup];
}

// Closes the tabs and deletes the current group and closes the view.
- (void)deleteGroup {
  [self.mutator deleteGroup];
  [_handler hideTabGroup];
}

@end
