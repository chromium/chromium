// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/safe_browsing_coordinator.h"

#import "base/feature_list.h"
#import "base/memory/weak_ptr.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/enhanced_safe_browsing_infobar_delegate.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper_delegate.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/ui_bundled/suspicious_site_warning_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper_delegate.h"
#import "ios/web/public/web_state.h"

// Forward-declaration of the private method so subsequent code can call it.
@interface SafeBrowsingCoordinator () <
    SuspiciousSiteTabHelperDelegate,
    SuspiciousSiteWarningViewControllerDelegate,
    TabsDependencyInstalling>

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, readonly) WebStateList* webStateList;

- (void)handlePreferenceChange;
- (void)showSafeBrowsingSyncInfobarForState:(BOOL)isEnhancedProtectionEnabled
                                  withEmail:(const std::string&)email;

@end

@implementation SafeBrowsingCoordinator {
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
  PrefChangeRegistrar _prefChangeRegistrar;
  SuspiciousSiteWarningViewController* _suspiciousSiteWarningViewController;
  base::WeakPtr<web::WebState> _suspiciousSiteWebState;
  BOOL _pendingBannerPreferenceChange;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webStateList = browser->GetWebStateList();
    _dependencyInstallerBridge.StartObserving(self, browser);

    // Initialize observations.
    _prefChangeRegistrar.Init(browser->GetProfile()->GetPrefs());
    __weak __typeof(self) weakSelf = self;
    _prefChangeRegistrar.Add(prefs::kSafeBrowsingEnabled, base::BindRepeating(^{
                               [weakSelf handlePreferenceChange];
                             }));
    _prefChangeRegistrar.Add(prefs::kSafeBrowsingEnhanced,
                             base::BindRepeating(^{
                               [weakSelf handlePreferenceChange];
                             }));
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)stop {
  if (_suspiciousSiteWarningViewController) {
    [self handleSuspiciousSiteWarningDismissedBySystem];
  }
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
      std::make_unique<EnhancedSafeBrowsingInfobarDelegate>(
          activeWebState, settingsHandler,
          EnhancedSafeBrowsingInfobarScenario::kAccountSync, /*email=*/"");
  delegate->RecordInteraction(EnhancedSafeBrowsingInfobarInteraction::kViewed);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(activeWebState);

  std::unique_ptr<infobars::InfoBar> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeEnhancedSafeBrowsing, std::move(delegate));
  infobar_manager->AddInfoBar(std::move(infobar), /*replace_existing=*/true);
}

#pragma mark - SuspiciousSiteTabHelperDelegate

- (BOOL)showSuspiciousSiteWarning {
  [self dismissSuspiciousSiteWarning];

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  // Do not present if there is no active tab, the base view controller is not
  // attached to a window, or another modal is already being presented.
  if (!activeWebState || !self.baseViewController.view.window ||
      self.baseViewController.presentedViewController) {
    return NO;
  }

  SuspiciousSiteWarningViewController* warningViewController =
      [[SuspiciousSiteWarningViewController alloc] init];
  warningViewController.delegate = self;
  _suspiciousSiteWarningViewController = warningViewController;
  _suspiciousSiteWebState = activeWebState->GetWeakPtr();

  __weak __typeof(self) weakSelf = self;
  __weak SuspiciousSiteWarningViewController* weakWarningViewController =
      warningViewController;
  [self.baseViewController
      presentViewController:warningViewController
                   animated:YES
                 completion:^{
                   __typeof(self) strongSelf = weakSelf;
                   if (!strongSelf) {
                     [weakWarningViewController.presentingViewController
                         dismissViewControllerAnimated:NO
                                            completion:nil];
                     return;
                   }
                   [strongSelf didFinishPresentingSuspiciousSiteWarning:
                                   weakWarningViewController];
                 }];
  return YES;
}

