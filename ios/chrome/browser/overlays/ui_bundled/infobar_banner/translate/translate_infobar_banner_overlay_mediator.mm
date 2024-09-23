// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"

#import <ostream>

#import "base/notreached.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface TranslateInfobarBannerOverlayMediator ()

// The translate banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation TranslateInfobarBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (translate::TranslateInfoBarDelegate*)translateDelegate {
  return static_cast<translate::TranslateInfoBarDelegate*>(
      self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // This can happen if the user quickly navigates to another website while the
  // banner is still appearing, causing the banner to be triggered before being
  // removed.
  if (!self.translateDelegate) {
    return;
  }

  translate::TranslateInfoBarDelegate* delegate = self.translateDelegate;
  translate::TranslateStep step = delegate->translate_step();
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE: {
      if (delegate->ShouldAutoAlwaysTranslate()) {
        delegate->ToggleAlwaysTranslate();
      }
      delegate->Translate();
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE: {
      delegate->RevertWithoutClosingInfobar();
      InfoBarIOS* infobar = GetOverlayRequestInfobar(self.request);
      infobar->set_accepted(false);
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR: {
      // On error, action is to retry.
      delegate->Translate();
      break;
    }
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      break;
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED() << "Should not be presenting Banner in this TranslateStep";
  }
  [self dismissOverlay];
}

@end

@implementation TranslateInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  [self.consumer
      setBannerAccessibilityLabel:
          [NSString stringWithFormat:@"%@ - %@", [self bannerTitleText],
                                     [self bannerSubtitleText]]];
  [self.consumer setButtonText:[self infobarButtonText]];

  UIImage* iconImage = CustomSymbolTemplateWithPointSize(
      kTranslateSymbol, kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];
  [self.consumer setPresentsModal:YES];
  [self.consumer setTitleText:[self bannerTitleText]];
  [self.consumer setSubtitleText:[self bannerSubtitleText]];
}

#pragma mark - Private

// Returns the title text of the banner depending on the
// `self.config.translate_step()`.
- (NSString*)bannerTitleText {
  switch (self.translateDelegate->translate_step()) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_ON_ERROR_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED() << "Should not be presenting Banner in this TranslateStep";
  }
}

// Returns the subtitle text of the banner.
- (NSString*)bannerSubtitleText {
  // Formatted as "[source] to [target]".
  return l10n_util::GetNSStringF(
      IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_BANNER_SUBTITLE,
      self.translateDelegate->source_language_name(),
      self.translateDelegate->target_language_name());
}

// Returns the text of the banner and modal action button depending on the
// `self.config.translate_step()`.
- (NSString*)infobarButtonText {
  switch (self.translateDelegate->translate_step()) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
      return l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_TRY_AGAIN_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED_IN_MIGRATION()
          << "Translate infobar should not be presenting anything in "
             "this state.";
      return nil;
  }
}

@end
