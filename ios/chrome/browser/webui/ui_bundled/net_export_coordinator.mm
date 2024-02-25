// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/net_export_coordinator.h"

#import <MessageUI/MessageUI.h>

#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/webui/model/show_mail_composer_context.h"
#import "ui/base/l10n/l10n_util.h"

@interface NetExportCoordinator () <MFMailComposeViewControllerDelegate> {
  // Coordinator for displaying alerts.
  AlertCoordinator* _alertCoordinator;
}

// Contains information for populating the email.
@property(nonatomic, strong) ShowMailComposerContext* context;

@end

@implementation NetExportCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       mailComposerContext:(ShowMailComposerContext*)context {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    self.context = context;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (![MFMailComposeViewController canSendMail]) {
    NSString* alertTitle =
        l10n_util::GetNSString([self.context emailNotConfiguredAlertTitleId]);
    NSString* alertMessage =
        l10n_util::GetNSString([self.context emailNotConfiguredAlertMessageId]);

    _alertCoordinator = [[AlertCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser
                             title:alertTitle
                           message:alertMessage];
    __weak NetExportCoordinator* weakSelf = self;
    [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                 action:^{
                                   [weakSelf stopAlertCoordinator];
                                 }
                                  style:UIAlertActionStyleDefault];

    [_alertCoordinator start];
    return;
  }
  MFMailComposeViewController* mailViewController =
      [[MFMailComposeViewController alloc] init];
  [mailViewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [mailViewController setToRecipients:[self.context toRecipients]];
  [mailViewController setSubject:[self.context subject]];
  [mailViewController setMessageBody:[self.context body] isHTML:NO];

  const base::FilePath& textFile = [self.context textFileToAttach];
  if (!textFile.empty()) {
    NSString* filename = base::SysUTF8ToNSString(textFile.value());
    NSData* data = [NSData dataWithContentsOfFile:filename];
    if (data) {
      NSString* displayName =
          base::SysUTF8ToNSString(textFile.BaseName().value());
      [mailViewController addAttachmentData:data
                                   mimeType:@"text/plain"
                                   fileName:displayName];
    }
  }

  [mailViewController setMailComposeDelegate:self];
  [self.baseViewController presentViewController:mailViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopAlertCoordinator];
}

#pragma mark - MFMailComposeViewControllerDelegate methods

- (void)mailComposeController:(MFMailComposeViewController*)controller
          didFinishWithResult:(MFMailComposeResult)result
                        error:(NSError*)error {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - private

- (void)stopAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end
