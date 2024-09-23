// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_ITEM_CONFIGURATION_H_

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"

// An example item configuration for the sample model showing how to add
// additional data to the item configuration.
struct SamplePanelItemConfiguration : public ContextualPanelItemConfiguration {
  SamplePanelItemConfiguration()
      : ContextualPanelItemConfiguration(
            ContextualPanelItemType::SamplePanelItem) {}

  std::string sample_name;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_SAMPLE_MODEL_SAMPLE_PANEL_ITEM_CONFIGURATION_H_
