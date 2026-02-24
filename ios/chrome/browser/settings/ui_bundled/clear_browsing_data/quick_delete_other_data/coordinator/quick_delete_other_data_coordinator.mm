// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_coordinator.h"

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@implementation QuickDeleteOtherDataCoordinator {
  // The mediator for this coordinator.
  QuickDeleteOtherDataMediator* _mediator;
  // The view controller for this coordinator.
  QuickDeleteOtherDataViewController* _viewController;
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
  // The "Quick Delete Other Data" page is only available on the regular
  // browser.
  CHECK(!profile->IsOffTheRecord());

  _mediator = [[QuickDeleteOtherDataMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::GetForProfile(
                                        profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        profile)
                 templateURLService:ios::TemplateURLServiceFactory::
                                        GetForProfile(profile)];

  _viewController = [[QuickDeleteOtherDataViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _mediator.consumer = _viewController;
  _viewController.quickDeleteOtherDataHandler =
      self.quickDeleteOtherDataHandler;
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _viewController.quickDeleteOtherDataHandler = nil;
  _viewController = nil;

  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
}

@end
