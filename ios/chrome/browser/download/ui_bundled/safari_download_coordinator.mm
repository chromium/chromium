// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/safari_download_coordinator.h"

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
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"

const char kUmaDownloadCalendarFileUI[] = "Download.IOSDownloadCalendarFileUI";
const char kUmaDownloadMobileConfigFileUI[] =
    "Download.IOSDownloadMobileConfigFileUI";

@interface SafariDownloadCoordinator () <DependencyInstalling,
                                         SafariDownloadTabHelperDelegate,
                                         SFSafariViewControllerDelegate> {
  // Bridge which observes WebStateList and alerts this coordinator when this
  // needs to register the Mediator with a new WebState.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

// Coordinator used to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// SFSafariViewController used to download files.
@property(nonatomic, strong) SFSafariViewController* safariViewController;

@end

@implementation SafariDownloadCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(
            self, browser->GetWebStateList());
  }
  return self;
}

- (void)stop {
  // Reset this observer manually. We want this to go out of scope now, to
  // ensure it detaches before `browser` and its WebStateList get destroyed.
  _dependencyInstallerBridge.reset();

  self.safariViewController = nil;
  [self dismissAlertCoordinator];
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

// Dismisses the alert coordinator.
- (void)dismissAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}
#pragma mark - DependencyInstalling methods

- (void)installDependencyForWebState:(web::WebState*)webState {
  SafariDownloadTabHelper::FromWebState(webState)->set_delegate(self);
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  SafariDownloadTabHelper::FromWebState(webState)->set_delegate(nil);
}

#pragma mark - SafariDownloadTabHelperDelegate

- (void)presentMobileConfigAlertFromURL:(NSURL*)fileURL {
  if (!fileURL || !fileURL.host.length) {
    return;
  }

  base::UmaHistogramEnumeration(kUmaDownloadMobileConfigFileUI,
                                SafariDownloadFileUI::kWarningAlertIsPresented);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_TITLE)
                         message:
                             l10n_util::GetNSStringF(
                                 IDS_IOS_DOWNLOAD_MOBILECONFIG_FILE_WARNING_MESSAGE,
                                 base::SysNSStringToUTF16(fileURL.host))];

  __weak SafariDownloadCoordinator* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::UmaHistogramEnumeration(
                      kUmaDownloadMobileConfigFileUI,
                      SafariDownloadFileUI::kWarningAlertIsDismissed);
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                action:^{
                  base::UmaHistogramEnumeration(
                      kUmaDownloadMobileConfigFileUI,
                      SafariDownloadFileUI::kSFSafariViewIsPresented);
                  [weakSelf presentSFSafariViewController:fileURL];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

- (void)presentCalendarAlertFromURL:(NSURL*)fileURL {
  if (!fileURL) {
    return;
  }

  base::UmaHistogramEnumeration(kUmaDownloadCalendarFileUI,
                                SafariDownloadFileUI::kWarningAlertIsPresented);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:
                               l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_TITLE)
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_DOWNLOAD_CALENDAR_FILE_WARNING_MESSAGE)];

  __weak SafariDownloadCoordinator* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::UmaHistogramEnumeration(
                      kUmaDownloadCalendarFileUI,
                      SafariDownloadFileUI::kWarningAlertIsDismissed);
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DOWNLOAD_MOBILECONFIG_CONTINUE)
                action:^{
                  base::UmaHistogramEnumeration(
                      kUmaDownloadCalendarFileUI,
                      SafariDownloadFileUI::kSFSafariViewIsPresented);
                  [weakSelf presentSFSafariViewController:fileURL];
                  [weakSelf dismissAlertCoordinator];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - SFSafariViewControllerDelegate

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
  [self.baseViewController dismissViewControllerAnimated:true completion:nil];
}

@end
