// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_

#include <string>

// Data to configure a Contextual Panel item. Individual features can subclass
// this to add their own data.
struct ContextualPanelItemConfiguration {
  // A constant defined to always be a high relevance amount.
  static const int high_relevance;

  // A constant defined to always be a low relevance amount.
  static const int low_relevance;

  ContextualPanelItemConfiguration();
  ~ContextualPanelItemConfiguration();
  ContextualPanelItemConfiguration(
      const ContextualPanelItemConfiguration& other);
  ContextualPanelItemConfiguration(ContextualPanelItemConfiguration&& other);
  ContextualPanelItemConfiguration& operator=(
      const ContextualPanelItemConfiguration& other);

  // The different supported image types.
  enum class EntrypointImageType {
    // The image name is a UIImage to be loaded in.
    Image,
    // The image name is an SFSymbol to display.
    SFSymbol,
  };

  // The string the UI can show the user if this item is the primary item in the
  // contextual panel.
  std::string entrypoint_message;

  // The string the entrypoint's badge button should have for accessibility.
  std::string accessibility_label;

  // The name of the image the UI can show the user if this item is the primary
  // item in the contextual panel.
  std::string entrypoint_image_name;

  // The type of entrypoint image. This is used by the UI to decide how to
  // interpret `entrypoint_image_name`.
  EntrypointImageType image_type;

  // A value from 0 to 100 representing the relevance of this item to the user.
  // Individual panel models can use one of the provided constants or set their
  // own value.
  int relevance;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
