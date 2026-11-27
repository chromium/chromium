// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/safari_download_coordinator.h"

#import <SafariServices/SafariServices.h>

#import "base/check_op.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/coordinator/safari_download_mediator.h"
#import "ios/chrome/browser/download/coordinator/safari_download_mediator_delegate.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

const char kUmaDownloadCalendarFileUI[] = "Download.IOSDownloadCalendarFileUI";
const char kUmaDownloadMobileConfigFileUI[] =
    "Download.IOSDownloadMobileConfigFileUI";
const char kUmaDownloadAppleWalletOrderFileUI[] =
    "Download.IOSDownloadAppleWalletOrderFileUI";

@interface SafariDownloadCoordinator () <SafariDownloadMediatorDelegate,
                                         SafariDownloadTabHelperDelegate,
                                         SFSafariViewControllerDelegate>

// Completes deferred dismissal if `alertController` was invalidated
// mid-presentation.
- (void)didFinishPresentingAlert:(UIAlertController*)alertController;

@end

namespace {

// Different types of downloads using SFSafariViewController.
enum class SafariDownloadType {
  kCalendar,
  kMobileConfig,
  kAppleWalletOrder,
};

// Returns the appropriate histogram for a given `download_type`.
const char* GetHistogramForDownloadType(SafariDownloadType download_type) {
  switch (download_type) {
    case SafariDownloadType::kCalendar:
      return kUmaDownloadCalendarFileUI;
    case SafariDownloadType::kMobileConfig:
      return kUmaDownloadMobileConfigFileUI;
    case SafariDownloadType::kAppleWalletOrder:
      return kUmaDownloadAppleWalletOrderFileUI;
  }
}

// Completes deferred dismissal if `coordinator` was deallocated or invalidated
// `alert_controller` mid-presentation.
void DidFinishPresentingAlert(UIAlertController* alert_controller,
                              SafariDownloadCoordinator* coordinator) {
  if (!coordinator) {
    [alert_controller.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
    return;
  }
  [coordinator didFinishPresentingAlert:alert_controller];
}

}  // namespace

@implementation SafariDownloadCoordinator {
  // Forwards `WebStateList` changes to `_mediator`.
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
  // Installs tab helper delegates and observes the active `WebState` to dismiss
  // stale download alerts.
  SafariDownloadMediator* _mediator;
  // `SFSafariViewController` used to download files.
  SFSafariViewController* _safariViewController;
  // Currently presented download confirmation alert.
  UIAlertController* _alertController;
  // `WebState` that triggered `_alertController`.
  base::WeakPtr<web::WebState> _alertWebState;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[SafariDownloadMediator alloc]
      initWithActiveWebState:self.browser->GetWebStateList()
                                 ->GetActiveWebState()];
  _mediator.delegate = self;
  _mediator.tabHelperDelegate = self;
  _dependencyInstallerBridge.StartObserving(_mediator, self.browser);
}

- (void)stop {
  // Stop observing the `WebStateList` before disconnecting the mediator and
  // destroying the bridge object.
  _dependencyInstallerBridge.StopObserving();
  [_mediator disconnect];
  _mediator = nil;

  _safariViewController.delegate = nil;
  [_safariViewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
  _safariViewController = nil;
  [self dismissAlertAnimated:NO];
}

#pragma mark - Private

// Builds a confirmation `UIAlertController` with accept and cancel actions for
// `downloadType`.
- (UIAlertController*)alertControllerForDownloadType:
                          (SafariDownloadType)downloadType
                                             fileURL:(NSURL*)fileURL
                                               title:(NSString*)title
                                             message:(NSString*)message
                                         acceptTitle:(NSString*)acceptTitle {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak SafariDownloadCoordinator* weakSelf = self;
  __weak UIAlertController* weakAlertController = alertController;

  UIAlertAction* accept = [UIAlertAction
      actionWithTitle:acceptTitle
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [weakSelf confirmDownloadType:downloadType
                                   forFileURL:fileURL
                              alertController:weakAlertController];
              }];
  [alertController addAction:accept];

  UIAlertAction* cancel = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction*) {
                [weakSelf cancelDownloadType:downloadType
                             alertController:weakAlertController];
              }];
  [alertController addAction:cancel];

  return alertController;
}

// Presents a confirmation alert if no modal is currently presented.
- (void)presentAlertForDownloadType:(SafariDownloadType)downloadType
                            fileURL:(NSURL*)fileURL
                              title:(NSString*)title
                            message:(NSString*)message
                        acceptTitle:(NSString*)acceptTitle {
  if (self.baseViewController.presentedViewController) {
    return;
  }

  base::UmaHistogramEnumeration(GetHistogramForDownloadType(downloadType),
                                SafariDownloadFileUI::kWarningAlertIsPresented);

  UIAlertController* alertController =
      [self alertControllerForDownloadType:downloadType
                                   fileURL:fileURL
                                     title:title
                                   message:message
                               acceptTitle:acceptTitle];
  _alertController = alertController;
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  _alertWebState = activeWebState ? activeWebState->GetWeakPtr()
                                  : base::WeakPtr<web::WebState>();

  __weak SafariDownloadCoordinator* weakSelf = self;
  __weak UIAlertController* weakAlertController = alertController;
  [self.baseViewController
      presentViewController:alertController
                   animated:YES
                 completion:^{
                   DidFinishPresentingAlert(weakAlertController, weakSelf);
                 }];
}

