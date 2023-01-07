// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/mock_sync_service_utils.h"

std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}
