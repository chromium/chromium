// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_coordinator.h"

#import "build/branding_buildflags.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Size of the Chrome product logo.
constexpr CGFloat kLogoSize = 30;

// Top spacing before the Chrome product logo in the bottom sheet.
constexpr CGFloat kSpacingBeforeImage = 24;

}  // namespace

@interface AutofillAIPrivateInferenceNoticeCoordinator () <
    ConfirmationAlertActionHandler,
    UISheetPresentationControllerDelegate>
@end

@implementation AutofillAIPrivateInferenceNoticeCoordinator {
  // The confirmation alert view controller showing the notice.
  ConfirmationAlertViewController* _viewController;
  // The mediator handling the business logic of the notice.
  AutofillAIPrivateInferenceNoticeMediator* _mediator;
}

- (void)start {
  PrefService* prefService = self.profile->GetPrefs();
  id<AutofillCommands> autofillHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutofillCommands);
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);

  _mediator = [[AutofillAIPrivateInferenceNoticeMediator alloc]
      initWithPrefService:prefService
          autofillHandler:autofillHandler
          settingsHandler:settingsHandler];

  _viewController = [[ConfirmationAlertViewController alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  _viewController.image = MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolMulticolorChromeball, kLogoSize));
#else
  _viewController.image = SymbolWithPointSize(SymbolChromeProduct, kLogoSize);
#endif
  _viewController.imageHasFixedSize = YES;
  _viewController.customSpacingBeforeImage = kSpacingBeforeImage;
  _viewController.titleString = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_TITLE);
  _viewController.subtitleString = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_SUBTITLE);
  _viewController.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_GOT_IT);
  _viewController.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_SETTINGS);

  _viewController.actionHandler = self;

  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  if (presentationController) {
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent],
    ];
    presentationController.delegate = self;
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  [_mediator markNoticeShown];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mediator didAcknowledgeNotice];
}

- (void)confirmationAlertSecondaryAction {
  [_mediator didTapSettings];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_mediator didDismissNotice];
}

@end
