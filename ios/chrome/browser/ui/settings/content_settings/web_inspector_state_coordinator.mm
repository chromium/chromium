// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_mediator.h"
#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_table_view_controller.h"

@interface WebInspectorStateCoordinator ()

@property(nonatomic, strong)
    WebInspectorStateTableViewController* viewController;
@property(nonatomic, strong) WebInspectorStateMediator* mediator;

@end

@implementation WebInspectorStateCoordinator

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
  self.mediator = [[WebInspectorStateMediator alloc]
      initWithUserPrefService:self.browser->GetProfile()->GetPrefs()];

  self.viewController = [[WebInspectorStateTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.delegate = self.mediator;

  self.mediator.consumer = self.viewController;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
}

@end
