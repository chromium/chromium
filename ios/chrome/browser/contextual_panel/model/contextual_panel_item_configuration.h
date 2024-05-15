// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_

#include <string>

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

  base::WeakPtrFactory<ContextualPanelItemConfiguration> weak_ptr_factory{this};
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
