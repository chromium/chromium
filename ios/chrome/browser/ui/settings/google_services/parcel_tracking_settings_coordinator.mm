// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_view_controller.h"

@implementation ParcelTrackingSettingsCoordinator {
  ParcelTrackingSettingsViewController* _viewController;
  ParcelTrackingSettingsMediator* _mediator;
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
  _viewController = [[ParcelTrackingSettingsViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _mediator = [[ParcelTrackingSettingsMediator alloc]
      initWithPrefs:self.browser->GetProfile()->GetPrefs()];

  _mediator.consumer = _viewController;
  _viewController.modelDelegate = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
  [_mediator disconnect];
  _mediator = nil;
}

@end
