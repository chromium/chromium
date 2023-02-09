// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_data_host_manager.h"

#include "content/browser/attribution_reporting/attribution_manager.h"

namespace content {

// static
AttributionDataHostManager* AttributionDataHostManager::FromBrowserContext(
    BrowserContext* browser_context) {
  auto* attribution_manager =
      AttributionManager::FromBrowserContext(browser_context);
  if (!attribution_manager) {
    return nullptr;
  }
  return attribution_manager->GetDataHostManager();
}

AttributionDataHostManager::AttributionDataHostManager() = default;

AttributionDataHostManager::~AttributionDataHostManager() = default;

}  // namespace content
