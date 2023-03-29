// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/tab_list_from_android_coordinator.h"

#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/bring_android_tabs/tab_list_from_android_mediator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabListFromAndroidCoordinator {
  // Main mediator for this coordinator.
  TabListFromAndroidMediator* _mediator;

  // Main view controller for this coordinator.
  ChromeTableViewController* _viewController;
}

- (void)start {
  BringAndroidTabsToIOSService* service =
      BringAndroidTabsToIOSServiceFactory::GetForBrowserStateIfExists(
          self.browser->GetBrowserState());
  _mediator = [[TabListFromAndroidMediator alloc]
      initWithBringAndroidTabsService:service
                            URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                          self.browser)
                        faviconLoader:IOSChromeFaviconLoaderFactory::
                                          GetForBrowserState(
                                              self.browser->GetBrowserState())];

  // TODO(crbug.com/1418120): Create table view controller.
  /*_viewController = [ChromeTableViewController
      initWithBringAndroidTabsService:service
            delegate:_mediator
      commandHandler:HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                        BringAndroidTabsCommands)];*/
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _mediator = nil;
  _viewController = nil;
}

@end
