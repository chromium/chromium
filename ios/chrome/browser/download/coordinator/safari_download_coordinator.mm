// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/safari_download_coordinator.h"

#import <SafariServices/SafariServices.h>

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

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

}  // namespace

const char kUmaDownloadCalendarFileUI[] = "Download.IOSDownloadCalendarFileUI";
const char kUmaDownloadMobileConfigFileUI[] =
    "Download.IOSDownloadMobileConfigFileUI";
const char kUmaDownloadAppleWalletOrderFileUI[] =
    "Download.IOSDownloadAppleWalletOrderFileUI";

@interface SafariDownloadCoordinator () <TabsDependencyInstalling,
                                         SafariDownloadTabHelperDelegate,
                                         SFSafariViewControllerDelegate>

// SFSafariViewController used to download files.
@property(nonatomic, strong) SFSafariViewController* safariViewController;

@end

@implementation SafariDownloadCoordinator {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
  // AlertController used to display modal alerts to the user.
  UIAlertController* _alertController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _dependencyInstallerBridge.StartObserving(
        self, browser, TabsDependencyInstaller::Policy::kOnlyRealized);
  }
  return self;
}

- (void)stop {
  // Stop observing the WebStateList before destroying the bridge object.
  _dependencyInstallerBridge.StopObserving();

  self.safariViewController = nil;
  [self dismissAlert];
}

#pragma mark - Private

// Presents SFSafariViewController in order to download the file.
- (void)presentSFSafariViewController:(NSURL*)fileURL {
  self.safariViewController =
      [[SFSafariViewController alloc] initWithURL:fileURL];
  self.safariViewController.delegate = self;
  self.safariViewController.preferredBarTintColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];

  [self.baseViewController presentViewController:self.safariViewController
                                        animated:YES
                                      completion:nil];
}

// Dismisses the alert.
- (void)dismissAlert {
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
  _alertController = nil;
}

// Presents SFSafariViewController in order to download the file and then
// dismisses the alert coordinator. Records the appropriate histogram for
// `downloadType`. Should only be called if the confirmation was initiated by
// the user from the alert.
- (void)confirmDownloadType:(SafariDownloadType)downloadType
                 forFileURL:(NSURL*)fileURL {
  base::UmaHistogramEnumeration(GetHistogramForDownloadType(downloadType),
                                SafariDownloadFileUI::kSFSafariViewIsPresented);
  [self presentSFSafariViewController:fileURL];
  [self dismissAlert];
}

// Dismisses the alert coordinator and records the appropriate histogram for
// `downloadType`. Should only be called if the confirmation was initiated by
// the user from the alert.
- (void)cancelDownloadType:(SafariDownloadType)downloadType {
  base::UmaHistogramEnumeration(GetHistogramForDownloadType(downloadType),
                                SafariDownloadFileUI::kWarningAlertIsDismissed);
  [self dismissAlert];
}

#pragma mark - TabsDependencyInstalling methods

- (void)webStateInserted:(web::WebState*)webState {
  SafariDownloadTabHelper::FromWebState(webState)->set_delegate(self);
}

- (void)webStateRemoved:(web::WebState*)webState {
  SafariDownloadTabHelper::FromWebState(webState)->set_delegate(nil);
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  // Nothing to do.
}

#pragma mark - SafariDownloadTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  if (!fileURL || !fileURL.host.length) {
    return;
  }

  base::UmaHistogramEnumeration(kUmaDownloadMobileConfigFileUI,
                                SafariDownloadFileUI::kWarningAlertIsPresented);

  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE);
  NSString* const message = l10n_util::GetNSStringF(
      IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_MESSAGE,
      base::SysNSStringToUTF16(fileURL.host));
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak SafariDownloadCoordinator* weakSelf = self;

  UIAlertAction* accept = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [weakSelf confirmDownloadType:SafariDownloadType::kMobileConfig
                                   forFileURL:fileURL];
              }];
  [_alertController addAction:accept];

  UIAlertAction* cancel = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction*) {
                [weakSelf cancelDownloadType:SafariDownloadType::kMobileConfig];
              }];
  [_alertController addAction:cancel];

  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

- (void)presentCalendarAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  base::UmaHistogramEnumeration(kUmaDownloadCalendarFileUI,
                                SafariDownloadFileUI::kWarningAlertIsPresented);

  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_TITLE);
  NSString* const message =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_MESSAGE);
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak SafariDownloadCoordinator* weakSelf = self;

  UIAlertAction* accept = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [weakSelf confirmDownloadType:SafariDownloadType::kCalendar
                                   forFileURL:fileURL];
              }];
  [_alertController addAction:accept];

  UIAlertAction* cancel = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction*) {
                [weakSelf cancelDownloadType:SafariDownloadType::kCalendar];
              }];
  [_alertController addAction:cancel];

  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

- (void)presentAppleWalletOrderAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  base::UmaHistogramEnumeration(kUmaDownloadAppleWalletOrderFileUI,
                                SafariDownloadFileUI::kWarningAlertIsPresented);
  NSString* const title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_WALLET_ORDER_FILE_WARNING_TITLE);
  NSString* const message = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_WALLET_ORDER_FILE_WARNING_MESSAGE);
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak SafariDownloadCoordinator* weakSelf = self;

  UIAlertAction* accept = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DOWNLOAD_WALLET_ORDER_OPEN)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [weakSelf
                    confirmDownloadType:SafariDownloadType::kAppleWalletOrder
                             forFileURL:fileURL];
              }];
  [_alertController addAction:accept];

  UIAlertAction* cancel = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction*) {
                [weakSelf
                    cancelDownloadType:SafariDownloadType::kAppleWalletOrder];
              }];
  [_alertController addAction:cancel];

  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - SFSafariViewControllerDelegate

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
  [self.baseViewController dismissViewControllerAnimated:true completion:nil];
}

@end
