// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_model.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

ReaderModeModel::ReaderModeModel() = default;

ReaderModeModel::~ReaderModeModel() = default;

void ReaderModeModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  ReaderModeTabHelper* reader_mode_tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (!reader_mode_tab_helper ||
      !reader_mode_tab_helper->CurrentPageSupportsReaderMode()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(nullptr)));
    return;
  }

  std::unique_ptr<ContextualPanelItemConfiguration> item_configuration =
      std::make_unique<ContextualPanelItemConfiguration>(
          ContextualPanelItemType::ReaderModeItem);
  item_configuration->entrypoint_message = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  item_configuration->accessibility_label = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_READER_MODE_MODEL_ENTRYPOINT_MESSAGE);
  item_configuration->entrypoint_image_name =
      base::SysNSStringToUTF8(kReaderModeSymbol);
  item_configuration->image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  item_configuration->relevance =
      ContextualPanelItemConfiguration::high_relevance;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(item_configuration)));
}
