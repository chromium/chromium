// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/overlays/public/overlay_response_info.h"

namespace translate_infobar_modal_responses {

// Response info used to create dispatched OverlayResponses that update the
// state of the source and target languages in Translate.
class UpdateLanguageInfo : public OverlayResponseInfo<UpdateLanguageInfo> {
 public:
  ~UpdateLanguageInfo() override;

  // The new target language index.
  int source_language_index() const { return source_language_index_; }
  // The new source language index.
  int target_language_index() const { return target_language_index_; }

 private:
  OVERLAY_USER_DATA_SETUP(UpdateLanguageInfo);
  UpdateLanguageInfo(int source_language_index, int target_language_index);

  int source_language_index_;
  int target_language_index_;
};

// Response info used to create dispatched OverlayResponses that notify the
// translate infobar to revert the translation.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(RevertTranslation);

// Response info used to create dispatched OverlayResponses that notify the
// translate infobar to toggle the always translate preference.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(ToggleAlwaysTranslate);

// Response info used to create dispatched OverlayResponses that notify the
// translate infobar to toggle the never translate source language preference.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(ToggleNeverTranslateSourceLanguage);

// Response info used to create dispatched OverlayResponses that notify the
// translate infobar to toggle the never translate site preference.
DEFINE_STATELESS_OVERLAY_RESPONSE_INFO(ToggleNeverPromptSite);

}  // namespace translate_infobar_modal_responses

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_RESPONSES_H_
