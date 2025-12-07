// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_coordinator.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/strings/grit/components_strings.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_mediator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_presenter.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using password_manager::CredentialLeakType;

@interface PasswordBreachCoordinator () <PasswordBreachPresenter>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordBreachMediator* mediator;

// UKM SourceId of the active WebState.
@property(nonatomic, assign) ukm::SourceId ukm_source_id;

// Leak type of the dialog.
@property(nonatomic, assign) CredentialLeakType leakType;

// Url needed for the dialog.
@property(nonatomic, assign) GURL url;

@end

@implementation PasswordBreachCoordinator {
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  leakType:(CredentialLeakType)leakType {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _leakType = leakType;
    web::WebState* web_state = browser->GetWebStateList()->GetActiveWebState();
    _ukm_source_id = web_state ? ukm::GetSourceIdForWebStateDocument(web_state)
                               : ukm::kInvalidSourceId;
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordBreachViewController alloc] init];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];

  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.modalInPresentation = YES;

  auto recorder = std::make_unique<
      password_manager::metrics_util::LeakDialogMetricsRecorder>(
      self.ukm_source_id, password_manager::GetLeakDialogType(self.leakType));

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  CHECK(authenticationService);

  self.mediator =
      [[PasswordBreachMediator alloc] initWithConsumer:self.viewController
                                             presenter:self
                                      metrics_recorder:std::move(recorder)
                                              leakType:self.leakType
                                 authenticationService:authenticationService];
  self.viewController.actionHandler = self.mediator;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - PasswordBreachPresenter

- (void)openPasswordCheckup {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordBreachOpenPasswordCheckup"));

  [handler dismissModalsAndShowPasswordCheckupPageForReferrer:
               password_manager::PasswordCheckReferrer::kPasswordBreachDialog];
}

- (void)openPasswordManager {
  id<SettingsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordBreachDialog);
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordBreachOpenPasswordManager"));

  [handler
      showSavedPasswordsSettingsFromViewController:self.baseViewController];
}

@end
