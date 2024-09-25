// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_view_controller.h"

@implementation SwitchProfileSettingsCoordinator {
  // View controller for the tabs settings.
  SwitchProfileSettingsTableViewController* _viewController;
  // The ProfileIOS instance passed to the initializer.
  raw_ptr<ProfileIOS> _profile;
  // Mediator for the switch profile settings.
  SwitchProfileSettingsMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _profile = browser->GetProfile();
  }
  return self;
}

- (void)start {
  NSString* activeProfileName =
      base::SysUTF8ToNSString(_profile->GetProfileName());
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(_profile);
  _mediator = [[SwitchProfileSettingsMediator alloc]
      initWithChromeAccountManagerService:accountManagerService
                        activeProfileName:activeProfileName];
  _viewController = [[SwitchProfileSettingsTableViewController alloc] init];
  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
