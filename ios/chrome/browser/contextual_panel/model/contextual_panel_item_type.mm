// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"

#import <string>

std::string StringForItemType(ContextualPanelItemType item_type) {
  switch (item_type) {
    case ContextualPanelItemType::SamplePanelItem:
      return "SamplePanelItem";
    case ContextualPanelItemType::PriceInsightsItem:
      return "PriceInsightsItem";
  }
}
