// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

enum class ContextualPanelItemType;
class ContextualPanelModel;

// Service for providing a list of available `ContextualPanelModel`s.
class ContextualPanelModelService : public KeyedService {
 public:
  ContextualPanelModelService(
      std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models);

  ContextualPanelModelService(const ContextualPanelModelService&) = delete;
  ContextualPanelModelService& operator=(const ContextualPanelModelService&) =
      delete;

  ~ContextualPanelModelService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Returns a map of ContextualPanelModels to use, each keyed by the
  // appropriate item type.
  const std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>>&
  models();

 private:
  std::map<ContextualPanelItemType, raw_ptr<ContextualPanelModel>> models_;
};

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_MODEL_CONTEXTUAL_PANEL_MODEL_SERVICE_H_