- (void)dismissSuspiciousSiteWarning {
  SuspiciousSiteWarningViewController* warningViewController =
      _suspiciousSiteWarningViewController;
  warningViewController.delegate = nil;
  _suspiciousSiteWarningViewController = nil;
  _suspiciousSiteWebState.reset();

  // If the alert is still mid-presentation, UIKit ignores dismissal calls;
  // `didFinishPresentingSuspiciousSiteWarning:` will dismiss it once the
  // presentation transition completes.
  if (warningViewController && !warningViewController.isBeingPresented) {
    [warningViewController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
}

#pragma mark - SuspiciousSiteWarningViewControllerDelegate

- (void)suspiciousSiteWarningViewControllerDidTapBackToSafety:
    (SuspiciousSiteWarningViewController*)viewController {
  // Ignore callbacks from stale view controllers that have already been
  // detached by dismissal.
  if (viewController != _suspiciousSiteWarningViewController) {
    return;
  }
  if (SuspiciousSiteTabHelper* tabHelper =
          [self suspiciousSiteTabHelperForAlertDismissal]) {
    tabHelper->OnBackToSafetyClicked();
  }
}

- (void)suspiciousSiteWarningViewControllerDidTapMarkAsSafe:
    (SuspiciousSiteWarningViewController*)viewController {
  // Ignore callbacks from stale view controllers that have already been
  // detached by dismissal.
  if (viewController != _suspiciousSiteWarningViewController) {
    return;
  }
  if (SuspiciousSiteTabHelper* tabHelper =
          [self suspiciousSiteTabHelperForAlertDismissal]) {
    tabHelper->OnMarkAsSafeClicked();
  }
}

- (void)suspiciousSiteWarningViewControllerDidDismissWithoutAction:
    (SuspiciousSiteWarningViewController*)viewController {
  // Ignore callbacks from stale view controllers that have already been
  // detached by dismissal.
  if (viewController != _suspiciousSiteWarningViewController) {
    return;
  }
  [self handleSuspiciousSiteWarningDismissedBySystem];
}

#pragma mark - Private

// Completes deferred dismissal if `dismissSuspiciousSiteWarning` was called
// while `warningViewController` was still being presented.
- (void)didFinishPresentingSuspiciousSiteWarning:
    (SuspiciousSiteWarningViewController*)warningViewController {
  if (warningViewController &&
      warningViewController != _suspiciousSiteWarningViewController) {
    [warningViewController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
}

// Dismisses `_suspiciousSiteWarningViewController` and returns the originating
// tab's `SuspiciousSiteTabHelper`, if the tab is still alive.
- (SuspiciousSiteTabHelper*)suspiciousSiteTabHelperForAlertDismissal {
  web::WebState* webState = _suspiciousSiteWebState.get();
  [self dismissSuspiciousSiteWarning];
  // The originating tab may have been closed before the alert callback runs.
  if (!webState) {
    return nullptr;
  }
  return SuspiciousSiteTabHelper::FromWebState(webState);
}

// Notifies the originating tab's `SuspiciousSiteTabHelper` that the warning
// alert was dismissed without user interaction.
- (void)handleSuspiciousSiteWarningDismissedBySystem {
  if (SuspiciousSiteTabHelper* tabHelper =
          [self suspiciousSiteTabHelperForAlertDismissal]) {
    tabHelper->OnWarningDismissedBySystem();
  }
}

- (void)handlePreferenceChange {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }
  ProfileIOS* profile = browser->GetProfile();
  if (!profile) {
    return;
  }

  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    // No active WebState to show the banner on. Set a flag to try again
    // when a tab is next activated.
    _pendingBannerPreferenceChange = YES;
    return;
  }

  // If we've reached this point, we are about to show a banner (or explicitly
  // not show one), so reset the pending flag.
  _pendingBannerPreferenceChange = NO;

  PrefService* prefService = profile->GetPrefs();

  // If the pref change was triggered by the Tailored Security Service, then
  // that service will show its own UI.
  safe_browsing::TailoredSecurityService* tailored_security_service =
      TailoredSecurityServiceFactory::GetForProfile(profile);
  if (safe_browsing::TailoredSecurityService::IsResponsibleForNotification(
          prefService, tailored_security_service)) {
    return;
  }

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string email;
  if (identityManager &&
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    email =
        identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email;
  }

  BOOL isSettingsVisible = [self.baseViewController.presentedViewController
      isKindOfClass:[SettingsNavigationController class]];

  bool isEnhancedProtectionEnabled =
      safe_browsing::IsEnhancedProtectionEnabled(*prefService);
  if (isEnhancedProtectionEnabled) {
    [self showSafeBrowsingSyncInfobarForState:YES withEmail:email];
  } else if (!isSettingsVisible) {
    // Only show the "off" banner if the user is not on the settings page.
    [self showSafeBrowsingSyncInfobarForState:NO withEmail:""];
  }
}

- (void)showSafeBrowsingSyncInfobarForState:(BOOL)isEnhancedProtectionEnabled
                                  withEmail:(const std::string&)email {
  EnhancedSafeBrowsingInfobarScenario scenario;
  if (isEnhancedProtectionEnabled) {
    scenario = EnhancedSafeBrowsingInfobarScenario::kClientSyncEnabled;
  } else {
    scenario =
        EnhancedSafeBrowsingInfobarScenario::kClientSyncDisabledWithButton;
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);

  auto delegate = std::make_unique<EnhancedSafeBrowsingInfobarDelegate>(
      activeWebState, settingsHandler, scenario, email);

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(activeWebState);
  auto infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeEnhancedSafeBrowsing, std::move(delegate));
  infobar_manager->AddInfoBar(std::move(infobar), /*replace_existing=*/true);
}

#pragma mark - TabsDependencyInstalling

- (void)webStateInserted:(web::WebState*)webState {
  SafeBrowsingTabHelper::FromWebState(webState)->SetDelegate(self);
  // `SuspiciousSiteTabHelper` is not attached to off-the-record WebStates or
  // when `kSuspiciousSiteWarnings` is disabled.
  if (SuspiciousSiteTabHelper* suspiciousSiteTabHelper =
          SuspiciousSiteTabHelper::FromWebState(webState)) {
    suspiciousSiteTabHelper->SetDelegate(self);
  }
}

- (void)webStateRemoved:(web::WebState*)webState {
  SafeBrowsingTabHelper::FromWebState(webState)->RemoveDelegate();
  // `SuspiciousSiteTabHelper` is not attached to off-the-record WebStates or
  // when `kSuspiciousSiteWarnings` is disabled.
  if (SuspiciousSiteTabHelper* suspiciousSiteTabHelper =
          SuspiciousSiteTabHelper::FromWebState(webState)) {
    suspiciousSiteTabHelper->SetDelegate(nil);
  }
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  if (_pendingBannerPreferenceChange) {
    [self handlePreferenceChange];
  }
}

@end
