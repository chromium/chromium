// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_

#include <string>

// Data to configure a Contextual Panel item. Individual features can subclass
// this to add their own data.
struct ContextualPanelItemConfiguration {
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

  // The name of the image the UI can show the user if this item is the primary
  // item in the contextual panel.
  std::string entrypoint_image_name;

  // The type of entrypoint image. This is used by the UI to decide how to
  // interpret `entrypoint_image_name`.
  EntrypointImageType image_type;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_ITEM_CONFIGURATION_H_