// Completes deferred dismissal if the alert was invalidated mid-presentation.
- (void)didFinishPresentingAlert:(UIAlertController*)alertController {
  if (alertController != _alertController) {
    [alertController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
}

// Presents SFSafariViewController to download `fileURL`.
- (void)presentSFSafariViewController:(NSURL*)fileURL {
  _safariViewController.delegate = nil;
  _safariViewController = [[SFSafariViewController alloc] initWithURL:fileURL];
  _safariViewController.delegate = self;
  _safariViewController.preferredBarTintColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];

  [self.baseViewController presentViewController:_safariViewController
                                        animated:YES
                                      completion:nil];
}

// Clears tracking references for the current download confirmation alert.
- (void)clearAlertState {
  _alertController = nil;
  _alertWebState.reset();
}

// Dismisses `_alertController`, deferring if it is still being presented.
- (void)dismissAlertAnimated:(BOOL)animated {
  UIAlertController* alertController = _alertController;
  [self clearAlertState];

  if (!alertController.beingPresented) {
    [alertController.presentingViewController
        dismissViewControllerAnimated:animated
                           completion:nil];
  }
}

// Records confirmation for `downloadType`, clears alert state, and presents
// SFSafariViewController for `fileURL`. No-ops if `alertController` is nil or
// no longer matches the currently tracked alert.
- (void)confirmDownloadType:(SafariDownloadType)downloadType
                 forFileURL:(NSURL*)fileURL
            alertController:(UIAlertController*)alertController {
  if (!alertController || alertController != _alertController) {
    return;
  }
  base::UmaHistogramEnumeration(GetHistogramForDownloadType(downloadType),
                                SafariDownloadFileUI::kSFSafariViewIsPresented);
  [self clearAlertState];
  [self presentSFSafariViewController:fileURL];
}

// Records cancellation for `downloadType` and clears alert state. No-ops if
// `alertController` is nil or no longer matches the currently tracked alert.
- (void)cancelDownloadType:(SafariDownloadType)downloadType
           alertController:(UIAlertController*)alertController {
  if (!alertController || alertController != _alertController) {
    return;
  }
  base::UmaHistogramEnumeration(GetHistogramForDownloadType(downloadType),
                                SafariDownloadFileUI::kWarningAlertIsDismissed);
  [self clearAlertState];
}

#pragma mark - SafariDownloadMediatorDelegate

- (void)safariDownloadMediator:(SafariDownloadMediator*)mediator
    shouldDismissAlertForWebState:(web::WebState*)webState {
  if (_alertController &&
      (!_alertWebState || _alertWebState.get() == webState)) {
    [self dismissAlertAnimated:YES];
  }
}

#pragma mark - SafariDownloadTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  if (!fileURL || !fileURL.host.length) {
    return;
  }

  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE);
  NSString* const message = l10n_util::GetNSStringF(
      IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_MESSAGE,
      base::SysNSStringToUTF16(fileURL.host));
  NSString* const acceptTitle =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE);
  [self presentAlertForDownloadType:SafariDownloadType::kMobileConfig
                            fileURL:fileURL
                              title:title
                            message:message
                        acceptTitle:acceptTitle];
}

- (void)presentCalendarAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_TITLE);
  NSString* const message =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_MESSAGE);
  NSString* const acceptTitle =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE);
  [self presentAlertForDownloadType:SafariDownloadType::kCalendar
                            fileURL:fileURL
                              title:title
                            message:message
                        acceptTitle:acceptTitle];
}

- (void)presentAppleWalletOrderAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_WALLET_ORDER_FILE_WARNING_TITLE);
  NSString* const message = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_WALLET_ORDER_FILE_WARNING_MESSAGE);
  NSString* const acceptTitle =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_WALLET_ORDER_OPEN);
  [self presentAlertForDownloadType:SafariDownloadType::kAppleWalletOrder
                            fileURL:fileURL
                              title:title
                            message:message
                        acceptTitle:acceptTitle];
}

#pragma mark - SFSafariViewControllerDelegate

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
  CHECK_EQ(_safariViewController, controller, base::NotFatalUntil::M164);
  if (_safariViewController != controller) {
    return;
  }
  _safariViewController.delegate = nil;
  [controller.presentingViewController dismissViewControllerAnimated:YES
                                                          completion:nil];
  _safariViewController = nil;
}

@end
