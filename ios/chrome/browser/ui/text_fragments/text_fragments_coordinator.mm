// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"

#import <memory>

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_mediator.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TextFragmentsCoordinator () <DependencyInstalling,
                                        TextFragmentsDelegate>

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
    _mediator = [[TextFragmentsMediator alloc] initWithConsumer:self];
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

#pragma mark - TextFragmentsDelegate methods

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState {
  // TODO(crbug.com/1267933): This works for phones, but for tablets the
  //     alignment of the bubble is wrong. The values used for the rect need
  //     to be piped through from the web layer, rather than the arbitrary
  //     numbers currently used.
  ActionSheetCoordinator* actionSheet = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:[self baseViewController]
                         browser:[self browser]
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE)
                         message:nil
                            rect:CGRectMake(0, 0, 100, 100)
                            view:[self.baseViewController view]];

  // TODO(crbug.com/1281931): The Learn More and Reshare options are currently
  //     no-ops. This functionality will be implemented in a follow-up patch.
  [actionSheet addItemWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_SHARED_HIGHLIGHT_LEARN_MORE)
                         action:^{
                         }
                          style:UIAlertActionStyleDefault];
  [actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_RESHARE)
                action:^{
                }
                 style:UIAlertActionStyleDefault];
  [actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_REMOVE)
                action:^{
                  [self.mediator removeTextFragmentsInWebState:webState];
                }
                 style:UIAlertActionStyleDestructive];
  [actionSheet start];
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
