// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_coordinator.h"

#import <optional>
#import <utility>

#import "components/autofill/core/browser/metrics/payments/wallet_reminder_notice_metrics.h"
#import "components/autofill/core/browser/payments/legal_message_line.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/coordinator/wallet_reminder_notice_mediator.h"
#import "ios/chrome/browser/autofill/wallet_reminder_notice/ui/wallet_reminder_notice_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@interface WalletReminderNoticeCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate,
    WalletReminderNoticeViewControllerDelegate>
@end

@implementation WalletReminderNoticeCoordinator {
  // Legal message lines passed to the mediator.
  std::optional<autofill::LegalMessageLines> _legalMessageLines;
  // The mediator owned by this coordinator.
  WalletReminderNoticeMediator* _mediator;
  // The view controller owned by this coordinator.
  WalletReminderNoticeViewController* _viewController;
  // The window whose VoiceOver modality was overridden while the reminder
  // notice is shown, restored on dismissal.
  __weak UIWindow* _voiceOverScopedWindow;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                         legalMessageLines:
                             (autofill::LegalMessageLines)legalMessageLines {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _legalMessageLines = std::move(legalMessageLines);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[WalletReminderNoticeMediator alloc]
      initWithLegalMessageLines:std::move(*_legalMessageLines)];
  _legalMessageLines.reset();

  _viewController = [[WalletReminderNoticeViewController alloc] init];
  _viewController.actionHandler = self;
  _viewController.delegate = self;
  _viewController.presentationController.delegate = self;

  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  if (presentationController) {
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.preferredCornerRadius = 38.0;
    presentationController.detents = @[
      [_viewController preferredHeightDetent],
      [UISheetPresentationControllerDetent largeDetent],
    ];
  }

  _mediator.consumer = _viewController;

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController
      presentViewController:_viewController
                   animated:YES
                 completion:^{
                   [weakSelf scopeVoiceOverToReminderNoticeSheet];
                 }];
}

- (void)stop {
  [self unscopeVoiceOverFromReminderNoticeSheet];
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  _mediator.consumer = nil;
  _mediator = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  autofill::autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill::autofill_metrics::WalletReminderNoticeInteraction::
          kAcknowledgedCta);
  [self dismissNotice];
}

#pragma mark - WalletReminderNoticeViewControllerDelegate

- (void)walletReminderNoticeViewControllerDidTapPrimaryAction:
    (WalletReminderNoticeViewController*)viewController {
  autofill::autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill::autofill_metrics::WalletReminderNoticeInteraction::
          kAcknowledgedCta);
  [self dismissNotice];
}

- (void)walletReminderNoticeViewController:
            (WalletReminderNoticeViewController*)viewController
                             didTapLinkURL:(NSURL*)URL {
  // Ensure the link URL is a valid HTTP or HTTPS URL before opening in a new
  // tab.
  const GURL gurl = net::GURLWithNSURL(URL);
  if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  autofill::autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill::autofill_metrics::WalletReminderNoticeInteraction::
          kClickedLink);
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:gurl]];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  autofill::autofill_metrics::LogWalletReminderNoticeInteraction(
      autofill::autofill_metrics::WalletReminderNoticeInteraction::kDismissed);
  [self dismissNotice];
}

#pragma mark - Private

// Restricts VoiceOver to the reminder notice sheet and moves focus onto its
// title.
// TODO(crbug.com/576780746): Clean up the accessibility view is modal logic
// when the bug is fixed.
- (void)scopeVoiceOverToReminderNoticeSheet {
  UIView* sheetView = _viewController.view;
  if (!sheetView) {
    return;
  }
  sheetView.accessibilityViewIsModal = YES;
  _viewController.presentationController.containerView
      .accessibilityViewIsModal = YES;

  UIWindow* window = sheetView.window;
  if (window && !window.accessibilityViewIsModal) {
    window.accessibilityViewIsModal = YES;
    _voiceOverScopedWindow = window;
  }

  // A screen change notification is required to pull focus away from the
  // content presented behind the sheet.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _viewController.titleLabel);
}

// Reverts the window modality applied by `scopeVoiceOverToReminderNoticeSheet`.
- (void)unscopeVoiceOverFromReminderNoticeSheet {
  _voiceOverScopedWindow.accessibilityViewIsModal = NO;
  _voiceOverScopedWindow = nil;
}

- (void)dismissNotice {
  [self stop];
}

@end
