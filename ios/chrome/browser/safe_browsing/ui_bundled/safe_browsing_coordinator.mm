// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/safe_browsing_coordinator.h"

#import "base/feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper_delegate.h"

@interface SafeBrowsingCoordinator () <SafeBrowsingTabHelperDelegate,
                                       TabsDependencyInstalling>

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, readonly) WebStateList* webStateList;

@end

@implementation SafeBrowsingCoordinator {
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webStateList = browser->GetWebStateList();
    _dependencyInstallerBridge.StartObserving(
        self, _webStateList,
        TabsDependencyInstaller::Policy::kAccordingToFeature);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)stop {
  // Stop observing the WebStateList before destroying the bridge object.
  _dependencyInstallerBridge.StopObserving();
  [super stop];
}

#pragma mark - SafeBrowsingTabHelperDelegate

- (void)openSafeBrowsingSettings {
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showSafeBrowsingSettings];
}

- (void)showEnhancedSafeBrowsingInfobar {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  std::unique_ptr<EnhancedSafeBrowsingInfobarDelegate> delegate =
      std::make_unique<EnhancedSafeBrowsingInfobarDelegate>(activeWebState,
                                                            settingsHandler);
  delegate->RecordInteraction(EnhancedSafeBrowsingInfobarInteraction::kViewed);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(activeWebState);

  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeEnhancedSafeBrowsing, std::move(delegate));
  infobar_manager->AddInfoBar(std::move(infobar), /*replace_existing=*/true);
}

#pragma mark - TabsDependencyInstalling

- (void)webStateInserted:(web::WebState*)webState {
  SafeBrowsingTabHelper::FromWebState(webState)->SetDelegate(self);
}

- (void)webStateRemoved:(web::WebState*)webState {
  SafeBrowsingTabHelper::FromWebState(webState)->RemoveDelegate();
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  // Nothing to do.
}

@end
