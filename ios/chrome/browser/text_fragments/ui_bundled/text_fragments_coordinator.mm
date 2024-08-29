// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_fragments/ui_bundled/text_fragments_coordinator.h"

#import <memory>

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/shared_highlighting/core/common/fragment_directives_utils.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/shared_highlighting/ios/shared_highlighting_constants.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"
#import "ios/chrome/browser/text_fragments/ui_bundled/text_fragments_mediator.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/text_fragments/text_fragments_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"
#import "url/gurl.h"

@interface TextFragmentsCoordinator () <DependencyInstalling,
                                        TextFragmentsDelegate,
                                        CRWWebStateObserver>

@property(nonatomic, strong, readonly) TextFragmentsMediator* mediator;

@property(nonatomic, strong) ActionSheetCoordinator* actionSheet;

@end

@implementation TextFragmentsCoordinator {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;

  // Used to observe the active WebState
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _mediator = [[TextFragmentsMediator alloc] initWithConsumer:self];
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        browser->GetWebStateList(), _webStateObserverBridge.get());
  }
  return self;
}

#pragma mark - TextFragmentsDelegate methods

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState {
}

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState
                              withSender:(CGRect)rect
                                withText:(NSString*)text
                           withFragments:
                               (std::vector<shared_highlighting::TextFragment>)
                                   fragments {
  base::RecordAction(base::UserMetricsAction("TextFragments.Menu.Opened"));

  self.actionSheet = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:[self baseViewController]
                         browser:[self browser]
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE)
                         message:nil
                            rect:rect
                            view:[self.baseViewController view]];

  __weak TextFragmentsCoordinator* weakSelf = self;
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_REMOVE)
                action:^{
                  base::RecordAction(
                      base::UserMetricsAction("TextFragments.Menu.Removed"));
                  [weakSelf.mediator removeTextFragmentsInWebState:webState];
                  [weakSelf dismissActionSheet];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARED_HIGHLIGHT_RESHARE)
                action:^{
                  base::RecordAction(
                      base::UserMetricsAction("TextFragments.Menu.Reshared"));
                  id<ActivityServiceCommands> handler = HandlerForProtocol(
                      weakSelf.browser->GetCommandDispatcher(),
                      ActivityServiceCommands);

                  auto* activeWebState =
                      weakSelf.browser->GetWebStateList()->GetActiveWebState();

                  // Take the fragments from the vector and put them into the
                  // last committed URL. In most cases this will yield the same
                  // URL, but it's necessary in case same-document navigation
                  // has cleared the fragments from the URL.
                  GURL sharingURL =
                      shared_highlighting::AppendFragmentDirectives(
                          activeWebState->GetLastCommittedURL(), fragments);

                  ShareHighlightCommand* command =
                      [[ShareHighlightCommand alloc]
                           initWithURL:sharingURL
                                 title:base::SysUTF16ToNSString(
                                           activeWebState->GetTitle())
                          selectedText:text
                            sourceView:activeWebState->GetView()
                            sourceRect:rect];

                  [handler shareHighlight:command];
                  [weakSelf dismissActionSheet];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheet
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SHARED_HIGHLIGHT_LEARN_MORE)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "TextFragments.Menu.LearnMoreOpened"));
                  id<ApplicationCommands> handler = HandlerForProtocol(
                      weakSelf.browser->GetCommandDispatcher(),
                      ApplicationCommands);
                  [handler openURLInNewTab:[OpenNewTabCommand
                                               commandWithURLFromChrome:
                                                   GURL(shared_highlighting::
                                                            kLearnMoreUrl)]];
                  [weakSelf dismissActionSheet];
                }
                 style:UIAlertActionStyleDefault];
  [self.actionSheet addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                              action:^{
                                [weakSelf dismissActionSheet];
                              }
                               style:UIAlertActionStyleCancel];
  [self.actionSheet start];
}

#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  [self.mediator registerWithWebState:webState];
}

#pragma mark - ChromeCoordinator methods

- (void)stop {
  [self dismissActionSheet];
  // Reset this observer manually. We want this to go out of scope now, ensuring
  // it detaches before `browser` and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  [self dismissActionSheet];
}

#pragma mark - Private

- (void)dismissActionSheet {
  [self.actionSheet stop];
  self.actionSheet = nil;
}

@end
