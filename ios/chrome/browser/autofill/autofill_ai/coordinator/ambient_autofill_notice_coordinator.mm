// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_coordinator.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/autofill_ai/coordinator/ambient_autofill_notice_mediator.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Size of the Chrome product logo.
constexpr CGFloat kLogoSize = 30;

// Top spacing before the Chrome product logo in the bottom sheet.
constexpr CGFloat kSpacingBeforeImage = 24;

}  // namespace

@interface AmbientAutofillNoticeCoordinator () <
    ConfirmationAlertActionHandler,
    UISheetPresentationControllerDelegate>
@end

@implementation AmbientAutofillNoticeCoordinator {
  // Parameters of the form activity that triggered the bottom sheet.
  autofill::FormActivityParams _params;
  // The confirmation alert view controller showing the Ambient Autofill notice.
  ConfirmationAlertViewController* _viewController;
  // The mediator handling the business logic of the notice.
  AmbientAutofillNoticeMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(const autofill::FormActivityParams&)
                                               params {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _params = params;
  }
  return self;
}

- (void)start {
  if (!self.browser) {
    return;
  }
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  base::WeakPtr<web::WebState> webState =
      activeWebState ? activeWebState->GetWeakPtr() : nullptr;
  id<AutofillCommands> autofillHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutofillCommands);

  _mediator =
      [[AmbientAutofillNoticeMediator alloc] initWithWebState:webState
                                                       params:_params
                                              autofillHandler:autofillHandler];

  _viewController = [[ConfirmationAlertViewController alloc] init];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  _viewController.image = MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolMulticolorChromeball, kLogoSize));
#else
  _viewController.image = SymbolWithPointSize(SymbolChromeProduct, kLogoSize);
#endif
  _viewController.imageHasFixedSize = YES;
  _viewController.customSpacingBeforeImage = kSpacingBeforeImage;
  _viewController.titleString =
      l10n_util::GetNSString(IDS_IOS_AMBIENT_AUTOFILL_NOTICE_TITLE);
  _viewController.subtitleString =
      l10n_util::GetNSString(IDS_IOS_AMBIENT_AUTOFILL_NOTICE_SUBTITLE);
  _viewController.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AMBIENT_AUTOFILL_NOTICE_OK);
  _viewController.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_AMBIENT_AUTOFILL_NOTICE_SETTINGS);

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
}

- (void)stop {
  if (_viewController) {
    [_viewController dismissViewControllerAnimated:YES completion:nil];
    _viewController = nil;
  }
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

#pragma mark - Public

- (void)markNoticeShown {
  [_mediator markNoticeShown];
}

@end
