// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_H_

#include "base/functional/callback.h"

struct ContextualPanelItemConfiguration;
namespace web {
class WebState;
}

// Abstract class representing a model for a Contextual Panel item.
class ContextualPanelModel {
 public:
  using FetchConfigurationForWebStateCallback = base::OnceCallback<void(
      std::unique_ptr<ContextualPanelItemConfiguration>)>;
  // Asks the model to do any work necessary to fetch the Contextual Panel
  // data for the given `web_state`. Once it has fetched the data, it should
  // call the `callback` with it, or nullopt if it has no data for this
  // web_state.
  virtual void FetchConfigurationForWebState(
      web::WebState* web_state,
      FetchConfigurationForWebStateCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_H_
