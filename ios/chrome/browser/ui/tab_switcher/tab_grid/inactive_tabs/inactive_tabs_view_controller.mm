// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"

#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InactiveTabsViewController () <UINavigationBarDelegate>

// The embedded navigation bar.
@property(nonatomic, readonly) UINavigationBar* navigationBar;

@end

@implementation InactiveTabsViewController

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  DCHECK(IsInactiveTabsEnabled());
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    _gridViewController = [[GridViewController alloc] init];
    _gridViewController.theme = GridThemeLight;
    _gridViewController.mode = TabGridModeInactive;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
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
  ]];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _gridViewController.gridView.contentInset =
      UIEdgeInsetsMake(CGRectGetMaxY(_navigationBar.frame), 0, 0, 0);
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

@end
