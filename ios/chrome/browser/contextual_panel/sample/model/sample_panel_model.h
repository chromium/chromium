// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_H_

#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"

// An example ContextualPanelModel.
class SamplePanelModel : public ContextualPanelModel, public KeyedService {
 public:
  SamplePanelModel();

  SamplePanelModel(const SamplePanelModel&) = delete;
  SamplePanelModel& operator=(const SamplePanelModel&) = delete;

  ~SamplePanelModel() override;

  // ContextualPanelModel:
  void FetchConfigurationForWebState(
      web::WebState* web_state,
      FetchConfigurationForWebStateCallback callback) override;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_MODEL_H_
