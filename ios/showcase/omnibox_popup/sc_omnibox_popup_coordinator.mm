// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_coordinator.h"

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/showcase/common/coordinator.h"
#import "ios/showcase/common/protocol_alerter.h"
#import "ios/showcase/omnibox_popup/sc_omnibox_popup_container_view_controller.h"
#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCOmniboxPopupCoordinator () <Coordinator>

@property(nonatomic, strong)
    OmniboxPopupBaseViewController* popupViewController;
@property(nonatomic, strong)
    SCOmniboxPopupContainerViewController* containerViewController;
@property(nonatomic, strong) SCOmniboxPopupMediator* mediator;

@property(nonatomic, strong) ProtocolAlerter* alerter;

@end

@implementation SCOmniboxPopupCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.alerter = [[ProtocolAlerter alloc]
      initWithProtocols:@[ @protocol(AutocompleteResultConsumerDelegate) ]];

  // Ignore didScroll because it's fired all the time.
  [self.alerter ignoreSelector:@selector(autocompleteResultConsumerDidScroll:)];

  self.popupViewController = [[OmniboxPopupViewController alloc] init];
  self.popupViewController.delegate =
      static_cast<id<AutocompleteResultConsumerDelegate>>(self.alerter);

  self.mediator = [[SCOmniboxPopupMediator alloc]
      initWithConsumer:self.popupViewController];
  [self.mediator updateMatches];

  self.containerViewController = [[SCOmniboxPopupContainerViewController alloc]
      initWithPopupViewController:self.popupViewController];

  self.alerter.baseViewController = self.containerViewController;

  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

@end
