// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_TYPE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_TYPE_H_

#include <string>

// Enum to represent the existing Contextual Panel item types.
// LINT.IfChange(ContextualPanelItemType)
enum class ContextualPanelItemType {
  SamplePanelItem = 0,
  PriceInsightsItem = 1,
  kMaxValue = PriceInsightsItem,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:ContextualPanelItemType)

// Converts the given item type to a string representation.
std::string StringForItemType(ContextualPanelItemType item_type);

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_TYPE_H_
