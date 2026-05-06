// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/inactive_tabs/inactive_tabs_grid_view_controller.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbar_background_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface InactiveTabsViewController () <UINavigationBarDelegate,
                                          LayoutStateObserver>

// The embedded navigation bar.
@property(nonatomic, readonly) UINavigationBar* navigationBar;

// The embedded bottom toolbar bar.
@property(nonatomic, readonly) UIToolbar* bottomBar;

// The Close All Inactive button.
@property(nonatomic, readonly) UIBarButtonItem* closeAllInactiveButton;

@end

@implementation InactiveTabsViewController {
  // The bottom constraint for the bottom bar.
  NSLayoutConstraint* _bottomBarBottomConstraint;

  // The gradient background view.
  TabGridToolbarBackgroundView* _gradientBackgroundView;
}

- (void)dealloc {
  [_layoutState removeObserver:self];
}

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
  [self updateBottomBarConstraints];
}

#pragma mark - LayoutStateObserver

- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self updateBottomBarConstraints];
}

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    _gridViewController = [[InactiveGridViewController alloc] init];
    _gridViewController.theme = GridTheme::kDynamic;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityViewIsModal = YES;
  self.view.backgroundColor = UIColor.blackColor;

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  gridView.accessibilityIdentifier = kInactiveTabGridIdentifier;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];
  [_gridViewController didMoveToParentViewController:self];

  _navigationBar = [[UINavigationBar alloc] init];
  NSString* title = l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_TITLE);
  _navigationBar.items = @[
    [[UINavigationItem alloc] init],  // To have a Back button.
    [[UINavigationItem alloc] initWithTitle:title],
  ];
  _navigationBar.barStyle = UIBarStyleBlack;
  _navigationBar.translucent = YES;
  _navigationBar.delegate = self;
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_navigationBar];

  // Add the bottom toolbar with the Close All Inactive button.
  _bottomBar = [[UIToolbar alloc] init];
  _bottomBar.barStyle = UIBarStyleBlack;
  _bottomBar.translucent = YES;
  _bottomBar.tintColor = [UIColor colorNamed:kRed500Color];
  _bottomBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_bottomBar];

  [NSLayoutConstraint activateConstraints:@[
    [gridView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [gridView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [gridView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [gridView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [_navigationBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_bottomBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_bottomBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
  _bottomBarBottomConstraint = [_bottomBar.bottomAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  _bottomBarBottomConstraint.active = YES;

  if (IsChromeNextIaEnabled()) {
    _gradientBackgroundView = [[TabGridToolbarBackgroundView alloc]
        initWithPosition:TabGridToolbarBackgroundPosition::kBottom];
    _gradientBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view insertSubview:_gradientBackgroundView belowSubview:_bottomBar];

    [NSLayoutConstraint activateConstraints:@[
      [_gradientBackgroundView.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_gradientBackgroundView.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
      [_gradientBackgroundView.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
      [_gradientBackgroundView.topAnchor
          constraintEqualToAnchor:_bottomBar.bottomAnchor],
    ]];

    _bottomBarBottomConstraint = [_bottomBar.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
    _bottomBarBottomConstraint.active = YES;
  }

  // Let the bottom bar lay itself out before setting the items, as otherwise it
  // spits out AutoLayout constraints conflicts.
  [_bottomBar layoutIfNeeded];
  NSString* buttonTitle =
      l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_CLOSE_ALL_BUTTON);
  __weak __typeof(self) weakSelf = self;
  UIAction* closeAllInactiveAction =
      [UIAction actionWithTitle:buttonTitle
                          image:nil
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakSelf didTapCloseAllInactive];
                        }];
  _closeAllInactiveButton =
      [[UIBarButtonItem alloc] initWithPrimaryAction:closeAllInactiveAction];
  _closeAllInactiveButton.accessibilityIdentifier =
      kInactiveTabGridCloseAllButtonIdentifier;
  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  _bottomBar.items = @[ flexibleSpace, _closeAllInactiveButton, flexibleSpace ];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  CGFloat topInset =
      CGRectGetMaxY(_navigationBar.frame) - CGRectGetMinY(self.view.bounds);
  CGFloat bottomInset =
      CGRectGetMaxY(self.view.bounds) - CGRectGetMinY(_bottomBar.frame);
  CGFloat leftInset = self.view.safeAreaInsets.left;
  CGFloat rightInset = self.view.safeAreaInsets.right;

  _gridViewController.contentInsets =
      UIEdgeInsetsMake(topInset, leftInset, bottomInset, rightInset);
  if (IsChromeNextIaEnabled()) {
    [self updateBottomBarConstraints];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _gridViewController.view);
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  nil);
}

#pragma mark - UIBarPositioningDelegate

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  // Let the background of the navigation bar extend to the top, behind the
  // Dynamic Island or notch.
  return UIBarPositionTopAttached;
}

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  [self.delegate inactiveTabsViewControllerDidTapBackButton:self];
  return NO;
}

#pragma mark - Private

// Called when the user tapped the Close All Inactive button.
- (void)didTapCloseAllInactive {
  [self.delegate inactiveTabsViewController:self
        didTapCloseAllInactiveBarButtonItem:self.closeAllInactiveButton];
}

// Updates the bottom bar constraints based on the App Bar position.
- (void)updateBottomBarConstraints {
  _bottomBarBottomConstraint.constant =
      self.layoutState.appBarPosition == AppBarPosition::kBottom
          ? -kAppBarHeight
          : 0;
}

@end
