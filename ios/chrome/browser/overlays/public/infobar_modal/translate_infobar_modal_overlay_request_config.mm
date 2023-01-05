// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(TranslateModalRequestConfig);

TranslateModalRequestConfig::TranslateModalRequestConfig(InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  translate::TranslateInfoBarDelegate* delegate =
      infobar_->delegate()->AsTranslateInfoBarDelegate();
  DCHECK(delegate);

  current_step_ = delegate->translate_step();
  source_language_name_ = delegate->source_language_name();
  initial_source_language_name_ = delegate->initial_source_language_name();
  target_language_name_ = delegate->target_language_name();
  unknown_language_name_ = delegate->unknown_language_name();
  for (size_t i = 0; i < delegate->num_languages(); ++i) {
    language_names_.push_back(delegate->language_name_at((int(i))));
  }
  is_always_translate_enabled_ = delegate->ShouldAlwaysTranslate();
  is_translatable_language_ = delegate->IsTranslatableLanguageByPrefs();
  is_site_on_never_prompt_list_ = delegate->IsSiteOnNeverPromptList();
}

TranslateModalRequestConfig::~TranslateModalRequestConfig() = default;

void TranslateModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

}  // namespace translate_infobar_overlays
