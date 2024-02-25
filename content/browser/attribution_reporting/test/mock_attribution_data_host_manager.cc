// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/mock_attribution_data_host_manager.h"

namespace content {

MockAttributionDataHostManager::MockAttributionDataHostManager() = default;

MockAttributionDataHostManager::~MockAttributionDataHostManager() = default;

base::WeakPtr<AttributionDataHostManager>
MockAttributionDataHostManager::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
