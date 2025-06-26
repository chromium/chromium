// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/content_settings_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/content_settings/content_settings_table_view_controller.h"

@interface ContentSettingsCoordinator () <
    ContentSettingsTableViewControllerPresentationDelegate>

@end

@implementation ContentSettingsCoordinator {
  ContentSettingsTableViewController* _viewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController =
      [[ContentSettingsTableViewController alloc] initWithBrowser:self.browser];
  _viewController.presentationDelegate = self;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

#pragma mark - ContentSettingsTableViewControllerPresentationDelegate

- (void)contentSettingsTableViewControllerWasRemoved:
    (ContentSettingsTableViewController*)controller {
  [self.delegate contentSettingsCoordinatorViewControllerWasRemoved:self];
}

@end
