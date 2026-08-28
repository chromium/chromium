// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_coordinator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface SitePermissionsCoordinator () <
    SitePermissionsTableViewControllerDelegate>

@end

@implementation SitePermissionsCoordinator {
  SitePermissionsTableViewController* _viewController;
  SitePermissionsMediator* _mediator;
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
  ProfileIOS* profile = self.profile;
  HostContentSettingsMap* settingsMap =
      ios::HostContentSettingsMapFactory::GetForProfile(profile);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);

  _mediator = [[SitePermissionsMediator alloc]
      initWithHostContentSettingsMap:settingsMap
                       faviconLoader:faviconLoader];

  _viewController = [[SitePermissionsTableViewController alloc] init];
  _viewController.delegate = self;
  _viewController.imageDataSource = _mediator;

  _mediator.consumer = _viewController;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - SitePermissionsTableViewControllerDelegate

- (void)sitePermissionsTableViewControllerWasRemoved:
    (SitePermissionsTableViewController*)controller {
  [self.delegate sitePermissionsCoordinatorWasRemoved:self];
}

- (void)sitePermissionsTableViewController:
            (SitePermissionsTableViewController*)controller
                             didSelectSite:(SitePermissionsSiteItem*)siteItem {
  // TODO(crbug.com/552561356): Navigate to site permissions detail screen.
}

@end
