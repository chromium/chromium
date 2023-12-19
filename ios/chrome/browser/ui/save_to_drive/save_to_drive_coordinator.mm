// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns formatted size string.
NSString* GetSizeString(int64_t size_in_bytes) {
  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.countStyle = NSByteCountFormatterCountStyleFile;
  formatter.zeroPadsFractionDigits = YES;
  NSString* result = [formatter stringFromByteCount:size_in_bytes];
  // Replace spaces with non-breaking spaces.
  result = [result stringByReplacingOccurrencesOfString:@" "
                                             withString:@"\u00A0"];
  return result;
}

// Returns the appropriate account picker body text given `file_name` and
// `file_size`. If `file_size` is negative, then it will not appear in the body
// text.
NSString* GetAccountPickerBodyText(NSString* file_name, int64_t file_size) {
  const auto file_name_u16string = base::SysNSStringToUTF16(file_name);
  if (file_size > -1) {
    const auto file_size_u16string =
        base::SysNSStringToUTF16(GetSizeString(file_size));
    return l10n_util::GetNSStringF(
        IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_BODY_WITH_SIZE,
        file_name_u16string, file_size_u16string);
  } else {
    return l10n_util::GetNSStringF(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_BODY,
                                   file_name_u16string);
  }
}

}  // namespace

@interface SaveToDriveCoordinator () <AccountPickerCoordinatorDelegate>

@end

@implementation SaveToDriveCoordinator {
  NSString* _fileName;
  int64_t _fileSize;
  AccountPickerCoordinator* _accountPickerCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  fileName:(NSString*)fileName
                                  fileSize:(int64_t)fileSize {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _fileName = fileName;
    _fileSize = fileSize;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  AccountPickerConfiguration* accountPickerConfiguration =
      [[AccountPickerConfiguration alloc] init];
  accountPickerConfiguration.titleText =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_TITLE);
  accountPickerConfiguration.bodyText =
      GetAccountPickerBodyText(_fileName, _fileSize);
  accountPickerConfiguration.submitButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_SUBMIT);

  _accountPickerCoordinator = [[AccountPickerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   configuration:accountPickerConfiguration];
  _accountPickerCoordinator.delegate = self;
  [_accountPickerCoordinator start];
}

- (void)stop {
  [_accountPickerCoordinator stop];
  _accountPickerCoordinator = nil;
}

#pragma mark - AccountPickerCoordinatorDelegate

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
    openAddAccountWithCompletion:(void (^)(id<SystemIdentity>))completion {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* addAccountCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_SAVE_TO_DRIVE_IOS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 if (completion) {
                   completion(completionInfo.identity);
                 }
               }];
  [applicationCommandsHandler
              showSignin:addAccountCommand
      baseViewController:accountPickerCoordinator.viewController];
}

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
               didSelectIdentity:(id<SystemIdentity>)identity
                    askEveryTime:(BOOL)askEveryTime {
  // TODO(crbug.com/1495353): Start download and save file to Drive.
  [_accountPickerCoordinator startValidationSpinner];
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorCancel:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorAllIdentityRemoved:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorDidStop:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  _accountPickerCoordinator = nil;
  id<SaveToDriveCommands> saveToDriveCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToDriveCommands);
  [saveToDriveCommandsHandler hideSaveToDrive];
}

@end
