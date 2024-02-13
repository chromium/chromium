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
  // Asks the model to do any work necessary to fetch the Contextual Panel
  // data for the given `web_state`. Once it has fetched the data, it should
  // call the `callback` with it.
  virtual void FetchConfigurationForWebState(
      web::WebState* web_state,
      base::OnceCallback<void(ContextualPanelItemConfiguration)> callback);
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_H_
