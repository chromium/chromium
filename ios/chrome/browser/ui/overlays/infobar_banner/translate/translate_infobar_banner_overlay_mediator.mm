// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;

@interface TranslateInfobarBannerOverlayMediator ()
// The translate banner config from the request.
@property(nonatomic, readonly) TranslateBannerRequestConfig* config;
@end

@implementation TranslateInfobarBannerOverlayMediator

#pragma mark - Accessors

- (TranslateBannerRequestConfig*)config {
  return self.request ? self.request->GetConfig<TranslateBannerRequestConfig>()
                      : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return TranslateBannerRequestConfig::RequestSupport();
}

@end

@implementation TranslateInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  TranslateBannerRequestConfig* config = self.config;
  if (!self.consumer || !config)
    return;

  [self.consumer setBannerAccessibilityLabel:[self bannerTitleText]];
  [self.consumer setButtonText:[self infobarButtonText]];

  UIImage* iconImage = CustomSymbolTemplateWithPointSize(
      kTranslateSymbol, kInfobarSymbolPointSize);
  [self.consumer setIconImage:iconImage];
  [self.consumer setPresentsModal:YES];
  [self.consumer setTitleText:[self bannerTitleText]];
  [self.consumer setSubtitleText:[self bannerSubtitleText]];
}

// Returns the title text of the banner depending on the
// `self.config.translate_step()`.
- (NSString*)bannerTitleText {
  switch (self.config->translate_step()) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_AFTER_TRANSLATE_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_ON_ERROR_BANNER_TITLE);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:

      NOTREACHED() << "Should not be presenting Banner in this TranslateStep";
      return nil;
  }
}

// Returns the subtitle text of the banner.
- (NSString*)bannerSubtitleText {
  // Formatted as "[source] to [target]".
  return l10n_util::GetNSStringF(
      IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_BANNER_SUBTITLE,
      self.config->source_language(), self.config->target_language());
}

// Returns the text of the banner and modal action button depending on the
// `self.config.translate_step()`.
- (NSString*)infobarButtonText {
  switch (self.config->translate_step()) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      return l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_UNDO_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      return l10n_util::GetNSString(
          IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_TRY_AGAIN_ACTION);
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:

      NOTREACHED() << "Translate infobar should not be presenting anything in "
                      "this state.";
      return nil;
  }
}

@end
