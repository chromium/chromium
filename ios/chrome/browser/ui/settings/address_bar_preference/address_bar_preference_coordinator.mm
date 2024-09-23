// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_mediator.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_view_controller.h"

@interface AddressBarPreferenceCoordinator ()
// View controller for the Address bar setting.
@property(nonatomic, strong) AddressBarPreferenceViewController* viewController;
// Address bar preference mediator.
@property(nonatomic, strong) AddressBarPreferenceMediator* mediator;

@end

@implementation AddressBarPreferenceCoordinator

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
  self.viewController = [[AddressBarPreferenceViewController alloc]
      initWithStyle:ChromeTableViewStyle()];

  self.mediator = [[AddressBarPreferenceMediator alloc] init];

  self.mediator.consumer = self.viewController;

  self.viewController.prefServiceDelegate = self.mediator;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [super stop];
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

@end
