// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_security_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_security_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/web/public/web_state.h"

@implementation PageInfoSecurityCoordinator {
  PageInfoSecurityViewController* _viewController;
  PageInfoSiteSecurityDescription* _siteSecurityDescription;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                         siteSecurityDescription:
                             (PageInfoSiteSecurityDescription*)
                                 siteSecurityDescription {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _siteSecurityDescription = siteSecurityDescription;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PageInfoSecurityViewController alloc]
      initWithSiteSecurityDescription:_siteSecurityDescription];

  _viewController.pageInfoCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PageInfoCommands);
  _viewController.pageInfoPresentationHandler =
      self.pageInfoPresentationHandler;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  _viewController.pageInfoCommandsHandler = nil;
  _viewController.pageInfoPresentationHandler = nil;
  _viewController = nil;
}

@end
