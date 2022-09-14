// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_responses.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate_infobar_modal_responses {

#pragma mark - UpdateLanguageInfo

OVERLAY_USER_DATA_SETUP_IMPL(UpdateLanguageInfo);

UpdateLanguageInfo::UpdateLanguageInfo(int source_language_index,
                                       int target_language_index)
    : source_language_index_(source_language_index),
      target_language_index_(target_language_index) {}

UpdateLanguageInfo::~UpdateLanguageInfo() = default;

OVERLAY_USER_DATA_SETUP_IMPL(RevertTranslation);

OVERLAY_USER_DATA_SETUP_IMPL(ToggleAlwaysTranslate);

OVERLAY_USER_DATA_SETUP_IMPL(ToggleNeverTranslateSourceLanguage);

OVERLAY_USER_DATA_SETUP_IMPL(ToggleNeverPromptSite);

}  // translate_infobar_modal_responses
