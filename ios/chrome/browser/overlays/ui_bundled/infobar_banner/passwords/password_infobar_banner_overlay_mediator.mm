// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "build/build_config.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface PasswordInfobarBannerOverlayMediator ()

// The password banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation PasswordInfobarBannerOverlayMediator {
  InfobarType infobarType_;
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config or `nullptr` if there is no
// config.
- (IOSChromeSavePasswordInfoBarDelegate*)passwordDelegate {
  if (!self.config) {
    return nullptr;
  }

  return static_cast<IOSChromeSavePasswordInfoBarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  default_browser::NotifyPasswordSavedOrUpdated(self.engagementTracker);

  // This can happen if the user quickly navigates to another website while the
  // banner is still appearing, where the infobar owning the delegate is deleted
  // before handling the button action.
  if (!self.passwordDelegate) {
    return;
  }

  self.passwordDelegate->Accept();
  [self dismissOverlay];
}

#pragma mark - InfobarBannerOverlayMediator

- (void)finishDismissal {
  if (self.passwordDelegate) {
    // If the infobar owning the delegate isn't yet deleted, report the infobar
    // as gone right now. The infobar outlives the banner UI when the state of
    // the page hasn't changed after dimissing the banner.
    //
    // Not having a delegate at this moment happens when navigating away from
    // the page on which the banner is displayed, where the infobar delegate is
    // deleted before the dismiss callback is called.
    self.passwordDelegate->InfobarGone();
  }

  [super finishDismissal];
}

#pragma mark - Private

// Returns the icon image.
- (UIImage*)iconImage {
  UIImage* image =
      CustomSymbolWithPointSize(kPasswordSymbol, kInfobarSymbolPointSize);
#if !BUILDFLAG(IS_IOS_MACCATALYST)
  image = MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kMulticolorPasswordSymbol, kInfobarSymbolPointSize));
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
  return image;
}

@end

@implementation PasswordInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  if (!self.consumer || !self.config) {
    return;
  }

  IOSChromeSavePasswordInfoBarDelegate* delegate = self.passwordDelegate;

  delegate->InfobarPresenting(YES);
  infobarType_ = self.config->infobar_type();

  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());

  std::optional<std::string> account_string =
      delegate->GetAccountToStorePassword();
  NSString* subtitle =
      account_string ? l10n_util::GetNSStringF(
                           IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                           base::UTF8ToUTF16(*account_string))
                     : l10n_util::GetNSString(
                           IDS_IOS_PASSWORD_MANAGER_LOCAL_SAVE_SUBTITLE);

  NSString* button_text = base::SysUTF16ToNSString(
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));

  [self.consumer setButtonText:button_text];
  [self.consumer setIconImage:[self iconImage]];
  [self.consumer setIgnoreIconColorWithTint:NO];
  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
}

@end
