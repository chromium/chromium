// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TRANSLATE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TRANSLATE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "components/translate/core/browser/translate_step.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace translate_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a TranslateInfoBarDelegate.
class TranslateBannerRequestConfig
    : public OverlayRequestConfig<TranslateBannerRequestConfig> {
 public:
  ~TranslateBannerRequestConfig() override;

  // The source language name.
  const std::u16string& source_language() const { return source_language_; }
  // The target language name.
  const std::u16string& target_language() const { return target_language_; }
  // The current TranslateStep Translate is in.
  translate::TranslateStep translate_step() const { return translate_step_; }

 private:
  OVERLAY_USER_DATA_SETUP(TranslateBannerRequestConfig);
  explicit TranslateBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* const infobar_;
  // Configuration data extracted from `infobar_`'s translate delegate.
  std::u16string source_language_;
  std::u16string target_language_;
  translate::TranslateStep translate_step_;
};

}  // namespace translate_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TRANSLATE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
