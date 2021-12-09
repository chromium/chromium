// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"

#import <memory>

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_mediator.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextFragmentsCoordinator () <DependencyInstalling>

@property(nonatomic, strong, readonly) TextFragmentsMediator* mediator;

@end

@implementation TextFragmentsCoordinator {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _mediator = [[TextFragmentsMediator alloc] init];
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  [self.mediator registerWithWebState:webState];
}

#pragma mark - ChromeCoordinator methods

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, ensuring
  // it detaches before |browser| and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();
}

@end
