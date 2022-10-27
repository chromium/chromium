// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

TranslateInfobarBannerInteractionHandler::
    TranslateInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          TranslateBannerRequestConfig::RequestSupport()) {}

TranslateInfobarBannerInteractionHandler::
    ~TranslateInfobarBannerInteractionHandler() = default;

void TranslateInfobarBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate = GetInfobarDelegate(infobar);
  translate::TranslateStep step = delegate->translate_step();
  switch (step) {
    case translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE:
      if (delegate->ShouldAutoAlwaysTranslate())
        delegate->ToggleAlwaysTranslate();
      delegate->Translate();
      break;
    case translate::TranslateStep::TRANSLATE_STEP_AFTER_TRANSLATE:
      delegate->RevertWithoutClosingInfobar();
      infobar->set_accepted(false);
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATE_ERROR:
      // On error, action is to retry.
      delegate->Translate();
      break;
    case translate::TranslateStep::TRANSLATE_STEP_TRANSLATING:
    case translate::TranslateStep::TRANSLATE_STEP_NEVER_TRANSLATE:
      NOTREACHED() << "Should not be presenting Banner in this TranslateStep";
  }
}

#pragma mark - Private

translate::TranslateInfoBarDelegate*
TranslateInfobarBannerInteractionHandler::GetInfobarDelegate(
    InfoBarIOS* infobar) {
  translate::TranslateInfoBarDelegate* delegate =
      infobar->delegate()->AsTranslateInfoBarDelegate();
  DCHECK(delegate);
  return delegate;
}
