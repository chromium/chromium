// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/vcard_coordinator.h"

#include "base/scoped_observation.h"
#import "ios/chrome/browser/download/vcard_tab_helper.h"
#import "ios/chrome/browser/download/vcard_tab_helper_delegate.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface VcardCoordinator () <DependencyInstalling, VcardTabHelperDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

@end

@implementation VcardCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before |browser| and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  if (VcardTabHelper::FromWebState(webState)) {
    VcardTabHelper::FromWebState(webState)->set_delegate(self);
  }
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  if (VcardTabHelper::FromWebState(webState)) {
    VcardTabHelper::FromWebState(webState)->set_delegate(nil);
  }
}

#pragma mark - VcardTabHelperDelegate

- (void)openVcardFromData:(NSData*)vcardData {
  DCHECK(vcardData);
  // TODO(crbug.com/1244002): Open Vcard with CNContactVCardSerialization.
}

@end
