// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model_service.h"

#import "ios/chrome/browser/contextual_panel/model/contextual_panel_model.h"

ContextualPanelModelService::ContextualPanelModelService() {}

ContextualPanelModelService::~ContextualPanelModelService() {}

std::vector<base::WeakPtr<ContextualPanelModel>>
ContextualPanelModelService::models() {
  return {};
}
