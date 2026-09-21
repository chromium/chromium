// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_coordinator.h"

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_mediator.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation SiteSettingsCategoryDetailCoordinator {
  SiteSettingsCategory _category;
  SiteSettingsCategoryDetailMediator* _mediator;
  SiteSettingsCategoryDetailViewController* _viewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        category:
                                            (SiteSettingsCategory)category {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _category = category;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ContentSettingsType type =
      ContentSettingsTypeFromSiteSettingsCategory(_category);

  _viewController = [[SiteSettingsCategoryDetailViewController alloc]
      initWithCategory:_category];
  _viewController.delegate = self.delegate;

  ProfileIOS* profile = self.profile;
  _mediator = [[SiteSettingsCategoryDetailMediator alloc]
      initWithHostContentSettingsMap:ios::HostContentSettingsMapFactory::
                                         GetForProfile(profile)
                       faviconLoader:IOSChromeFaviconLoaderFactory::
                                         GetForProfile(profile)
                 contentSettingsType:type];

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.imageDataSource = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController.mutator = nil;
  _viewController.imageDataSource = nil;
  _viewController = nil;
}

@end
