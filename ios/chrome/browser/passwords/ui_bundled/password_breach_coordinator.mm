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
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/password_breach_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_mediator.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_presenter.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_breach_view_controller.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using password_manager::CredentialLeakType;

@interface PasswordBreachCoordinator () <PasswordBreachPresenter>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordBreachMediator* mediator;

// UKM SourceId of the active WebState.
@property(nonatomic, assign) ukm::SourceId ukm_source_id;

// Leak type of the dialog.
@property(nonatomic, assign) CredentialLeakType leakType;

// Url needed for the dialog.
@property(nonatomic, assign) GURL url;

@end

@implementation PasswordBreachCoordinator

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
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  self.viewController.modalInPresentation = YES;

  auto recorder = std::make_unique<
      password_manager::metrics_util::LeakDialogMetricsRecorder>(
      self.ukm_source_id, password_manager::GetLeakDialogType(self.leakType));
  self.mediator =
      [[PasswordBreachMediator alloc] initWithConsumer:self.viewController
                                             presenter:self
                                      metrics_recorder:std::move(recorder)
                                              leakType:self.leakType];
  self.viewController.actionHandler = self.mediator;

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - PasswordBreachPresenter

- (void)presentLearnMore {
  NSString* message =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.viewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

- (void)openSavedPasswordsSettings {
  id<SettingsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  password_manager::LogPasswordCheckReferrer(
      password_manager::PasswordCheckReferrer::kPasswordBreachDialog);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordBreachDialog);
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordBreachOpenPasswordManager"));

  [handler showSavedPasswordsSettingsFromViewController:self.baseViewController
                                       showCancelButton:NO];
}

@end
