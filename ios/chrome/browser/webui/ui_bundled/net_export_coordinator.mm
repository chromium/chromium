// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/net_export_coordinator.h"

#import <MessageUI/MessageUI.h>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
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

  const base::FilePath& textFile = [self.context textFileToAttach];
  if (textFile.empty()) {
    [self presentMailComposerWithAttachmentData:nil displayName:nil];
    return;
  }

  // Read the file on a background thread to avoid blocking the main thread,
  // as net export log files can be large.
  NSString* displayName = base::SysUTF8ToNSString(textFile.BaseName().value());
  __weak NetExportCoordinator* weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::ReadFileToBytes, textFile),
      base::BindOnce(^(std::optional<std::vector<uint8_t>> fileBytes) {
        NSData* data = nil;
        if (fileBytes.has_value()) {
          data = [NSData dataWithBytes:fileBytes->data()
                                length:fileBytes->size()];
        }
        [weakSelf presentMailComposerWithAttachmentData:data
                                            displayName:displayName];
      }));
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

// Presents the mail compose view controller. If `data` is non-nil, attaches
// it to the email with the given `displayName`.
- (void)presentMailComposerWithAttachmentData:(NSData*)data
                                  displayName:(NSString*)displayName {
  MFMailComposeViewController* mailViewController =
      [[MFMailComposeViewController alloc] init];
  [mailViewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [mailViewController setToRecipients:[self.context toRecipients]];
  [mailViewController setSubject:[self.context subject]];
  [mailViewController setMessageBody:[self.context body] isHTML:NO];

  if (data) {
    [mailViewController addAttachmentData:data
                                 mimeType:@"text/plain"
                                 fileName:displayName];
  }

  [mailViewController setMailComposeDelegate:self];
  [self.baseViewController presentViewController:mailViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stopAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end
