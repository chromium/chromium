// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include <string>
#include <vector>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

#include "components/translate/core/browser/translate_step.h"

class InfoBarIOS;

namespace translate_infobar_overlays {

// Configuration object for OverlayRequests for the modal UI for an infobar with
// a TranslateInfoBarDelegate.
class TranslateModalRequestConfig
    : public OverlayRequestConfig<TranslateModalRequestConfig> {
 public:
  ~TranslateModalRequestConfig() override;

  // The current TranslateStep Translate is in.
  translate::TranslateStep current_step() const { return current_step_; }
  // The source language name.
  std::u16string source_language_name() const { return source_language_name_; }
  // The initial source language name.
  std::u16string initial_source_language_name() const {
    return initial_source_language_name_;
  }
  // A special language name the is used when the source language could not be
  // detected.
  std::u16string unknown_language_name() const {
    return unknown_language_name_;
  }
  // The target language name.
  std::u16string target_language_name() const { return target_language_name_; }
  // A list of names of all possible languages.
  std::vector<std::u16string> language_names() const { return language_names_; }
  // Whether the current Translate pref is set to always translate for the
  // source language.
  bool is_always_translate_enabled() const {
    return is_always_translate_enabled_;
  }
  // Whether the current Translate pref is set to never translate for the source
  // language.
  bool is_translatable_language() const { return is_translatable_language_; }
  // Whether the current Translate pref is set to never translate for current
  // page.
  bool is_site_on_never_prompt_list() const {
    return is_site_on_never_prompt_list_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(TranslateModalRequestConfig);
  explicit TranslateModalRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s translate delegate.
  translate::TranslateStep current_step_;
  std::u16string source_language_name_;
  std::u16string initial_source_language_name_;
  std::u16string unknown_language_name_;
  std::u16string target_language_name_;
  std::vector<std::u16string> language_names_;
  bool is_always_translate_enabled_ = false;
  bool is_translatable_language_ = false;
  bool is_site_on_never_prompt_list_ = false;
};

}  // namespace translate_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
