// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_

#include <string>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"

enum class ContextualPanelItemType;

// Data to configure a Contextual Panel item. Individual features can subclass
// this to add their own data.
struct ContextualPanelItemConfiguration {
  // A constant defined to always be a high relevance amount.
  static const int high_relevance;

  // A constant defined to always be a low relevance amount.
  static const int low_relevance;

  explicit ContextualPanelItemConfiguration(ContextualPanelItemType item_type);
  ~ContextualPanelItemConfiguration();
  ContextualPanelItemConfiguration(
      const ContextualPanelItemConfiguration& other) = delete;
  ContextualPanelItemConfiguration& operator=(
      const ContextualPanelItemConfiguration& other) = delete;

  // Helper for checking if a given config can be used to show different
  // entrypoint loud moment states.
  bool CanShowLargeEntrypoint();
  bool CanShowEntrypointIPH();

  // The different supported image types.
  enum class EntrypointImageType {
    // The image name is a UIImage to be loaded in.
    Image,
    // The image name is an SFSymbol to display.
    SFSymbol,
  };

  // The item type of this item.
  const ContextualPanelItemType item_type;

  // The string the UI can show the user if this item is the primary item in the
  // contextual panel. If none is provided, no large entrypoint can be shown.
  std::string entrypoint_message;

  // Required. The string the entrypoint's badge button should have for
  // accessibility.
  std::string accessibility_label;

  // Required. The name of the image the UI can show the user if this item is
  // the primary item in the contextual panel.
  std::string entrypoint_image_name;

  // Required. The type of entrypoint image. This is used by the UI to decide
  // how to interpret `entrypoint_image_name`.
  EntrypointImageType image_type;

  // Required. A value from 0 to 100 representing the relevance of this item to
  // the user. Individual panel models can use one of the provided constants or
  // set their own value.
  int relevance;

  // ** Entrypoint IPH (rich IPH type) related config keys. **

  // Optional. The FET feature controlling the impressions for the item's
  // entrypoint rich IPH (in-product help). The entrypoint will try to show the
  // IPH, but if the FET config decides it shouldn't be shown, the large
  // entrypoint will be shown. If nothing is set here, the IPH will never be
  // shown.
  raw_ptr<const base::Feature> iph_feature;

  // Optional (required if `iph_feature` is non-nil). The FET event name the
  // entrypoint should use when firing an event because the entrypoint was used
  // with the current infoblock model (the `used` key of the FET config). Any
  // number of events can be used in conjunction with the FET config to control
  // the IPH impressions.
  std::string iph_entrypoint_used_event_name;

  // Optional (required if `iph_feature` is non-nil). The FET event name the
  // entrypoint should use when firing an event because the in-product help was
  // explicitly dismissed by the user. Any number of events can be used in
  // conjunction with the FET config to control the IPH impressions.
  std::string iph_entrypoint_explicitly_dismissed;

  // Optional (required if `iph_feature` is non-nil). The title of the rich IPH
  // bubble.
  std::string iph_title;

  // Optional (required if `iph_feature` is non-nil). The main body text of the
  // rich IPH bubble.
  std::string iph_text;

  // Optional (required if `iph_feature` is non-nil). The name of the image used
  // in the rich IPH bubble.
  std::string iph_image_name;

  // ** End of entrypoint IPH (rich IPH type) related config keys. **

  base::WeakPtrFactory<ContextualPanelItemConfiguration> weak_ptr_factory{this};
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
