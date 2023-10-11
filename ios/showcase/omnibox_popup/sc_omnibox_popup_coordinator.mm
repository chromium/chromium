// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_coordinator.h"

#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/showcase/common/coordinator.h"
#import "ios/showcase/omnibox_popup/sc_omnibox_popup_container_view_controller.h"
#import "ios/showcase/omnibox_popup/sc_omnibox_popup_mediator.h"
#import "ios/testing/protocol_fake.h"

@interface SCOmniboxPopupCoordinator () <Coordinator>

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong)
    SCOmniboxPopupContainerViewController* containerViewController;
@property(nonatomic, strong) SCOmniboxPopupMediator* mediator;

@property(nonatomic, strong) ProtocolFake* alerter;

@end

@implementation SCOmniboxPopupCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.alerter = [[ProtocolFake alloc]
      initWithProtocols:@[ @protocol(AutocompleteResultConsumerDelegate) ]];

  // Ignore didScroll because it's fired all the time.
  [self.alerter ignoreSelector:@selector(autocompleteResultConsumerDidScroll:)];

  self.popupViewController = [[OmniboxPopupViewController alloc] init];
  self.popupViewController.delegate =
      static_cast<id<AutocompleteResultConsumerDelegate>>(self.alerter);
  self.popupViewController.layoutGuideCenter = [[LayoutGuideCenter alloc] init];

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
