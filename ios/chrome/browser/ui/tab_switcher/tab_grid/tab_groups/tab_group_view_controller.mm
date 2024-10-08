// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "base/i18n/time_formatting.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_group_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Background.
constexpr CGFloat kBackgroundAlpha = 0.6;

// Button.
constexpr CGFloat kPlusImageSize = 20;

// Animation.
constexpr CGFloat kTranslationCompletion = 0;
constexpr CGFloat kOriginScale = 0.1;

// Navigation bar.
constexpr CGFloat kDotSize = 12;
constexpr CGFloat kSpace = 8;

}  // namespace

@interface TabGroupViewController () <TabGridToolbarsGridDelegate,
                                      UINavigationBarDelegate>
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
  // Whether this is shared with other users.
  BOOL _shared;
  // The bottom toolbar.
  TabGridBottomToolbar* _bottomToolbar;
}

#pragma mark - Public

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler
                      incognito:(BOOL)incognito
                         shared:(BOOL)shared
                       tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group view controller outside "
         "the Tab Groups experiment.";
  CHECK(tabGroup);
  if ((self = [super init])) {
    _handler = handler;
    _incognito = incognito;
    _shared = shared;
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

  [self configureNavigationBar];

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];

  [self updateGridInsets];

  [_gridViewController didMoveToParentViewController:self];

  [NSLayoutConstraint activateConstraints:@[
    [gridView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [gridView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [gridView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [gridView.topAnchor constraintEqualToAnchor:_navigationBar.bottomAnchor],
  ]];

  // Add the toolbar after the grid to make sure it is above it.
  [self configureBottomToolbar];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                       withAction:@selector(updateGridInsets)];
    [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.self ]
                       withAction:@selector(updateGridInsets)];
  }
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)didTapPlusButton {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridTabGroupCreateNewTab"));
  [self openNewTab];
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

#pragma mark - Private

// Returns the navigation item which contain the back button.
- (UINavigationItem*)configuredBackButton {
  UINavigationItem* back = [[UINavigationItem alloc] init];
  back.title = @"";
  return back;
}

// Returns the navigation item which contain the plus button and the overflow
// menu.
- (UINavigationItem*)configuredRightNavigationItems {
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];

  UIImage* threeDotImage =
      DefaultSymbolWithPointSize(kMenuSymbol, kPlusImageSize);
  UIBarButtonItem* dotsItem =
      [[UIBarButtonItem alloc] initWithImage:threeDotImage
                                        menu:[self configuredTabGroupMenu]];
  dotsItem.accessibilityIdentifier = kTabGroupOverflowMenuButtonIdentifier;
  dotsItem.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_TAB_GROUP_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL);

  if (IsTabGroupIndicatorEnabled()) {
    navigationItem.rightBarButtonItems = @[ dotsItem ];
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

    navigationItem.rightBarButtonItems = @[ dotsItem, plusItem ];
  }
  return navigationItem;
}

// Returns the navigation item which contain the group title, color and the
// right navigation items.
- (UINavigationItem*)configuredGroupItem {
  UINavigationItem* navigationItem = [[UINavigationItem alloc] init];

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

  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kDotSize / 2;
  dotView.backgroundColor = _groupColor;

  UIView* titleView = [[UIView alloc] init];
  titleView.translatesAutoresizingMaskIntoConstraints = NO;

  [titleView addSubview:dotView];
  [titleView addSubview:titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.leadingAnchor constraintEqualToAnchor:dotView.trailingAnchor
                                             constant:kSpace],
    [dotView.centerYAnchor constraintEqualToAnchor:titleView.centerYAnchor],
    [dotView.leadingAnchor constraintEqualToAnchor:titleView.leadingAnchor
                                          constant:-kDotSize - kSpace],
    [titleLabel.trailingAnchor
        constraintEqualToAnchor:titleView.trailingAnchor],
    [titleLabel.topAnchor constraintEqualToAnchor:titleView.topAnchor],
    [titleLabel.bottomAnchor constraintEqualToAnchor:titleView.bottomAnchor],
    [dotView.heightAnchor constraintEqualToConstant:kDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kDotSize],
  ]];

  _titleView = titleView;
  _titleLabel = titleLabel;
  _coloredDotView = dotView;
  navigationItem.titleView = titleView;
  navigationItem.titleView.hidden = YES;
  navigationItem.rightBarButtonItems =
      [self configuredRightNavigationItems].rightBarButtonItems;
  return navigationItem;
}

// Configures the navigation bar.
- (void)configureNavigationBar {
  _navigationBar = [[UINavigationBar alloc] init];
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  _navigationBar.items =
      @[ [self configuredBackButton], [self configuredGroupItem] ];

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

// Adds the bottom toolbar containing the "plus" button.
- (void)configureBottomToolbar {
  if (!IsTabGroupIndicatorEnabled()) {
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

  [self.view addSubview:bottomToolbar];

  [NSLayoutConstraint activateConstraints:@[
    [bottomToolbar.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
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

  [menuElements addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
                  [weakSelf displayEditionMenu];
                }]];

  [menuElements addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
                  [weakSelf openNewTab];
                }]];

  if (_shared) {
    [menuElements addObject:[actionFactory actionToShowRecentActivity:^{
                    [weakSelf showRecentActivity];
                  }]];

    // TODO(crbug.com/358533115): Add an entry point to the management UI.
  }

  [menuElements addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
                  [weakSelf ungroup];
                }]];

  if (IsTabGroupSyncEnabled()) {
    [menuElements addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
                    [weakSelf closeGroup];
                  }]];
    if (!_incognito) {
      [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                      [weakSelf deleteGroup];
                    }]];
    }
  } else {
    [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                    [weakSelf deleteGroup];
                  }]];
  }

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

// Shows the recent activity of a shared tab group.
- (void)showRecentActivity {
  CHECK(_shared);
  // TODO(crbug.com/370897655): Create a half sheet with logs and show it.
}

// Updates the safe area inset of the grid based on this VC safe areas and the
// bottom toolbar, except the top one as the grid is below a toolbar.
- (void)updateGridInsets {
  CGFloat bottomToolbarInset = 0;
  if (IsTabGroupIndicatorEnabled()) {
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

@end
