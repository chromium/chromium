// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/content_settings/reader_mode_settings_coordinator.h"

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/content_settings/reader_mode_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation ReaderModeSettingsCoordinator {
  ReaderModeSettingsTableViewController* _viewController;
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
  DistillerService* distillerService =
      DistillerServiceFactory::GetForProfile(self.browser->GetProfile());
  _viewController = [[ReaderModeSettingsTableViewController alloc]
      initWithDistilledPagePrefs:distillerService->GetDistilledPagePrefs()
                     prefService:self.browser->GetProfile()->GetPrefs()];

  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
