// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_coordinator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"

@interface DefaultPageModeCoordinator ()

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
      ios::HostContentSettingsMapFactory::GetForProfile(
          self.browser->GetProfile());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());

  self.mediator =
      [[DefaultPageModeMediator alloc] initWithSettingsMap:settingsMap
                                                   tracker:tracker];

  self.viewController = [[DefaultPageModeTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self.mediator;

  self.mediator.consumer = self.viewController;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

@end
