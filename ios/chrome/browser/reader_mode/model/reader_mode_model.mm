// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

#import "ios/chrome/browser/reader_mode/model/reader_mode_panel_item_configuration.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

namespace {

// Calls `callback` with the appropriate ContextualPanelItemConfiguration object
// depend on the value of `current_page_supports_reader_mode`.
void HandleCurrentPageIsDistillableResult(
    ProfileIOS* profile,
    base::WeakPtr<web::WebState> web_state,
    ReaderModeModel::FetchConfigurationForWebStateCallback callback,
    std::optional<bool> current_page_supports_reader_mode) {
  std::unique_ptr<ContextualPanelItemConfiguration> configuration;
  if (web_state && current_page_supports_reader_mode &&
      *current_page_supports_reader_mode) {
    configuration = std::make_unique<ReaderModePanelItemConfiguration>(
        profile, web_state.get());
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(configuration)));
}

}  // namespace

ReaderModeModel::ReaderModeModel(ProfileIOS* profile) : profile_(profile) {}

ReaderModeModel::~ReaderModeModel() = default;

void ReaderModeModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  PrefService* prefs = profile_->GetPrefs();
  if (!reader_mode_tab_helper ||
      !prefs->GetBoolean(prefs::kIosReaderModeShowAvailability)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(nullptr)));
    return;
  }

  reader_mode_tab_helper->FetchLastCommittedUrlDistillabilityResult(
      base::BindOnce(&HandleCurrentPageIsDistillableResult, profile_,
                     web_state->GetWeakPtr(), std::move(callback)));
}
