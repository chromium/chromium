// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_coordinator.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultPageModeCoordinator () <
    DefaultPageModeTableViewControllerDelegate>

@property(nonatomic, strong) DefaultPageModeTableViewController* viewController;
@property(nonatomic, strong) DefaultPageModeMediator* mediator;

@end

@implementation DefaultPageModeCoordinator

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
  HostContentSettingsMap* settingsMap =
      ios::HostContentSettingsMapFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator =
      [[DefaultPageModeMediator alloc] initWithSettingsMap:settingsMap];

  self.viewController = [[DefaultPageModeTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self;

  self.mediator.consumer = self.viewController;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - DefaultPageModeTableViewControllerDelegate

- (void)didSelectMode:(DefaultPageMode)selectedMode {
  [self.mediator setDefaultMode:selectedMode];
}

@end
