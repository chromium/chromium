// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_fragments/text_fragments_coordinator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/ios/shared_highlighting_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/text_fragments/text_fragments_mediator.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

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
}

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState
                              withSender:(CGRect)rect
                                withText:(NSString*)text {
  ActionSheetCoordinator* actionSheet = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:[self baseViewController]
                         browser:[self browser]
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE)
                         message:nil
                            rect:rect
                            view:[self.baseViewController view]];

  [actionSheet
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SHARED_HIGHLIGHT_LEARN_MORE)
                action:^{
                  id<ApplicationCommands> handler =
                      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         ApplicationCommands);
                  [handler openURLInNewTab:[OpenNewTabCommand
                                               commandWithURLFromChrome:
                                                   GURL(shared_highlighting::
                                                            kLearnMoreUrl)]];
                }
                 style:UIAlertActionStyleDefault];
  [actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_RESHARE)
                action:^{
                  id<ActivityServiceCommands> handler =
                      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         ActivityServiceCommands);

                  auto* webState =
                      self.browser->GetWebStateList()->GetActiveWebState();

                  ShareHighlightCommand* command =
                      [[ShareHighlightCommand alloc]
                           initWithURL:webState->GetLastCommittedURL()
                                 title:base::SysUTF16ToNSString(
                                           webState->GetTitle())
                          selectedText:text
                            sourceView:webState->GetView()
                            sourceRect:rect];

                  [handler shareHighlight:command];
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
