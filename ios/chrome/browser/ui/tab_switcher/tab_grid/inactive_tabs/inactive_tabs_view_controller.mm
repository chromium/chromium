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
@end

@implementation InactiveTabsViewController

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  DCHECK(IsInactiveTabsEnabled());
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    _gridViewController = [[GridViewController alloc] init];
    _gridViewController.theme = GridThemeLight;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.blackColor;

  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  NSString* title = l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_TITLE);
  navigationBar.items = @[
    [[UINavigationItem alloc] init],  // To have a Back button.
    [[UINavigationItem alloc] initWithTitle:title],
  ];
  navigationBar.barStyle = UIBarStyleBlack;
  navigationBar.translucent = NO;
  navigationBar.delegate = self;
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:navigationBar];

  UIView* gridView = _gridViewController.view;
  gridView.translatesAutoresizingMaskIntoConstraints = NO;
  gridView.accessibilityIdentifier = kInactiveTabGridIdentifier;
  [self addChildViewController:_gridViewController];
  [self.view addSubview:gridView];
  [_gridViewController didMoveToParentViewController:self];

  [NSLayoutConstraint activateConstraints:@[
    [navigationBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [gridView.topAnchor constraintEqualToAnchor:navigationBar.bottomAnchor],
    [gridView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [gridView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [gridView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

#pragma mark - UINavigationBarDelegate

- (BOOL)navigationBar:(UINavigationBar*)navigationBar
        shouldPopItem:(UINavigationItem*)item {
  [self.delegate inactiveTabsViewControllerDidTapBackButton:self];
  return NO;
}

@end
