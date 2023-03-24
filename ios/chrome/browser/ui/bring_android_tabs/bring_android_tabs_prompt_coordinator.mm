// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_mediator.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BringAndroidTabsPromptCoordinator {
  // Mediator that updates Chromium model objects; serves as a delegate to the
  // view controller.
  BringAndroidTabsPromptMediator* _mediator;
}

- (void)start {
  BringAndroidTabsToIOSService* service =
      BringAndroidTabsToIOSServiceFactory::GetForBrowserStateIfExists(
          self.browser->GetBrowserState());
  _mediator = [[BringAndroidTabsPromptMediator alloc]
      initWithBringAndroidTabsService:service
                            URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                          self.browser)];

  // TODO(crbug.com/1418117): Create view controller for the prompts.
  /*self.viewController = [UIViewController
      initWithNumber:static_cast<int>(service->GetNumberOfAndroidTabs())
            delegate:_mediator
      commandHandler:HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                        BringAndroidTabsCommands)];*/
}

- (void)stop {
  // The view controller should have already dismissed itself using the
  // Bring Android Commands handler.
  DCHECK(self.viewController);
  DCHECK(self.viewController.beingDismissed ||
         self.viewController.parentViewController == nil);
  self.viewController = nil;
  // Remove the mediator.
  DCHECK(_mediator);
  _mediator = nil;
}

@end
