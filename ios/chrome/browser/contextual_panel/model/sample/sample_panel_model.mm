// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/sample/sample_panel_model.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/web/public/web_state.h"

SamplePanelModel::SamplePanelModel() {}

SamplePanelModel::~SamplePanelModel() {}

void SamplePanelModel::FetchConfigurationForWebState(
    web::WebState* web_state,
    FetchConfigurationForWebStateCallback callback) {
  ContextualPanelItemConfiguration item_configuration;
  item_configuration.entrypoint_image_name = "book.pages";
  item_configuration.image_type =
      ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol;
  item_configuration.relevance =
      ContextualPanelItemConfiguration::high_relevance;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(item_configuration)));
}
