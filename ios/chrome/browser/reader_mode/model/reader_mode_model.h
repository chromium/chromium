// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_H_

#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"

// A ContextualPanelModel for Reader Mode.
class ReaderModeModel : public ContextualPanelModel, public KeyedService {
 public:
  ReaderModeModel();

  ReaderModeModel(const ReaderModeModel&) = delete;
  ReaderModeModel& operator=(const ReaderModeModel&) = delete;

  ~ReaderModeModel() override;

  // ContextualPanelModel:
  void FetchConfigurationForWebState(
      web::WebState* web_state,
      FetchConfigurationForWebStateCallback callback) override;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_H_
