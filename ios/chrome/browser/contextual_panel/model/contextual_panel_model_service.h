// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class ContextualPanelModel;

// Service for providing a list of available `ContextualPanelModel`s.
class ContextualPanelModelService : public KeyedService {
 public:
  ContextualPanelModelService();

  ContextualPanelModelService(const ContextualPanelModelService&) = delete;
  ContextualPanelModelService& operator=(const ContextualPanelModelService&) =
      delete;

  ~ContextualPanelModelService() override;

  // Returns a list of ContextualPanelModels to use.
  std::vector<base::WeakPtr<ContextualPanelModel>> models();
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_
