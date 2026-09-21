// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_coordinator.h"

#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_coordinator.h"
#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_coordinator_delegate.h"
#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_mediator.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface SiteSettingsCoordinator () <
    SiteSettingsCategoryDetailViewControllerDelegate,
    SiteSettingsTableViewControllerDelegate>
@end

@implementation SiteSettingsCoordinator {
  SiteSettingsMediator* _mediator;
  SiteSettingsTableViewController* _viewController;
  SiteSettingsCategoryDetailCoordinator* _categoryDetailCoordinator;
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

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[SiteSettingsTableViewController alloc] init];
  _viewController.delegate = self;

  _mediator = [[SiteSettingsMediator alloc]
      initWithHostContentSettingsMap:ios::HostContentSettingsMapFactory::
                                         GetForProfile(self.profile)];
  _mediator.consumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_categoryDetailCoordinator stop];
  _categoryDetailCoordinator = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - SiteSettingsTableViewControllerDelegate

- (void)siteSettingsTableViewControllerWasRemoved:
    (SiteSettingsTableViewController*)controller {
  [self.delegate siteSettingsCoordinatorWasRemoved:self];
}

- (void)siteSettingsTableViewController:
            (SiteSettingsTableViewController*)controller
                      didSelectCategory:(SiteSettingsCategory)category {
  [_categoryDetailCoordinator stop];
  _categoryDetailCoordinator = [[SiteSettingsCategoryDetailCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              category:category];
  _categoryDetailCoordinator.delegate = self;
  [_categoryDetailCoordinator start];
}

#pragma mark - SiteSettingsCategoryDetailViewControllerDelegate

- (void)siteSettingsCategoryDetailViewControllerWasRemoved:
    (SiteSettingsCategoryDetailViewController*)controller {
  [_categoryDetailCoordinator stop];
  _categoryDetailCoordinator = nil;
}

@end
