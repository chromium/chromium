// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_coordinator.h"

#import "base/metrics/histogram_macros.h"
#import "components/ui_metrics/sadtab_metrics_types.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/fullscreen/chrome_coordinator+fullscreen_disabling.h"
#import "ios/chrome/browser/web/model/sad_tab_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/web_state.h"

@interface SadTabCoordinator () <SadTabViewControllerDelegate,
                                 DependencyInstalling> {
  SadTabViewController* _viewController;
  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}
@end

@implementation SadTabCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, self.browser->GetWebStateList());
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (_viewController)
    return;

  if (self.repeatedFailure) {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabFeedbackHistogramKey,
                              ui_metrics::SadTabEvent::DISPLAYED,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(ui_metrics::kSadTabReloadHistogramKey,
                              ui_metrics::SadTabEvent::DISPLAYED,
                              ui_metrics::SadTabEvent::MAX_SAD_TAB_EVENT);
  }
  // Creates a fullscreen disabler.
  [self didStartFullscreenDisablingUI];

  _viewController = [[SadTabViewController alloc] init];
  _viewController.delegate = self;
  _viewController.overscrollDelegate = self.overscrollDelegate;
  _viewController.offTheRecord = self.browser->GetProfile()->IsOffTheRecord();
  _viewController.repeatedFailure = self.repeatedFailure;

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  _viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints([NamedGuide guideWithName:kContentAreaGuide
                                          view:self.baseViewController.view],
                     _viewController.view);
}

- (void)stop {
  if (!_viewController)
    return;

  [self didStopFullscreenDisablingUI];

  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

- (void)disconnect {
  // Deleting the installer bridge will cause all web states to have
  // dependencies uninstalled.
  _dependencyInstallerBridge.reset();
}

- (void)setOverscrollDelegate:
    (id<OverscrollActionsControllerDelegate>)delegate {
  _viewController.overscrollDelegate = delegate;
  _overscrollDelegate = delegate;
}

#pragma mark - SadTabViewDelegate

- (void)sadTabViewControllerShowReportAnIssue:
    (SadTabViewController*)sadTabViewController {
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  [static_cast<id<ApplicationCommands>>(self.browser->GetCommandDispatcher())
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::SadTab];
}

- (void)sadTabViewController:(SadTabViewController*)sadTabViewController
    showSuggestionsPageWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:URL
                   inIncognito:self.browser->GetProfile()->IsOffTheRecord()];

  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  [static_cast<id<ApplicationCommands>>(self.browser->GetCommandDispatcher())
      openURLInNewTab:command];
}

- (void)sadTabViewControllerReload:(SadTabViewController*)sadTabViewController {
  WebNavigationBrowserAgent::FromBrowser(self.browser)->Reload();
}

#pragma mark - SadTabTabHelperDelegate

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForWebState:(web::WebState*)webState
             repeatedFailure:(BOOL)repeatedFailure {
  if (!webState->IsVisible())
    return;

  self.repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDismissSadTab:(SadTabTabHelper*)tabHelper {
  [self stop];
}

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    didShowForRepeatedFailure:(BOOL)repeatedFailure {
  self.repeatedFailure = repeatedFailure;
  [self start];
}

- (void)sadTabTabHelperDidHide:(SadTabTabHelper*)tabHelper {
  [self stop];
}

#pragma mark - DependencyInstalling

- (void)installDependencyForWebState:(web::WebState*)webState {
  SadTabTabHelper::FromWebState(webState)->SetDelegate(self);
}

@end
