// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"
#import "ios/chrome/browser/ui/page_info/page_info_site_security_mediator.h"
#import "ios/chrome/browser/ui/page_info/page_info_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoCoordinator ()

@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
@property(nonatomic, strong) CommandDispatcher* dispatcher;
@property(nonatomic, strong) PageInfoViewController* viewController;

@end

@implementation PageInfoCoordinator

@synthesize presentationProvider = _presentationProvider;

#pragma mark - ChromeCoordinator

- (void)start {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();

  PageInfoSiteSecurityDescription* siteSecurityDescription =
      [PageInfoSiteSecurityMediator configurationForWebState:webState];

  self.viewController = [[PageInfoViewController alloc]
      initWithSiteSecurityDescription:siteSecurityDescription];

  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;

  self.dispatcher = self.browser->GetCommandDispatcher();
  self.viewController.handler =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.dispatcher stopDispatchingToTarget:self];
  self.navigationController = nil;
  self.viewController = nil;
}

@end
