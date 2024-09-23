// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_model.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/contextual_panel/sample/model/sample_panel_item_configuration.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

SamplePanelModel::SamplePanelModel() {}

SamplePanelModel::~SamplePanelModel() {}

void SamplePanelModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  // No data to show for the current webstate.
  if (IsUrlNtp(web_state->GetVisibleURL())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(nullptr)));
    return;
  }

  std::unique_ptr<SamplePanelItemConfiguration> item_configuration =
      std::make_unique<SamplePanelItemConfiguration>();

  // Happy path, there is data to show for the current webstate. This is
  // sample/test data, it can be anything.
  item_configuration->sample_name = "sample_config";
  item_configuration->accessibility_label = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_SAMPLE_MODEL_ENTRYPOINT_MESSAGE);
  item_configuration->entrypoint_message = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_SAMPLE_MODEL_ENTRYPOINT_MESSAGE);
  item_configuration->entrypoint_image_name = "chrome_product";
  item_configuration->iph_title = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_SAMPLE_MODEL_ENTRYPOINT_IPH_TITLE);
  item_configuration->iph_text = l10n_util::GetStringUTF8(
      IDS_IOS_CONTEXTUAL_PANEL_SAMPLE_MODEL_ENTRYPOINT_IPH_TEXT);
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  item_configuration->iph_image_name = "chrome_search_engine_choice_icon";
#else
  item_configuration->iph_image_name = "chromium_search_engine_choice_icon";
#endif
  item_configuration->iph_feature =
      &feature_engagement::kIPHiOSContextualPanelSampleModelFeature;
  item_configuration->iph_entrypoint_used_event_name =
      feature_engagement::events::kIOSContextualPanelSampleModelEntrypointUsed;
  item_configuration->iph_entrypoint_explicitly_dismissed =
      "ios_contextual_panel_sample_model_entrypoint_explicitly_dismissed";
  item_configuration->image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  item_configuration->relevance =
      ContextualPanelItemConfiguration::high_relevance;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(item_configuration)));
}
