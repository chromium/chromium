// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/passwords/password_infobar_modal_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface PasswordInfobarModalOverlayMediator ()
// The save password modal config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation PasswordInfobarModalOverlayMediator {
  raw_ptr<IOSChromeSavePasswordInfoBarDelegate> delegate_;
  InfobarType infobarType_;
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

- (void)setConsumer:(id<InfobarPasswordModalConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  if (!_consumer || !self.config) {
    return;
  }

  delegate_ = static_cast<IOSChromeSavePasswordInfoBarDelegate*>(
      self.config->delegate());
  infobarType_ = self.config->infobar_type();

  [_consumer setUsername:delegate_->GetUserNameText()];
  NSString* password = delegate_->GetPasswordText();
  [_consumer setMaskedPassword:[@"" stringByPaddingToLength:password.length
                                                 withString:@"•"
                                            startingAtIndex:0]];
  [_consumer setUnmaskedPassword:password];
  std::optional<std::string> account_string =
      delegate_->GetAccountToStorePassword();
  NSString* details_text =
      account_string
          ? l10n_util::GetNSStringF(
                IDS_SAVE_PASSWORD_FOOTER_DISPLAYING_USER_EMAIL,
                base::UTF8ToUTF16(*account_string))
          : l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORD_FOOTER_NOT_SYNCING);
  [_consumer setDetailsTextMessage:details_text];

  NSString* save_button_text = base::SysUTF16ToNSString(
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  [_consumer setSaveButtonText:save_button_text];

  NSString* cancel_button_text = base::SysUTF16ToNSString(
      delegate_->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  [_consumer setCancelButtonText:cancel_button_text];

  [_consumer setURL:delegate_->GetURLHostText()];
  [_consumer setCurrentCredentialsSaved:delegate_->IsCurrentPasswordSaved()];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarPasswordModalDelegate

- (void)updateCredentialsWithUsername:(NSString*)username
                             password:(NSString*)password {
  // Receiving this delegate callback when the request is null means that the
  // update credentials button was tapped after the request was cancelled, but
  // before the modal UI has finished being dismissed.
  if (!self.request) {
    return;
  }

  delegate_->UpdateCredentials(username, password);
  delegate_->Accept();

  [self dismissInfobarModal:nil];
}

- (void)neverSaveCredentialsForCurrentSite {
  // Receiving this delegate callback when the request is null means that the
  // never save credentials button was tapped after the request was cancelled,
  // but before the modal UI has finished being dismissed.
  if (!self.request) {
    return;
  }

  delegate_->Cancel();
  [self dismissInfobarModal:nil];
}

- (void)presentPasswordSettings {
  // Receiving this delegate callback when the request is null means that the
  // present passwords settings button was tapped after the request was
  // cancelled, but before the modal UI has finished being dismissed.
  if (!self.request) {
    return;
  }

  [self dismissInfobarModal:nil];

  id<SettingsCommands> settings_command_handler =
      HandlerForProtocol(delegate_->GetDispatcher(), SettingsCommands);
  [settings_command_handler showSavedPasswordsSettingsFromViewController:nil
                                                        showCancelButton:YES];

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);
  base::RecordAction(
      base::UserMetricsAction("MobilePasswordInfobarModalOpenPasswordManager"));
}

@end
