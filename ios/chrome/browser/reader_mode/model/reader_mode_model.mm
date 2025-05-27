// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Performs the custom Reader mode contextual chip entrypoint for `web_state`.
void PerformReaderModeCustomEntrypointAction(
    base::WeakPtr<web::WebState> web_state) {
  if (!web_state || web_state->IsBeingDestroyed()) {
    return;
  }
  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state.get());
  if (!reader_mode_tab_helper) {
    return;
  }
  if (reader_mode_tab_helper->IsReaderModeWebStateAvailable()) {
    // TODO(crbug.com/409941529): Show options instead when the UI is ready.
    reader_mode_tab_helper->SetActive(false);
  } else {
    reader_mode_tab_helper->SetActive(true);
  }
}

// Calls `callback` with the appropriate ContextualPanelItemConfiguration object
// depend on the value of `current_page_supports_reader_mode`.
void HandleCurrentPageSupportsReaderModeResult(
    base::WeakPtr<web::WebState> web_state,
    ReaderModeModel::FetchConfigurationForWebStateCallback callback,
    std::optional<bool> current_page_supports_reader_mode) {
  std::unique_ptr<ContextualPanelItemConfiguration> configuration;
  if (web_state && current_page_supports_reader_mode &&
      *current_page_supports_reader_mode) {
    configuration = std::make_unique<ContextualPanelItemConfiguration>(
        ContextualPanelItemType::ReaderModeItem);
    configuration->entrypoint_message = l10n_util::GetStringUTF8(
        IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
    configuration->entrypoint_message_large_entrypoint_always_shown = true;
    configuration->accessibility_label = l10n_util::GetStringUTF8(
        IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
    configuration->entrypoint_image_name =
        base::SysNSStringToUTF8(GetReaderModeSymbolName());
    configuration->image_type =
        ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
    configuration->relevance = ContextualPanelItemConfiguration::low_relevance;
    configuration->entrypoint_custom_action = base::BindRepeating(
        &PerformReaderModeCustomEntrypointAction, web_state->GetWeakPtr());
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(configuration)));
}

}  // namespace

ReaderModeModel::ReaderModeModel() = default;

ReaderModeModel::~ReaderModeModel() = default;

void ReaderModeModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (!reader_mode_tab_helper) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(nullptr)));
    return;
  }
  reader_mode_tab_helper->FetchLastCommittedUrlEligibilityResult(
      base::BindOnce(&HandleCurrentPageSupportsReaderModeResult,
                     web_state->GetWeakPtr(), std::move(callback)));
}
