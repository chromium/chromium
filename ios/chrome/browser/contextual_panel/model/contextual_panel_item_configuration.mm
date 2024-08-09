// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"

const int ContextualPanelItemConfiguration::high_relevance = 80;

const int ContextualPanelItemConfiguration::low_relevance = 20;

ContextualPanelItemConfiguration::ContextualPanelItemConfiguration(
    ContextualPanelItemType item_type)
    : item_type(item_type) {}

ContextualPanelItemConfiguration::~ContextualPanelItemConfiguration() = default;

bool ContextualPanelItemConfiguration::CanShowLargeEntrypoint() {
  return !entrypoint_message.empty() && relevance >= high_relevance;
}

bool ContextualPanelItemConfiguration::CanShowEntrypointIPH() {
  return iph_feature && !iph_text.empty() && !iph_title.empty() &&
         !iph_image_name.empty() && !iph_entrypoint_used_event_name.empty() &&
         !iph_entrypoint_explicitly_dismissed.empty() &&
         relevance >= high_relevance;
}
